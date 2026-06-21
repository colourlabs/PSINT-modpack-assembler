doctest_proj = cmake.subproject('doctest')
doctest_dep = doctest_proj.dependency('doctest')

test_src = files(
  'test_main.cpp',
  '../src/manifest.cpp',
  '../src/server_export.cpp',
)

test_exe = executable(
  'please-speed-tests',
  sources: test_src,
  include_directories: ['../src'],
  dependencies: [
    doctest_dep,
    yyjson_dep,
    openssl,
  ],
)

test('please-speed-tests', test_exe)
