#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <memory>
#include <net/route.h>
#include <netinet/in.h>
#include <stdexcept>
#include <set>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "NetworkManager.hpp"

namespace Agentd {

NetworkManager::NetworkManager(const char* dev_name) {
  this->set_tun_name(dev_name);
  this->new_tun_interface();
}
NetworkManager::~NetworkManager() {
  this->delete_tun_interface();  
}

void NetworkManager::set_tun_name(const char* dev_name) {
  if (std::strlen(dev_name) > 16) {
    throw std::invalid_argument("Invalid lenght of dev_name");
  }
  this->tun_name = dev_name;
}

std::string NetworkManager::gen_tun_name() {
  std::set<std::string> interface_names;

  std::unique_ptr<struct ifaddrs, decltype(&freeifaddrs)> address_info(nullptr, freeifaddrs);
  {
    struct ifaddrs *temp_raw = nullptr;
    if (getifaddrs(&temp_raw) == 0) {
      address_info.reset(temp_raw); // pass temp_raw ownership to address_info
    } 
  }  
  for (ifaddrs *current = address_info.get(); current != nullptr; current = address_info.get()->ifa_next) {
    if (current->ifa_name) {
      interface_names.insert(current->ifa_name);
    }
  }

  std::string base_name = "agent";
  int tun_number = 0;
  int try_limit = 10;

  while (tun_number < try_limit) {
    std::string temp_name = base_name + std::to_string(tun_number);
    if (!interface_names.contains(temp_name)) {
      return temp_name;
    }
    tun_number++;
  }
  
  throw std::runtime_error("Failed to find an available virtual net name (agent0-agent9)");
}
void Agentd::NetworkManager::new_tun_interface() {
  struct ifreq ifr;
  std::string tun_path = "/dev/net/tun";
  int err;
  if ((this->tun_fd = open(tun_path.c_str(), O_RDWR)) < 0) {
    throw std::runtime_error("Open /dev/net/tun");
  }
  std::memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  // set interface name
  std::strncpy(ifr.ifr_name, this->tun_name.c_str(), IFNAMSIZ);

  if ((err = ioctl(this->tun_fd, TUNSETIFF, (void*)&ifr)) < 0) {
    close(tun_fd);
  }
  this->tun_name = ifr.ifr_name;
}


void NetworkManager::configure_tun_interface(const std::string& ip_addr, const std::string& ip_mask) {
  struct ifreq ifr;
  
  std::memset(&ifr, 0, sizeof(ifr));
  
  // dummy socket so i can configure the virtual interface
  int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock_fd < 0) {
    throw std::runtime_error("Failed: Socket creation");
  }
  // set interface name
  std::strncpy(ifr.ifr_name, this->tun_name.c_str(), IFNAMSIZ);

  // View of sockaddr_in struct that is part of the ifreq struct
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

void NetworkManager::add_route_tun_interface(const std::string& external_ip_addr, const std::string& external_ip_mask) {
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
  inet_pton(AF_INET, external_ip_addr.c_str(), &addr->sin_addr);

  // target company mask
  struct sockaddr_in* mask = reinterpret_cast<struct sockaddr_in*>(&route.rt_genmask);
  mask->sin_family = AF_INET;
  inet_pton(AF_INET, external_ip_mask.c_str(), &mask->sin_addr);

  route.rt_flags = RTF_UP; // UP route
  route.rt_dev = const_cast<char*>(this->tun_name.c_str()); // set interface name

  if (ioctl(sock_fd, SIOCADDRT, &route) < 0) {
    close(sock_fd);
    throw std::runtime_error("Failed to add route");
  }

  close(sock_fd);
}

void NetworkManager::delete_tun_interface() {
  if (this->tun_fd < 0) return;
  if (ioctl(this->tun_fd, TUNSETPERSIST, 0) < 0) {
    // TODO: log the error
  }
  int err = close(this->tun_fd);
  if (err < 0) {
    // return err;
  }
}

void NetworkManager::write_data_to_tun(std::string& data) {
  
}

std::string NetworkManager::read_data_from_tun() {
  return "";
}

} // namespace Agentd 
