#include "AgentDispatcher.hpp"
#include <string>
#include <thread>
#include <zenoh/api/sample.hxx>

namespace Agentd {

void Agent_Dispatcher::start() {
  // Handle IPC -> TUN
  auto sub = session.declare_subscriber("rt/data/**", [&](const zenoh::Sample sample) {
                                          std::string payload = sample.get_payload().as_string();
                                          net_manager.write_data_to_tun(payload);
                                        }, []{});
  // Handle TUN -> IPC
  std::thread tun_thread([this]() {
                           while (running) {
                             std::string packet = net_manager.read_data_from_tun();
                             if (!packet.empty()) {
                               session.put("rt/data/from_tun", packet);
                             }
                           }
                         });
  tun_thread.detach();
}

}
