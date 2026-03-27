project(
  'Flotilla',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++23'],
)

add_project_arguments('-DZENOHCXX_ZENOHC', language: 'cpp')

inc_dir = include_directories('src')

zenohc_dep = dependency('zenohc', required: true)
zenohcxx_dep = dependency('zenohcxx', required: true)
zenoh_dep = [zenohc_dep, zenohcxx_dep]
# threads_dep = dependency('threads', required: true)
raylib_dep = dependency('raylib', required: true)
openssl_dep = dependency('openssl', required: true)

subdir('src/agent')
# subdir('src/controller')

# controller_source = [
#   'src/controller/controller.cpp'
# ]

# executable(
#   'controller',
#   controller_source,
#   install: false,
#   dependencies: [zenoh_dep, openssl_dep],
#   cpp_args: zenoh_args,
# )

# test('basic', exe)
