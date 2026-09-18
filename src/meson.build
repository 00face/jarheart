jarheart_sources = files(
  'checks.c',
  'colorramp.c',
  'config-ini.c',
  'gamma-dummy.c',
  'hooks.c',
  'ipc.c',
  'location-manual.c',
  'location-timezone.c',
  'options.c',
  'pipeutils.c',
  'redshift.c',
  'signals.c',
  'solar.c',
  'systemtime.c',
)

jarheart_deps = [
  m_dep,
  rt_dep,
  threads_dep,
]

if enable_wayland
  wlr_gamma_proto_h = custom_target(
    'wlr-gamma-control-unstable-v1-client-protocol.h',
    input: meson.project_source_root() / 'protocols/wlr-gamma-control-unstable-v1.xml',
    output: 'wlr-gamma-control-unstable-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  wlr_gamma_proto_c = custom_target(
    'wlr-gamma-control-unstable-v1-protocol.c',
    input: meson.project_source_root() / 'protocols/wlr-gamma-control-unstable-v1.xml',
    output: 'wlr-gamma-control-unstable-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  jarheart_sources += [
    files('gamma-wayland.c'),
    wlr_gamma_proto_h,
    wlr_gamma_proto_c,
  ]
  jarheart_deps += wayland_client_dep
endif

if drm_dep.found()
  jarheart_sources += files('gamma-drm.c')
  jarheart_deps += drm_dep
endif

if enable_randr
  jarheart_sources += files('gamma-randr.c')
  jarheart_deps += randr_deps
endif

if enable_vidmode
  jarheart_sources += files('gamma-vidmode.c')
  jarheart_deps += [x11_dep, xxf86vm_dep]
endif

if enable_quartz
  jarheart_sources += files('gamma-quartz.c')
  jarheart_deps += quartz_dep
endif

if enable_wingdi
  jarheart_sources += files('gamma-w32gdi.c')
  jarheart_deps += wingdi_dep
endif

if enable_geoclue2
  jarheart_sources += files('location-geoclue2.c')
  jarheart_deps += [glib_dep, gio_dep]
endif

if enable_corelocation
  jarheart_sources += files('location-corelocation.m')
  jarheart_deps += corelocation_dep
endif

jarheart_bin = executable(
  'jarheart',
  jarheart_sources,
  include_directories: [inc_root, inc_src],
  dependencies: jarheart_deps,
  install: true,
)

# Also install 'redshift' as a symlink to 'jarheart' for backwards compatibility
# Meson >= 0.61 install_symlink
install_symlink(
  'redshift',
  pointing_to: 'jarheart',
  install_dir: get_option('bindir'),
)

if get_option('gui')
  python3 = import('python').find_installation('python3')
  python_dir = python3.get_install_dir() / 'redshift_gtk'

  defs_conf = configuration_data()
  defs_conf.set('bindir', get_option('prefix') / get_option('bindir'))
  defs_conf.set('localedir', get_option('prefix') / get_option('localedir'))

  configure_file(
    input: 'redshift-gtk/defs.py.in',
    output: 'defs.py',
    configuration: defs_conf,
    install: true,
    install_dir: python_dir,
  )

  install_data(
    'redshift-gtk/__init__.py',
    'redshift-gtk/controller.py',
    'redshift-gtk/statusicon.py',
    'redshift-gtk/settings_dialog.py',
    'redshift-gtk/utils.py',
    install_dir: python_dir,
  )

  launcher_conf = configuration_data()
  launcher_conf.set('pythondir', python3.get_install_dir())

  configure_file(
    input: 'redshift-gtk/redshift-gtk.in',
    output: 'redshift-gtk',
    configuration: launcher_conf,
    install: true,
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x',
  )

  install_symlink(
    'jarheart-gtk',
    pointing_to: 'redshift-gtk',
    install_dir: get_option('bindir'),
  )

  install_data(
    'jarheart-migrate.py',
    rename: 'jarheart-migrate',
    install_dir: get_option('bindir'),
    install_mode: 'rwxr-xr-x',
  )
endif
