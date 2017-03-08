d3dvideosink_sources = [
  'd3dhelpers.c',
  'd3dvideosink.c',
]

if host_system == 'windows'
  d3d_videosink_comp = component('d3dvideosink')

  d3d_videosink_dep = [
    cc.find_dependency('d3d9', headers: 'd3d9.h', libs: 'd3d9', components: d3d_videosink_comp),
    cc.find_dependency('gdi32', libs: 'gdi32', components: d3d_videosink_comp)]

  gstd3dvideosink = library('gstd3d',
    d3dvideosink_sources,
    c_args : gst_plugins_bad_args,
    include_directories : [configinc],
    dependencies : [gstvideo_dep] + d3d_videosink_comp.dependencies(),
    install : true,
    install_dir : plugins_install_dir,
    components: d3d_videosink_comp
  )
endif
