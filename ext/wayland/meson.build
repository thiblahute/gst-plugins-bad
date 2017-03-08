wl_sources = [
    'gstwaylandsink.c',
    'wlshmallocator.c',
    'wlbuffer.c',
    'wldisplay.c',
    'wlwindow.c',
    'wlvideoformat.c',
    'wllinuxdmabuf.c'
]

if wayland_comp.enabled()
    protocols_datadir = wl_protocol_dep.get_pkgconfig_variable('pkgdatadir')

    protocol_defs = [
        ['/stable/viewporter/viewporter.xml', 'viewporter-protocol.c', 'viewporter-client-protocol.h'],
        ['/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml',
         'linux-dmabuf-unstable-v1-protocol.c', 'linux-dmabuf-unstable-v1-client-protocol.h']
    ]
    protocols_files = []

    foreach protodef: protocol_defs
        xmlfile = protocols_datadir + protodef.get(0)

        protocols_files += [custom_target(protodef.get(1),
          output : protodef.get(1),
          input : xmlfile,
          command : [wl_scanner, 'code', '@INPUT@', '@OUTPUT@'])]

        protocols_files += [custom_target(protodef.get(2),
          output : protodef.get(2),
          input : xmlfile,
          command : [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])]
    endforeach

    gstwaylandsink = library('gstwaylandsink',
        wl_sources + protocols_files,
        c_args : gst_plugins_bad_args + ['-DGST_USE_UNSTABLE_API'],
        include_directories : [configinc],
        dependencies : [gst_dep, gstvideo_dep, gstwayland_dep, gstallocators_dep,
                        wayland_comp.dependencies()],
        install : true,
        install_dir : plugins_install_dir,
        components: wayland_comp
    )
endif
