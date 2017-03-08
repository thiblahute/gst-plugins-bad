openh264_sources = [
  'gstopenh264dec.cpp',
  'gstopenh264enc.cpp',
  'gstopenh264plugin.c',
]

openh264_comp = component('openh264')
openh264_dep = dependency('openh264', version : '>= 1.3.0', required : false,
  components: openh264_comp)

# FIXME: check if C++ compiler is available via new add_languages() semantics in meson git (~v30)
gstopenh264 = library('gstopenh264',
  openh264_sources,
  c_args : gst_plugins_bad_args,
  link_args : noseh_link_args,
  include_directories : [configinc],
  dependencies : [gstvideo_dep, openh264_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: openh264_comp
)
