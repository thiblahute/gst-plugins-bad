mms_comp = component('libmms')
mms_dep = dependency('libmms', version : '>= 0.4', required : false,
  components: mms_comp)

gstmms = library('gstmms',
  'gstmms.c',
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, mms_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: mms_comp
)
