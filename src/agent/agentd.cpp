#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <linux/sockios.h>
#include <memory>
#include <netinet/in.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <thread>
#include <unistd.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <utility>
#include <vector>
#include <zenoh/api/bytes.hxx>
#include <zenoh/api/closures.hxx>
#include <zenoh/api/config.hxx>
#include <zenoh/api/query.hxx>
#include <zenoh/api/sample.hxx>
#include <zenoh/api/session.hxx>
#include <zenoh.hxx>
#include <net/route.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/core_names.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "NetworkManager.hpp"

#define ZENOHCXX_ZENOHC

#define SERVICE_DIR "/etc/agentd/"

#define ECC_TYPE "secp256k1"

#define MAX_EVENTS 10

std::atomic<bool> running(true);
int global_tun_fd = -1;
std::string global_net_name;


bool service_loop(int tun_fd, zenoh::Session& session) {
  struct epoll_event ev, events[MAX_EVENTS];
  int nfds;
  int epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) throw std::runtime_error("Failed epoll_create1");

  // add the tun fd to epoll
  ev.events = EPOLLIN;
  ev.data.fd = tun_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tun_fd, &ev) < 0) {
    throw std::runtime_error("Failed epoll_ctl for tun_fd");
  } 

  // define a zenoh subscriber for IPC
  auto gui_sub = session.declare_subscriber("agent/ipc/**", [](const zenoh::Sample& sample) {
                                          const auto& payload = sample.get_payload();
                                          // Handle command from gui
                                          std::println("Received command from GUI: {}", payload.as_string().c_str());
                                        }, zenoh::closures::none);
  auto net_sub = session.declare_subscriber("agent/net/tun_in", [tun_fd](const zenoh::Sample& sample) {
                                          const auto& payload = sample.get_payload();
                                          // Handle command from gui
                                          std::println("Received command from Controller: {}", payload.as_string().c_str());
                                          // write incoming internet traffic directly to the tun interface
                                          write(tun_fd, payload.as_vector().data(), payload.size());
                                        }, zenoh::closures::none);
  std::vector<char> buffer(2000);

  while (running) {
    nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 500); // 500 ms to check 'running' variable
    if (nfds < 0) {
      if (nfds == EINTR) continue; // if is interrupted by a signal handler
      break;
    }
    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == tun_fd) {
        // read tun interface traffic and send it over tls to remote peers
        ssize_t nread = read(tun_fd, buffer.data(), buffer.size());
        for (auto& data : buffer) {
          std::println("Data: {}", data);
        }
        if (nread > 0) {
          session.put("agent/net/tun_out", zenoh::Bytes(std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + nread)));
        } else if (nread < 0 && errno != EAGAIN) {
          std::println(stderr, "Read error on TUN");           
        }
      }
    }
  }
  return true;
}

void configure_net_interface(const std::string& net_name, const std::string& ip_addr, const std::string& ip_mask) {
  struct ifreq ifr;

  std::memset(&ifr, 0, sizeof(ifr));
  
  // dummy socket so i can configure the virtual interface
  int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock_fd < 0) {
    throw std::runtime_error("Failed: Socket creation");
  }
  // set interface name
  std::strncpy(ifr.ifr_name, net_name.c_str(), IFNAMSIZ);

  struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  // set ip
  addr->sin_family = AF_INET;
  inet_pton(AF_INET, ip_addr.c_str(), &addr->sin_addr);
  if (ioctl(sock_fd, SIOCSIFADDR, &ifr) < 0) {
    close(sock_fd);
    throw std::runtime_error("Failed to set IP");
  }

  // set mask
  inet_pton(AF_INET, ip_mask.c_str(), &addr->sin_addr);
  if (ioctl(sock_fd, SIOCSIFNETMASK, &ifr) < 0) {
    close(sock_fd);
    throw std::runtime_error("Failed to set Netmask");
  }

  // up interface
  // get the current flags
  ioctl(sock_fd, SIOCGIFFLAGS, &ifr);
  ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
  if (ioctl(sock_fd, SIOCSIFFLAGS, &ifr) < 0) {
    close(sock_fd);
    throw std::runtime_error("Failed to set interface UP");
  }

  close(sock_fd);
}

void add_company_route(const std::string& net_name, const std::string& company_ip, const std::string& company_mask) {
  struct rtentry route;

  std::memset(&route, 0, sizeof(route));

  // dummy socket so i can configure the virtual interface
  int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock_fd < 0) {
    throw std::runtime_error("Failed: Socket creation");
  }

  // target company ip
  struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(&route.rt_dst);
  addr->sin_family = AF_INET;
  inet_pton(AF_INET, company_ip.c_str(), &addr->sin_addr);

  // target company mask
  struct sockaddr_in* mask = reinterpret_cast<struct sockaddr_in*>(&route.rt_genmask);
  mask->sin_family = AF_INET;
  inet_pton(AF_INET, company_mask.c_str(), &mask->sin_addr);

  route.rt_flags = RTF_UP; // UP route
  route.rt_dev = const_cast<char*>(net_name.c_str()); // set interface name

  if (ioctl(sock_fd, SIOCADDRT, &route) < 0) {
    close(sock_fd);
    throw std::runtime_error("Failed to add route");
  }

  close(sock_fd);
}

