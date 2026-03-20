project(
  'Flotilla',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

dep_zenohc = dependency('zenohc')
dep_zenohcxx = dependency('zenohcxx')
dep_threads = dependency('threads')
dep_raylib = dependency('raylib')
dep_openssl = dependency('openssl')

zenoh_args = ['-DZENOHCXX_ZENOHC']

# shared_lib = [
#   shared_library() 
# ]

agentd_source = [
  'src/agent/main.cpp',
]

agent_source = [
  'src/agent/gui/main.cpp'
]

executable(
  'agentd',
  agentd_source,
  install : false,
  dependencies : [dep_zenohc, dep_zenohcxx, dep_openssl],
  cpp_args : zenoh_args,
)

executable(
  'agent',
  agent_source,
  install: false,
  dependencies: [dep_zenohc, dep_zenohcxx, dep_raylib],
  cpp_args: zenoh_args,
)

# test('basic', exe)
