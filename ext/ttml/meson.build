ttml_comp = component('ttml')
dependency('libxml-2.0', version : '>= 2.9.2', required : false, components: ttml_comp)
dependency('pango', required : false, components: ttml_comp)
dependency('cairo', required : false, components: ttml_comp)
dependency('pangocairo', required : false, components: ttml_comp)
libm = cc.find_dependency('m', libs: ['m'], required : false)

gstttmlsubs = library('gstttmlsubs',
  ['subtitle.c',
   'subtitlemeta.c',
   'gstttmlparse.c',
   'ttmlparse.c',
   'gstttmlrender.c',
   'gstttmlplugin.c'],
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, ttml_comp.dependencies(), libm],
  install : true,
  install_dir : plugins_install_dir,
  components: ttml_comp
)
