voaac_sources = ['gstvoaac.c', 'gstvoaacenc.c']

voaac_comp = component('voaacenc')
voaac_dep = dependency('vo-aacenc', required : false,
  components: voaac_comp)
gstbz2 = library('gstvoaacenc',
  voaac_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstpbutils_dep, gstaudio_dep, voaac_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: voaac_comp
)
