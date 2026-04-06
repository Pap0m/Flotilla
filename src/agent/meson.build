agentd_sources = [
  # 'Agent_Dispatcher.cpp',
  # 'zenoh/Session_Factory.cpp',
  # 'zenoh/Connection_Strategy.cpp',
  # 'network/Session_Manager.cpp',
  # 'agentd.cpp'
  'agent_proc.cpp'
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
