#pragma once

#include "zenoh/ConnectionStrategy.hpp"
#include <memory>
#include <zenoh/api/session.hxx>
#ifndef AGENTD_SESSION_FACTORY_HPP
#define AGENTD_SESSION_FACTORY_HPP

namespace Agentd {

enum class Source_Type { TUN_INTERFACE, INTERNAL_IPC };
enum class Security_Level { INSECURE, TLS, MTLS };

class Session_Factory {
private:
  static std::unique_ptr<Zenoh::Connection_Strategy> get_strategy(Security_Level sec_level);

public:
  static zenoh::Session create_session(Source_Type src_type, Security_Level sec_level);  
};

}

#endif
