project(
  'Flotilla',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

zenoh_option = ['-DZENOHCXX_ZENOHC']

dependencies = [
  dependency('uuid'),
  dependency('zenohc'),
  dependency('zenohcxx'),
  dependency('threads')
]

# shared_lib = [
#   shared_library() 
# ]

sources = [
  'src/agent/main.cpp',
]

exe = executable(
  'agent',
  sources,
  install : false,
  dependencies : dependencies,
  cpp_args : zenoh_option,
)

test('basic', exe)
