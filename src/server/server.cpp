#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <memory>
#include <string>

#include "server.hpp"

Server::Server(std::string server_address) : server_address_(server_address) {}

Server::~Server() {}

void Server::run() {
  // sample::SampleServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
  // builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address_ << std::endl;
}
