#pragma once

#include "NetworkManager.hpp"
#include <atomic>
#include <zenoh/api/session.hxx>
#ifndef AGENTD_AGENT_DISPATCHER
#define AGENTD_AGENT_DISPATCHER

namespace Agentd {

class Agent_Dispatcher {
private:
  zenoh::Session session;
  Agentd::NetworkManager& net_manager;
  std::atomic<bool> running;

public:
  Agent_Dispatcher();
  void start();
};


}
#endif
