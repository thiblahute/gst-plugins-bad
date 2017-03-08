fluidsynth_comp = component('fluidsynth')
fluidsynth_dep = dependency('fluidsynth', version : '>= 1.0', required : false,
  components: fluidsynth_comp)

gstfluidsynth = library('gstfluidsynthmidi',
  'gstfluiddec.c',
  c_args : gst_plugins_bad_args,
  include_directories : [configinc],
  dependencies : [gstaudio_dep, gst_dep, fluidsynth_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: fluidsynth_comp
)
