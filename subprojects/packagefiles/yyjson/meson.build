project(
  'yyjson',
  'c',
)

yyjson_lib = static_library(
  'yyjson',
  'src/yyjson.c',
  install: false,
)

yyjson_dep = declare_dependency(
  include_directories: include_directories(
    'src',
  ),
  link_with: yyjson_lib,
)