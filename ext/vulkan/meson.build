vulkan_sources = [
  'gstvulkan.c',
  'vkbuffermemory.c',
  'vkbufferpool.c',
  'vkdevice.c',
  'vkdisplay.c',
  'vkerror.c',
  'vkfence.c',
  'vkimagememory.c',
  'vkinstance.c',
  'vkmemory.c',
  'vkqueue.c',
  'vksink.c',
  'vkswapper.c',
  'vktrash.c',
  'vkupload.c',
  'vkutils.c',
  'vkwindow.c',
]

vulkan_defines = []
optional_deps = []

vulkan_comp = component('vulkan')
vulkan_dep = cc.find_dependency('vulkan',
  headers: ['vulkan/vulkan.h'],
  libs: 'vulkan',
  required: false,
  components: vulkan_comp
)

has_vulkan_windowing = false
vkconf = configuration_data()
xcb_dep = dependency('xcb', version : '>=1.10', required : false)
if xcb_dep.found()
  vulkan_sources += [
    'xcb/vkdisplay_xcb.c',
    'xcb/vkwindow_xcb.c',
    'xcb/xcb_event_source.c',
  ]

  optional_deps += xcb_dep
  has_vulkan_windowing = true
  vkconf.set10('GST_VULKAN_HAVE_WINDOW_XCB', 1)
endif

wayland_client_dep = dependency('wayland-client', version : '>=1.4',
    required : false)
if wayland_client_dep.found()
  vulkan_sources += [
    'wayland/vkdisplay_wayland.c',
    'wayland/vkwindow_wayland.c',
    'wayland/wayland_event_source.c',
  ]

  optional_deps += wayland_client_dep
  has_vulkan_windowing = true
  vkconf.set10('GST_VULKAN_HAVE_WINDOW_WAYLAND', 1)
endif

if not has_vulkan_windowing
  if vulkan_comp.value() == 'enabled'
    error('No vulkan windowing found.')
  endif
else
  configure_file(input : 'vkconfig.h.meson',
    output : 'vkconfig.h',
    configuration : vkconf)

  gstvulkan = library('gstvulkan',
    vulkan_sources,
    c_args : gst_plugins_bad_args + vulkan_defines,
    link_args : noseh_link_args,
    include_directories : [configinc],
    dependencies : [vulkan_comp.dependencies(), gstvideo_dep, gstbase_dep] + optional_deps,
    install : true,
    install_dir : plugins_install_dir,
    components: vulkan_comp
    )
endif
