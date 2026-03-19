#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <linux/sockios.h>
#include <netinet/in.h>
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

#define MAX_EVENTS 10

std::atomic<bool> running(true);
int global_tun_fd = -1;
std::string global_net_name;

std::string generate_net_name() {
  struct ifaddrs *address_info = nullptr;
  std::set<std::string> interface_names;

  if (getifaddrs(&address_info) < 0) {
    throw std::runtime_error("Error get net info");
  } 

  for (ifaddrs * current = address_info; current != nullptr; current = current->ifa_next) {
    if (current->ifa_name) {
      interface_names.insert(current->ifa_name);
    }
  }

  freeifaddrs(address_info);

  std::string base_name = "agent";
  std::uint8_t net_number = 0;
  std::uint8_t try_limit = 10;
  
  while (net_number < try_limit) {
    std::string temp_name = base_name + std::to_string(net_number);
    if (!interface_names.contains(temp_name)) {
      return temp_name;
    }
    net_number++;
  }
  throw std::runtime_error("Failed to find an available virtual net name (agent0-agent9)");
}

int open_net_interface(std::string& net_name) {
  struct ifreq ifr;
  std::string tun_path = "/dev/net/tun";
  int tun_fd, err;
  if ((tun_fd = open(tun_path.c_str(), O_RDWR)) < 0) {
    throw std::runtime_error("Open /dev/net/tun");
  }

  std::memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  // set interface name
  std::strncpy(ifr.ifr_name, net_name.c_str(), IFNAMSIZ);

  if ((err = ioctl(tun_fd, TUNSETIFF, (void*)&ifr)) < 0) {
    close(tun_fd);
    throw std::runtime_error("ioctl TUNSETIFF");
  }
  net_name = ifr.ifr_name;

  return tun_fd;
}

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

zenoh::Config setup_dual_transport_config() {
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

int main() {
  // setup signals
  signal(SIGINT, cleanup);
  signal(SIGTERM, cleanup);

  try {
  auto config = setup_dual_transport_config();
  // session that handles gui and net
  zenoh::Session session = zenoh::Session::open(std::move(config));
  
  std::string net_name = generate_net_name();
  int tun_fd = open_net_interface(net_name);

  auto login_handler = session.declare_queryable("agent/ipc/login", [&](const zenoh::Query& query) {
                                                   std::string credentials = std::string(query.get_parameters());
                                                   std::println("Auth attempt: {}", credentials);
                                                   if (credentials == "") {
                                                     query.reply("agent/ipc/login", "OK");
                                                   } else {
                                                     query.reply("agent/ipc/login", "FAIL");
                                                   }
                                                 },
                                                 zenoh::closures::none);

  // start service
  service_loop(tun_fd, session);
  } catch (const std::exception& e) {
    std::println(stderr, "Critical Error: {}", e.what());
    cleanup(1);
  }

  cleanup(0);
  return 0;
}
