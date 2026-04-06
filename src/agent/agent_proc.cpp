#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <sys/mman.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <zenoh.hxx>
#include <zenoh/api/closures.hxx>

#include "credentials.h"

enum class Event_State { NONE, BOOT_TLS, UPGRADE_TLS, TIMEOUT, SHUTDOWN };

struct Mem_File {
    int fd = -1;
    std::string path;
};

struct Agent_Ctx {
    std::string agent_id = "agent-01";
    Event_State last_event = Event_State::NONE;
    
    // Crypto State
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{nullptr, EVP_PKEY_free};
    std::map<std::string, Mem_File> ram_files;

    // Zenoh State
    std::optional<zenoh::Session> session;
    std::vector<std::variant<zenoh::Queryable<void>, zenoh::Subscriber<void>>> active_routes;

    std::mutex mtx;
    std::condition_variable cv;

    // Standard 8-hour session duration    
    std::chrono::seconds session_duration{28800};
    bool is_authenticated = false;

    Agent_Ctx() = default;

    ~Agent_Ctx() {
        for (auto& [name, file] : ram_files) {
            if (file.fd != -1) close(file.fd);
        }
    }
};

// --- Utilities ---

std::string write_to_ram(Agent_Ctx& ctx, const std::string& name, const std::string& data) {
    // If overwrite, close old FD
    if (ctx.ram_files.count(name)) close(ctx.ram_files[name].fd);

    int fd = memfd_create(name.c_str(), MFD_CLOEXEC);
    if (fd == -1) return "";
    
    if (write(fd, data.c_str(), data.size()) == -1) {
        close(fd);
        return "";
    }

    // Crucial: We do NOT close the FD here. Zenoh needs the path /proc/self/fd/N to remain open.
    ctx.ram_files[name] = {fd, "/proc/self/fd/" + std::to_string(fd)};
    return ctx.ram_files[name].path;
}

template <typename T, typename WriteFunc>
std::string to_pem_string(T* obj, WriteFunc write_func) {
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio || !write_func(bio.get(), obj)) return "";
    char* data;
    long len = BIO_get_mem_data(bio.get(), &data);
    return std::string(data, len);
}

// --- Crypto ---

void crypto_init_keys(Agent_Ctx& ctx) {
    ctx.pkey.reset(EVP_EC_gen("secp256k1"));
    std::string pem = to_pem_string(ctx.pkey.get(), [](BIO* b, EVP_PKEY* k) {
        return PEM_write_bio_PrivateKey(b, k, nullptr, nullptr, 0, nullptr, nullptr);
    });
    write_to_ram(ctx, "priv_key.pem", pem);
}

void crypto_inject_root_ca(Agent_Ctx& ctx, const std::string& pem_content) {
    if (pem_content.empty()) return;
    write_to_ram(ctx, "root_ca.pem", pem_content);
}

std::string crypto_generate_csr(Agent_Ctx& ctx) {
    std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> req(X509_REQ_new(), X509_REQ_free);
    X509_REQ_set_version(req.get(), 0);
    X509_NAME* name = X509_REQ_get_subject_name(req.get());
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)ctx.agent_id.c_str(), -1, -1, 0);
    X509_REQ_set_pubkey(req.get(), ctx.pkey.get());
    X509_REQ_sign(req.get(), ctx.pkey.get(), EVP_sha256());

    return to_pem_string(req.get(), PEM_write_bio_X509_REQ);
}

// --- Zenoh Config Factory ---

zenoh::Config build_zenoh_config(Agent_Ctx& ctx, bool use_mtls) {
    auto config = zenoh::Config::create_default();
    
    // Shared security configs
    config.insert_json5("transport/link/tls/enabled", "true");
    
    if (ctx.ram_files.count("root_ca.pem")) {
        config.insert_json5("transport/link/tls/root_ca_certificate", "\"" + ctx.ram_files["root_ca.pem"].path + "\"");
    }

    if (use_mtls) {
        config.insert_json5("transport/link/tls/certificate", "\"" + ctx.ram_files["cert.pem"].path + "\"");
        config.insert_json5("transport/link/tls/private_key", "\"" + ctx.ram_files["priv_key.pem"].path + "\"");
    }

    return config;
}

