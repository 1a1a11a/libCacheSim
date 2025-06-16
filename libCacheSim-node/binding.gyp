{
  "targets": [
    {
      "target_name": "cachesim-addon",
      "sources": [ "binding.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../libCacheSim/include",
        "/usr/include/glib-2.0",
        "/usr/lib/x86_64-linux-gnu/glib-2.0/include",
        "/usr/lib/aarch64-linux-gnu/glib-2.0/include"
      ],
      "libraries": [
        "-L<(module_root_dir)/../_build",
        "-llibCacheSim",
        "-lglib-2.0",
        "-lzstd",
        "-lm",
        "-lpthread"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [ "-fPIC" ],
      "cflags_cc": [ "-fPIC" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "ldflags": [
        "-Wl,--whole-archive",
        "<(module_root_dir)/../_build/liblibCacheSim.a",
        "-Wl,--no-whole-archive"
      ]
    }
  ]
}