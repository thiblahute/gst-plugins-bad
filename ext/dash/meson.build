dash_sources = [
  'gstdashdemux.c',
  'gstmpdparser.c',
  'gstplugin.c',
]

dash_comp = component('dash')
dash_dep = dependency('libxml-2.0', version : '>= 2.8', required : false,
  components: dash_comp)

gstsouphttpsrc = library('gstdashdemux',
  dash_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gstadaptivedemux_dep, gsturidownloader_dep, gsttag_dep,
                  gstnet_dep, gstbase_dep, gio_dep, gstisoff_dep, dash_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: dash_comp
)
