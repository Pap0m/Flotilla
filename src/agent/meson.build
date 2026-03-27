agentd_sources = [
  # 'AgentService.cpp',
  # 'IdentityManager.cpp',
  # 'IpcHandler.cpp',
  'NetworkManager.cpp',
  # 'ZenohController.cpp'
  'zenoh/ZenohConfigProfile.cpp',
  'agentd.cpp'
]

agentd_libs = static_library('agentd_core',
  agentd_sources,
  include_directories: inc_dir,
  dependencies: [zenoh_dep, openssl_dep]
)

agentd_dep = declare_dependency(
  link_with: agentd_libs,
  include_directories: inc_dir
)

agent_sources = [
  'gui/agent.cpp'
]

executable(
  'agentd',
  agentd_sources,
  install : false,
  dependencies : agentd_dep,
)

executable(
  'agent',
  agent_sources,
  install: false,
  dependencies: [zenoh_dep, raylib_dep],
)
