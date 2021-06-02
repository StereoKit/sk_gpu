add_rules("mode.debug", "mode.release")

target("skgpu")
    set_version("0.1")
    set_kind("static")

    -- 2.5.3 is needed for utils.install.pkgconfig_importfiles
    set_xmakever("2.5.3") 
    add_rules("utils.install.pkgconfig_importfiles")
    add_headerfiles("../sk_gpu.h")

    add_files("sk_gpu_lib.cpp")