smoothstreaming_sources = [
  'gstmssdemux.c',
  'gstmssmanifest.c',
  'gstmssfragmentparser.c',
  'gstsmoothstreaming-plugin.c',
]

mms_comp = component('smoothstreaming')
mms_dep = dependency('libxml-2.0', version : '>= 2.8', required : false, components: mms_comp)

gstmss = library('gstsmoothstreaming',
  smoothstreaming_sources,
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  link_args : noseh_link_args,
  include_directories : [configinc, libsinc],
  dependencies : [gst_dep, gstadaptivedemux_dep, gstcodecparsers_dep,
          gsturidownloader_dep, mms_comp.dependencies(), gstisoff_dep,
          gio_dep],
  install : true,
  install_dir : plugins_install_dir,
  components: mms_comp
)
