project(
  'please-speed',
  'cpp',
  'c',
  version : '0.1.0',
  meson_version : '>=1.1',
  default_options : [
    'cpp_std=c++20',
  ],
)

wren_dep = dependency('wren',
  fallback : ['wren', 'wren_dep'])

yyjson_dep = dependency('yyjson',
  fallback : ['yyjson', 'yyjson_dep'])

openssl = dependency('openssl')

curl = dependency('libcurl')

cxxopts_dep = dependency('cxxopts',
  fallback : ['cxxopts', 'cxxopts_dep'])

cmake = import('cmake')

vcpkg_path = get_option('vcpkg_path')

libzip_opts = cmake.subproject_options()
libzip_opts.add_cmake_defines({
  'BUILD_TESTING'  : 'OFF',
  'BUILD_EXAMPLES' : 'OFF',
  'BUILD_OSSFUZZ'  : 'OFF',
  'CMAKE_PREFIX_PATH' : vcpkg_path,
  'ZLIB_INCLUDE_DIR'  : vcpkg_path / 'include',
  'ZLIB_LIBRARY'      : vcpkg_path / 'lib' / 'zlib.lib',
})

libzip_proj = cmake.subproject('libzip', options : libzip_opts)
libzip_dep = libzip_proj.dependency('zip')

executable('please-speed',
  sources : [
    'src/main.cpp',
    'src/wren_bindings.cpp',
    'src/manifest.cpp',
    'src/server_export.cpp',
    'src/resolver.cpp',
    'src/packer.cpp',
  ],
  include_directories : ['src'],
  dependencies : [
    wren_dep,
    openssl,
    curl,
    yyjson_dep,
    cxxopts_dep,
    libzip_dep,
  ],
)