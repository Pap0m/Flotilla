#pragma once
#ifndef AGENTD_NETWORK_MANAGER_HPP
#define AGENTD_NETWORK_MANAGER_HPP
#include <string>

namespace Agentd {

// NetworkManager: Handles the TUN/TAP interface lifecycle (ioctl, read/write, routing tables).

class NetworkManager {
private:
  int tun_fd;
  std::string tun_name;

public:
  NetworkManager(const char* dev_name);
  ~NetworkManager();

  void set_tun_name(const char* dev_name);
  std::string gen_tun_name();
  void new_tun_interface();
  void configure_tun_interface(const std::string& ip_addr, const std::string& ip_mask);
  void add_route_tun_interface(const std::string& external_ip_addr, const std::string& external_ip_mask);
  void delete_tun_interface();
  void write_data_to_tun(std::string& data);
  std::string read_data_from_tun();
};

}

#endif
