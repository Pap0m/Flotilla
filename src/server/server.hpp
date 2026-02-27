#pragma once
#ifndef FLOTILLA_SERVER_HPP_
#define FLOTILLA_SERVER_HPP_

#include <string>

class Server {
public:
  Server(std::string server_address);
  ~Server();

  void run();

private:
  std::string server_address_;
};

#endif // FLOTILLA_SERVER_HPP_
