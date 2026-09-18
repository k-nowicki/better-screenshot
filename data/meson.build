gnome = import('gnome')

desktop_file = i18n.merge_file(
  type: 'desktop',
  input: 'io.github.k-nowicki.BetterScreenshot.desktop.in',
  output: 'io.github.k-nowicki.BetterScreenshot.desktop',
  po_dir: join_paths(meson.current_source_dir(), '../po'),
  install: true,
  install_dir: gnome_screenshot_appsdir
)

# Validate Desktop file
desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: [
      desktop_file.full_path()
    ]
  )
endif

metainfo_file = i18n.merge_file(
  input: 'io.github.k-nowicki.BetterScreenshot.metainfo.xml.in',
  output: 'io.github.k-nowicki.BetterScreenshot.metainfo.xml',
  po_dir: join_paths(meson.current_source_dir(), '../po'),
  install: true,
  install_dir: gnome_screenshot_appdatadir
)

# Validate metainfo
appstreamcli = find_program('appstreamctl', required: false)
if appstreamcli.found()
  test(
    'validate-metainfo',
    appstreamcli,
    args: [
      'validate',
      '--no-net',
      '--explain',
      metainfo_file.full_path()
    ]
  )
endif

service_conf = configuration_data()
service_conf.set('bindir', gnome_screenshot_bindir)
configure_file(
  input: 'io.github.k-nowicki.BetterScreenshot.service.in',
  output: 'io.github.k-nowicki.BetterScreenshot.service',
  configuration: service_conf,
  install: true,
  install_dir: gnome_screenshot_servicesdir
)

install_data(
  'io.github.k-nowicki.BetterScreenshot.gschema.xml',
  install_dir: gnome_screenshot_schemadir
)

glib_compile_schemas = find_program('glib-compile-schemas', required: false)
# Validata GSchema
if glib_compile_schemas.found()
  test(
    'validate-gschema',
    glib_compile_schemas,
    args: [
      '--strict',
      '--dry-run',
      meson.current_source_dir()
    ]
  )
endif

resources = gnome.compile_resources(
  'screenshot-resources',
  'io.github.k-nowicki.BetterScreenshot.gresource.xml',
  source_dir: '.',
  c_name: 'screenshot'
)

install_data(
  'better-screenshot.1',
  install_dir: join_paths(get_option('mandir'), 'man1')
)

subdir('icons')
