test_solar = executable(
  'test_solar',
  'test_solar.c',
  '../src/solar.c',
  include_directories: [inc_root, inc_src],
  dependencies: [m_dep],
)
test('solar', test_solar)

test_colorramp = executable(
  'test_colorramp',
  'test_colorramp.c',
  '../src/colorramp.c',
  include_directories: [inc_root, inc_src],
  dependencies: [m_dep],
)
test('colorramp', test_colorramp)

test_config_ini = executable(
  'test_config_ini',
  'test_config_ini.c',
  '../src/config-ini.c',
  include_directories: [inc_root, inc_src],
)
test('config_ini', test_config_ini)

test_ipc = executable(
  'test_ipc',
  'test_ipc.c',
  '../src/ipc.c',
  '../src/colorramp.c',
  '../src/checks.c',
  '../src/location-timezone.c',
  include_directories: [inc_root, inc_src],
  dependencies: [m_dep],
)
test('ipc', test_ipc)

test_timezone = executable(
  'test_timezone',
  'test_timezone.c',
  '../src/location-timezone.c',
  include_directories: [inc_root, inc_src],
  dependencies: [m_dep],
)
test('timezone', test_timezone)


