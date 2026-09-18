project(
  'jarheart',
  'c',
  version: '1.13.0',
  license: 'GPL-3.0-or-later',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
  meson_version: '>= 0.61.0'
)

add_project_arguments(
  '-DHAVE_CONFIG_H',
  '-D_DEFAULT_SOURCE',
  '-D_POSIX_C_SOURCE=200809L',
  language: 'c'
)

cc = meson.get_compiler('c')

cdata = configuration_data()
cdata.set_quoted('PACKAGE', meson.project_name())
cdata.set_quoted('PACKAGE_NAME', meson.project_name())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_STRING', meson.project_name() + ' ' + meson.project_version())
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/00face/jarheart/issues')
cdata.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
cdata.set('HAVE_CONFIG_H', 1)

# Check standard headers
check_headers = [
  'signal.h',
  'unistd.h',
  'stdint.h',
  'stdlib.h',
  'string.h',
  'locale.h',
  'poll.h',
  'sys/types.h',
  'sys/stat.h',
]
foreach h : check_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

# Check functions
check_functions = [
  'setlocale',
  'strchr',
  'floor',
  'pow',
  'clock_gettime',
]
foreach f : check_functions
  if cc.has_function(f)
    cdata.set('HAVE_' + f.to_upper(), 1)
  endif
endforeach

# Required / standard libraries
m_dep = cc.find_library('m', required: false)
rt_dep = cc.find_library('rt', required: false)
threads_dep = dependency('threads')

# Optional adjustment backends
wayland_client_dep = dependency('wayland-client', required: get_option('wayland'))
wayland_scanner = find_program('wayland-scanner', required: get_option('wayland'))
enable_wayland = wayland_client_dep.found() and wayland_scanner.found()
if enable_wayland
  cdata.set('ENABLE_WAYLAND', 1)
endif

drm_dep = dependency('libdrm', required: get_option('drm'))
if drm_dep.found()
  cdata.set('ENABLE_DRM', 1)
endif

x11_dep = dependency('x11', required: false)
xrandr_dep = dependency('xrandr', required: false)
xcb_dep = dependency('xcb', required: false)
xcb_randr_dep = dependency('xcb-randr', required: false)

enable_randr = false
randr_deps = []
if xrandr_dep.found() and x11_dep.found()
  enable_randr = true
  randr_deps = [x11_dep, xrandr_dep]
elif xcb_dep.found() and xcb_randr_dep.found()
  enable_randr = true
  randr_deps = [xcb_dep, xcb_randr_dep]
endif

if enable_randr
  cdata.set('ENABLE_RANDR', 1)
elif get_option('randr').enabled()
  error('RandR enabled but neither xrandr nor xcb-randr found')
endif

xxf86vm_dep = dependency('xxf86vm', required: get_option('vidmode'))
enable_vidmode = x11_dep.found() and xxf86vm_dep.found()
if enable_vidmode
  cdata.set('ENABLE_VIDMODE', 1)
endif

# macOS Quartz & CoreLocation
enable_quartz = false
quartz_dep = []
if host_machine.system() == 'darwin' and get_option('quartz').allowed()
  quartz_dep = dependency('appleframeworks', modules: ['ApplicationServices'], required: get_option('quartz'))
  if quartz_dep.found()
    enable_quartz = true
    cdata.set('ENABLE_QUARTZ', 1)
  endif
endif

enable_corelocation = false
corelocation_dep = []
if host_machine.system() == 'darwin' and get_option('corelocation').allowed()
  corelocation_dep = dependency('appleframeworks', modules: ['Foundation', 'Cocoa', 'CoreLocation'], required: get_option('corelocation'))
  if corelocation_dep.found()
    enable_corelocation = true
    cdata.set('ENABLE_CORELOCATION', 1)
  endif
endif

# Windows GDI
enable_wingdi = false
wingdi_dep = []
if host_machine.system() == 'windows' and get_option('wingdi').allowed()
  wingdi_dep = cc.find_library('gdi32', required: get_option('wingdi'))
  if wingdi_dep.found()
    enable_wingdi = true
    cdata.set('ENABLE_WINGDI', 1)
  endif
endif

# Location providers
glib_dep = dependency('glib-2.0', required: get_option('geoclue2'))
gio_dep = dependency('gio-2.0', version: '>= 2.26', required: get_option('geoclue2'))
enable_geoclue2 = glib_dep.found() and gio_dep.found()
if enable_geoclue2
  cdata.set('ENABLE_GEOCLUE2', 1)
endif

# Systemd user unit
systemd_dep = dependency('systemd', required: get_option('systemd'))

# i18n
i18n = import('i18n')
cdata.set('ENABLE_NLS', 1)

configure_file(
  output: 'config.h',
  configuration: cdata
)

inc_root = include_directories('.')
inc_src = include_directories('src')

subdir('src')
subdir('po')
subdir('data')
subdir('tests')

summary({
  'Wayland (wlr-gamma)': enable_wayland,
  'DRM (Direct Rendering)': drm_dep.found(),
  'RandR (X11)': enable_randr,
  'VidMode (Legacy X11)': enable_vidmode,
  'Quartz (macOS)': enable_quartz,
  'WinGDI (Windows)': enable_wingdi,
  'GeoClue2 (Location)': enable_geoclue2,
  'CoreLocation (macOS)': enable_corelocation,
  'GUI Status Icon': get_option('gui'),
  'Systemd Service': systemd_dep.found(),
}, section: 'Enabled Features', bool_yn: true)
