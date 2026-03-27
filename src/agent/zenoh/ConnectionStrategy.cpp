#include "ConnectionStrategy.hpp"
#include <zenoh/api/config.hxx>

namespace Zenoh {

void Insecure_Strategy::apply_security(zenoh::Config& config) const {
  config.insert_json5("scouting/multicast/enabled", "false");

  config.insert_json5("transport/link/tls/enabled", "false");
}

void TLS_Strategy::apply_security(zenoh::Config& config) const {
  config.insert_json5("scouting/multicast/enabled", "false");

  config.insert_json5("transport/link/tls/enabled", "true");

  config.insert_json5("transport/link/tls/certificate", "\"/etc/agentd/tls/cert.pem\"");
}

void MTLS_Strategy::apply_security(zenoh::Config& config) const {
  config.insert_json5("scouting/multicast/enabled", "false");

  config.insert_json5("transport/link/tls/enabled", "true");

  config.insert_json5("transport/link/tls/root_ca_certificate", "\"/etc/agentd/tls/ca.pem\"");
  config.insert_json5("transport/link/tls/certificate", "\"/etc/agentd/tls/cert.pem\"");
  config.insert_json5("transport/link/tls/private_key", "\"/etc/agentd/tls/priv_key.pem\"");
}

} // namespace Zenoh
