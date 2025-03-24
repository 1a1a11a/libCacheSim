{
  "targets": [
    {
      "target_name": "libcachesim-addon",
      "sources": [ "binding.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../libCacheSim/include",
        "/usr/include/glib-2.0",
        "/usr/lib/x86_64-linux-gnu/glib-2.0/include"
      ],
      "libraries": [
          "-L../_build",
          "-llibCacheSim",
          "-lglib-2.0",
          "-lzstd",
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ]
    }
  ]
}