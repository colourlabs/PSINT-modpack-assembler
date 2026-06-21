project(
  'wren',
  'c',
)

wren_src = files(
  'src/optional/wren_opt_meta.c',
  'src/optional/wren_opt_random.c',
  'src/vm/wren_compiler.c',
  'src/vm/wren_core.c',
  'src/vm/wren_debug.c',
  'src/vm/wren_primitive.c',
  'src/vm/wren_utils.c',
  'src/vm/wren_value.c',
  'src/vm/wren_vm.c',
)

wren_inc = include_directories(
  'src/include',
  'src/vm',
  'src/optional',
)

wren_lib = static_library(
  'wren',
  wren_src,
  include_directories: wren_inc,
)

wren_dep = declare_dependency(
  link_with: wren_lib,
  include_directories: wren_inc,
)