wl_req = '>= 1.4'

wl_scanner = find_program('wayland-scanner', required: false)
wayland_comp = component('wayland')

if not wl_scanner.found()
  dependency('wayland-scanner-not-found', required: false,
    components: wayland_comp)
endif

wl_protocol_dep = dependency('wayland-protocols', version: wl_req, required: false, components: wayland_comp)
wl_client_dep = dependency('wayland-client', version: wl_req, required: false, components: wayland_comp)
libdrm_dep = dependency('libdrm', version: '>= 2.4.55', required: false, components: wayland_comp)

gstwayland = library('gstwayland-' + api_version,
  'wayland.c',
  c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
  include_directories : [configinc, libsinc],
  version : libversion,
  soversion : soversion,
  install : true,
  dependencies : [gst_dep, gstvideo_dep],
  components: wayland_comp
)

gstwayland_dep = declare_dependency(link_with : gstwayland,
  include_directories : [libsinc],
  dependencies : [gst_dep, gstvideo_dep])
