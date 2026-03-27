#include "SessionFactory.hpp"
#include "zenoh/ConnectionStrategy.hpp"
#include <algorithm>
#include <memory>
#include <zenoh/api/config.hxx>
#include <zenoh/api/session.hxx>

std::unique_ptr<Zenoh::Connection_Strategy> Agentd::Session_Factory::get_strategy(Security_Level sec_level) {
  switch (sec_level) {
    case Agentd::Security_Level::TLS:  return std::make_unique<Zenoh::TLS_Strategy>();
    case Agentd::Security_Level::MTLS: return std::make_unique<Zenoh::MTLS_Strategy>();
    default:                           return std::make_unique<Zenoh::Insecure_Strategy>(); 
  }
}

zenoh::Session Agentd::Session_Factory::create_session(Source_Type src_type, Security_Level sec_level) {
    zenoh::Config config = zenoh::Config::create_default();

    // set endpoint based on source
    if (src_type == Source_Type::TUN_INTERFACE) {
      config.insert_json5("listen/endpoints", "[\"tls/0.0.0.0:7447\"]");
    } else {
      config.insert_json5("listen/endpoints", "[\"tcp/127.0.0.1:7447\"]");
    }

    // Apply security level
    auto strategy = get_strategy(sec_level);
    strategy->apply_security(config);

    return zenoh::Session::open(std::move(config));
}
