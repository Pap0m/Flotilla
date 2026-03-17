#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
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
#include <thread>
#include <unistd.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <vector>
#include <zenoh/api/config.hxx>
#include <zenoh/api/session.hxx>
#include <net/route.h>

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

std::uint32_t open_net_interface(std::string& net_name) {
  struct ifreq ifr;
  std::string tun_path = "/dev/net/tun";
  std::uint32_t tun_fd, err;
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
  std::strcpy(net_name.data(), ifr.ifr_name);

  return tun_fd;
}

std::string read_net_interface(std::uint32_t tun_fd) {
  std::vector<char> buffer(1600);
  char src_ip[INET_ADDRSTRLEN];
  char dst_ip[INET_ADDRSTRLEN];

  // while true -> epoll
  while (true) {
    ssize_t nbytes = read(tun_fd, buffer.data(), buffer.size());
    if (nbytes < 0) {
      throw std::runtime_error("read error");
    }

    struct iphdr *ip = reinterpret_cast<struct iphdr*>(buffer.data());

    // Convert bytes into human readable strings
    inet_ntop(AF_INET, &(ip->saddr), src_ip, INET6_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip->daddr), dst_ip, INET6_ADDRSTRLEN);

    std::println("Packet received: {} -> {} | Size: {} bytes", src_ip, dst_ip, buffer.size());
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void configure_net_interface(const std::string& net_name, const std::string& ip_addr, const std::string& ip_mask) {
  struct ifreq ifr;

  std::memset(&ifr, 0, sizeof(ifr));
  
  // dummy socket so i can configure the virtual interface
  std::uint32_t sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
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
  std::uint32_t sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
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

zenoh::Config configure_mtls(const std::string& ca, const std::string& cert, const std::string& key) {
  zenoh::Config config = zenoh::Config::create_default();

  config.insert_json5("transport/link/tls/enabled", "true");

  return config;
}

int main() {
  zenoh::Config config = zenoh::Config::create_default();
  zenoh::Session session = zenoh::Session::open(std::move(config));
  
  std::string net_name = generate_net_name();

  std::uint32_t tun_fd = open_net_interface(net_name);


  return 0;
}
