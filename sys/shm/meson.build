shm_sources = [
  'shmpipe.c',
  'shmalloc.c',
  'gstshm.c',
  'gstshmsrc.c',
  'gstshmsink.c',
]

shm_enabled = false
shm_deps = []
shm_comp = component('shm')

shm_deps = [gstbase_dep]
libs = ['rt']
if  (host_system == 'osx' or host_system == 'bsd')
  libs = []
endif

rt_dep = cc.find_dependency('rt', libs: libs,
  headers: 'sys/socket.h', required: false, components: shm_comp)
gstshm = library('gstshm',
  shm_sources,
  c_args : gst_plugins_bad_args + ['-DSHM_PIPE_USE_GLIB'],
  include_directories : [configinc],
  dependencies : [gstbase_dep, shm_comp.dependencies()],
  install : true,
  install_dir : plugins_install_dir,
  components: shm_comp
)
