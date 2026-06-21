project(
  'please-speed',
  'cpp',
  'c',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
  ],
)

wren_dep = dependency(
  'wren',
  fallback: [
    'wren',
    'wren_dep',
  ],
)
yyjson_dep = dependency(
  'yyjson',
  fallback: [
    'yyjson',
    'yyjson_dep',
  ],
)
openssl = dependency(
  'openssl',
)
curl = dependency(
  'libcurl',
)

executable(
  'please-speed',
  sources: [
    'src/main.cpp',
    'src/wren_bindings.cpp',
    'src/manifest.cpp',
    'src/server_export.cpp',
    'src/resolver.cpp',
  ],
  include_directories: [
    'include',
    'src',
  ],
  dependencies: [
    wren_dep,
    openssl,
    curl,
    yyjson_dep,
  ],
)