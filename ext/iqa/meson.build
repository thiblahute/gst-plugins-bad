iqa_comp = component('iqa')
iqa_dep = dependency('dssim', required : false, fallback: ['dssim', 'dssim_dep'],
  components: iqa_comp)

gstiqa = library('gstiqa',
  'iqa.c',
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API', '-DHAVE_DSSIM'],
  include_directories : [configinc],
  dependencies : [gst_dep, gstbadvideo_dep, gstbadbase_dep, iqa_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: iqa_comp
)
