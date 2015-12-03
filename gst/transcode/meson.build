c_args = ['-DHAVE_CONFIG_H',
  '-D_GNU_SOURCE',
  '-DGST_EXPORTS',
  '-DGST_DISABLE_DEPRECATED',
]

gst_transcoder_plugin = shared_library('gsttranscoderplugin',
  'gsttranscodebin.c', 'gst-cpu-throttling-clock.c', 'gsturitranscodebin.c',
  install : true,
  dependencies : [glib_dep, gobject_dep, gst_dep, gst_pbutils_dep],
  c_args : c_args,
  install_dir : plugins_install_dir,
)