void cleanup(int signum) {
  running = false;
  if (global_tun_fd >= 0) {
    close(global_tun_fd);
  }
  unlink("/tmp/agentd.sock");

  if (signum != 0) exit(signum);
}

zenoh::Config create_config() {
  zenoh::Config config = zenoh::Config::create_default();

  config.insert_json5("mode", "\"router\"");
  config.insert_json5("listen/endpoints", "[\"tcp/127.0.0.1:7447\"]");
  // , \"tls/0.0.0.0:7447\"]
  // require tls
  // config.insert_json5("transport/link/tls/enabled", "true");
  // // certificates
  // config.insert_json5("transport/link/tls/root_ca_certificate", "\"/etc/agentd/tls/ca.pem\"");
  // config.insert_json5("transport/link/tls/certificate", "\"/etc/agentd/tls/cert.pem\"");
  // config.insert_json5("transport/link/tls/private_key", "\"/etc/agentd/tls/priv_key.pem\"");
  // disable scouting
  config.insert_json5("scouting/multicast/enabled", "false");

  return config;
}

// void init_openssl() {
//   OpenSSL_add_all_algorithms();
//   ERR_load_BIO_strings();
//   ERR_load_crypto_strings();
// }


void gen_crt(EVP_PKEY *ec_key, const std::filesystem::path csr_path) {
  std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> req(X509_REQ_new(), X509_REQ_free);
  if (!req) {
    throw std::runtime_error("Failed to create X509_REQ");
  }
  X509_REQ_set_version(req.get(), 0);

  char hostname[256] = {0};
  if (gethostname(hostname, sizeof(hostname)) < 0) {
    throw std::runtime_error("Failed to get hostname");  
  }

  // set the subject name
  X509_NAME *name = X509_REQ_get_subject_name(req.get());
  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"XX", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"ZTNA-Agent", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)hostname, -1, -1, 0);

  // associate the public key with the CSR
  X509_REQ_set_pubkey(req.get(), ec_key);
  // sign the CSR with the private key
  if (!X509_REQ_sign(req.get(), ec_key, EVP_sha256())) {
    throw std::runtime_error("Failed to sign CSR");
  }

  std::unique_ptr<BIO, decltype(&BIO_free)> bio_csr(BIO_new_file(csr_path.c_str(), "w"), BIO_free); 
  if (!bio_csr || !PEM_write_bio_X509_REQ(bio_csr.get(), req.get())) {
    throw std::runtime_error("Failed to write CSR file");
  }
}

void gen_key(const std::filesystem::path& dir_path) {
  if (!std::filesystem::exists(dir_path)) {
    std::filesystem::create_directories(dir_path);
  }
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ec_key(EVP_EC_gen(ECC_TYPE), EVP_PKEY_free);
  if (!ec_key) {
    throw std::runtime_error("Failed to generate ec_key");
  }

  std::filesystem::path priv_path = dir_path / ECC_TYPE".key";
  std::filesystem::path pub_path = dir_path / ECC_TYPE".pem";
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_priv(BIO_new_file(priv_path.c_str(), "w"), BIO_free); 
  if (!bio_priv || !PEM_write_bio_PrivateKey(bio_priv.get(), ec_key.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
    throw std::runtime_error("Failed to write private key");
  }
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_pub(BIO_new_file(pub_path.c_str(), "w"), BIO_free); 
  if (!bio_pub || !PEM_write_bio_PUBKEY(bio_pub.get(), ec_key.get())) {
    throw std::runtime_error("Failed to write public key");
  }

  std::filesystem::permissions(priv_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
  std::filesystem::permissions(pub_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);

  gen_crt(ec_key.get(), SERVICE_DIR"tls/certificates");
}

int main() {
  if (!(geteuid() == 0)) {
    throw std::runtime_error("Run the agentd service as root");
  }
  Agentd::NetworkManager net = Agentd::NetworkManager("agent0");
  std::this_thread::sleep_for(std::chrono::seconds(60));
  // // setup signals
  // signal(SIGINT, cleanup);
  // signal(SIGTERM, cleanup);

  // try {
  // gen_key(SERVICE_DIR"tls/keys");

  // auto config = create_config();
  // // session that handles gui and net
  // zenoh::Session session = zenoh::Session::open(std::move(config));
  
  // std::string net_name = generate_net_name();
  // int tun_fd = open_net_interface(net_name);

  // auto login_handler = session.declare_queryable("agent/ipc/login", [&](const zenoh::Query& query) {
  //                                                  std::string credentials = std::string(query.get_parameters());
  //                                                  std::println("Auth attempt: {}", credentials);
  //                                                  if (credentials == "") {
  //                                                    query.reply("agent/ipc/login", "OK");
  //                                                  } else {
  //                                                    query.reply("agent/ipc/login", "FAIL");
  //                                                  }
  //                                                },
  //                                                zenoh::closures::none);

  // // start service
  // service_loop(tun_fd, session);
  // } catch (const std::exception& e) {
  //   std::println(stderr, "Critical Error: {}", e.what());
  //   cleanup(1);
  // }

  // cleanup(0);
  return 0;
}
