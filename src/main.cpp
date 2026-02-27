#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <grpcpp/grpcpp.h>

#include "server/server.hpp"

void timeout() {
  std::random_device rd;
  std::mt19937 eng(rd());
  std::uniform_int_distribution<> distr(150, 301);

  uint32_t random_number = distr(eng);

  std::this_thread::sleep_for(std::chrono::seconds(random_number));
}

int main() {
  Server s("0.0.0.0:50051");
  s.run();
  timeout();

  

  return 0; 
}
