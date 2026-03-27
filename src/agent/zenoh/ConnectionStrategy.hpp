#pragma once

#ifndef AGENTD_ZENOH_CONFIG_PROFILE_HPP
#define AGENTD_ZENOH_CONFIG_PROFILE_HPP

#include <zenoh/api/config.hxx>

namespace Zenoh {

class Connection_Strategy {
private:

public:
  virtual ~Connection_Strategy() = default;
  virtual void apply_security(zenoh::Config& base_config) const = 0;  
};

class Insecure_Strategy : public Connection_Strategy {
public:
  void apply_security(zenoh::Config& config) const override;
};

class TLS_Strategy : public Connection_Strategy {
public:
  void apply_security(zenoh::Config& config) const override;
};

class MTLS_Strategy : public Connection_Strategy {
public:
  void apply_security(zenoh::Config& config) const override;
};


}

#endif
