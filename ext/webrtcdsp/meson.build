webrtc_sources = [
  'gstwebrtcdsp.cpp',
  'gstwebrtcechoprobe.cpp'
]

webrtcdsp_comp = component('webrtcpdsp')
webrtcdsp_dep = dependency('webrtc-audio-processing', version : ['>= 0.2', '< 0.4'], required : false,
  components: webrtcdsp_comp)

gstwebrtcdsp = library('gstwebrtcdsp',
  webrtc_sources,
  cpp_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstbase_dep, gstaudio_dep, webrtcdsp_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: webrtcdsp_comp
)
