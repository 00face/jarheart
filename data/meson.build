# Icons
if get_option('gui')
  install_data(
    'icons/hicolor/scalable/apps/redshift.svg',
    'icons/hicolor/scalable/apps/redshift-status-on.svg',
    'icons/hicolor/scalable/apps/redshift-status-off.svg',
    'icons/hicolor/scalable/apps/jarheart.svg',
    'icons/hicolor/scalable/apps/jarheart-status-on.svg',
    'icons/hicolor/scalable/apps/jarheart-status-off.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
  )

  if get_option('ubuntu_icons')
    install_data(
      'icons/ubuntu-mono-dark/scalable/apps/redshift-status-on.svg',
      'icons/ubuntu-mono-dark/scalable/apps/redshift-status-off.svg',
      install_dir: get_option('datadir') / 'icons/ubuntu-mono-dark/scalable/apps',
    )
    install_data(
      'icons/ubuntu-mono-light/scalable/apps/redshift-status-on.svg',
      'icons/ubuntu-mono-light/scalable/apps/redshift-status-off.svg',
      install_dir: get_option('datadir') / 'icons/ubuntu-mono-light/scalable/apps',
    )
  endif

  # Desktop entries
  i18n.merge_file(
    input: 'applications/jarheart.desktop.in',
    output: 'jarheart.desktop',
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )

  i18n.merge_file(
    input: 'applications/jarheart-gtk.desktop.in',
    output: 'jarheart-gtk.desktop',
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )

  i18n.merge_file(
    input: 'applications/jarheart-settings.desktop.in',
    output: 'jarheart-settings.desktop',
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )

  i18n.merge_file(
    input: 'applications/redshift.desktop.in',
    output: 'redshift.desktop',
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )

  i18n.merge_file(
    input: 'applications/redshift-gtk.desktop.in',
    output: 'redshift-gtk.desktop',
    po_dir: '../po',
    type: 'desktop',
    install: true,
    install_dir: get_option('datadir') / 'applications',
  )

  # AppStream MetaInfo
  i18n.merge_file(
    input: 'appdata/redshift-gtk.metainfo.xml.in',
    output: 'redshift-gtk.metainfo.xml',
    po_dir: '../po',
    install: true,
    install_dir: get_option('datadir') / 'metainfo',
  )
endif

# Systemd user units
if systemd_dep.found()
  if get_option('prefix') == '/usr'
    systemd_userunitdir = systemd_dep.get_variable(
      pkgconfig: 'systemduserunitdir',
      default_value: '/usr/lib/systemd/user',
    )
  else
    systemd_userunitdir = get_option('prefix') / 'lib/systemd/user'
  endif

  unit_conf = configuration_data()
  unit_conf.set('bindir', get_option('prefix') / get_option('bindir'))

  configure_file(
    input: 'systemd/jarheart.service.in',
    output: 'jarheart.service',
    configuration: unit_conf,
    install: true,
    install_dir: systemd_userunitdir,
  )

  configure_file(
    input: 'systemd/redshift.service.in',
    output: 'redshift.service',
    configuration: unit_conf,
    install: true,
    install_dir: systemd_userunitdir,
  )

  if get_option('gui')
    configure_file(
      input: 'systemd/jarheart-gtk.service.in',
      output: 'jarheart-gtk.service',
      configuration: unit_conf,
      install: true,
      install_dir: systemd_userunitdir,
    )

    configure_file(
      input: 'systemd/redshift-gtk.service.in',
      output: 'redshift-gtk.service',
      configuration: unit_conf,
      install: true,
      install_dir: systemd_userunitdir,
    )
  endif
endif

# Man page
install_man('../redshift.1')