// --- Handlers & Routes ---

void on_login_query(const zenoh::Query& query, Agent_Ctx& ctx) {
    std::string credentials = std::string(query.get_parameters());
    std::string csr = crypto_generate_csr(ctx);

    // Call Controller Auth/Sign service
    // In real code, use ctx.session->get(...) to send (credentials + csr)
    // Here we simulate the return of a signed cert:
    std::string mock_signed_cert = "-----BEGIN CERTIFICATE-----\n..."; 
    
    // Store signed cert in RAM
    write_to_ram(ctx, "cert.pem", mock_signed_cert);

    // Signal main loop to restart Zenoh with mTLS
    query.reply("flotilla/agent/login", "UPGRADING");
    
    {
        std::lock_guard<std::mutex> lock(ctx.mtx);
        ctx.last_event = Event_State::UPGRADE_TLS;
        ctx.cv.notify_one();
    }
}

// --- Main Service Logic ---

void handle_state_boot(Agent_Ctx& ctx) {
    ctx.is_authenticated = false;
    ctx.active_routes.clear();

    if (ctx.session) ctx.session->close();

    // Config: No client certs, just the Root CA to verify the controller
    ctx.session = zenoh::Session::open(build_zenoh_config(ctx, false));
    
    // route attachment
    ctx.active_routes.push_back(ctx.session->declare_queryable("flotilla/agent/login",
        [&](const zenoh::Query& q) {
            on_login_query(q, ctx);
        },
    zenoh::closures::none));
}

void handle_state_upgrade(Agent_Ctx& ctx) {
    ctx.active_routes.clear();
    if (ctx.session) ctx.session->close();

    // Now config includes the cert.pem we just received
    ctx.session = zenoh::Session::open(build_zenoh_config(ctx, true));
    ctx.is_authenticated = true;
    
    // {
    //     std::lock_guard<std::mutex> lock(ctx.mtx);
    //     ctx.last_event = Event_State::TIMEOUT;
    //     ctx.cv.notify_one();
    // }
}

void handle_state_timeout(Agent_Ctx& ctx) {
    // Wipe the signed certificate from memory
    if (ctx.ram_files.count("cert.pem")) {
        close(ctx.ram_files["cert.pem"].fd);
        ctx.ram_files.erase("cert.pem");
    }

    // Force return to boot state
    std::lock_guard<std::mutex> lock(ctx.mtx);
    ctx.last_event = Event_State::BOOT_TLS;
    ctx.cv.notify_one();
}

void run_service(Agent_Ctx& ctx) {
    std::map<Event_State, std::function<void(Agent_Ctx&)>> state_handlers = {
      { Event_State::BOOT_TLS, handle_state_boot },
      { Event_State::UPGRADE_TLS, handle_state_upgrade },
      { Event_State::TIMEOUT, handle_state_timeout },
    };  
    {
        std::lock_guard<std::mutex> lock(ctx.mtx);
        ctx.last_event = Event_State::BOOT_TLS;
    }
    while (true) {
        Event_State job;
        {
            std::unique_lock<std::mutex> lock(ctx.mtx);

            // If we are in UPGRADE_TLS, we wait for the timeout
            if (ctx.is_authenticated) {
                // Wait for 8 hours or an explicit event
                if (!ctx.cv.wait_for(lock, ctx.session_duration, [&]{ return ctx.last_event != Event_State::NONE; })) {
                    ctx.last_event = Event_State::TIMEOUT;
                }
            } else {
                ctx.cv.wait(lock, [&]{ return ctx.last_event != Event_State::NONE; });
            }
            
            job = ctx.last_event;
            ctx.last_event = Event_State::NONE;
        }

        if (state_handlers.count(job)) state_handlers[job](ctx);
        if (job == Event_State::SHUTDOWN) break;
    }
}

int main() {
    Agent_Ctx agent_ctx;

    crypto_init_keys(agent_ctx);
    crypto_inject_root_ca(agent_ctx, CONTROLLER_ROOT_CA);

    run_service(agent_ctx);

    return 0;
}
