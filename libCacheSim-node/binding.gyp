{
  "targets": [
    {
      "target_name": "cachesim-addon",
      "sources": [ "binding.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor/include"
      ],
      "libraries": [
        "<(module_root_dir)/vendor/liblibCacheSim.a",
        "<!(pkg-config --libs glib-2.0)",
        "-lzstd",
        "-lm",
        "-lpthread"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags": [ "-fPIC" ],
      "cflags_cc": [
        "-fPIC",
        "<!(pkg-config --cflags glib-2.0)"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ]
    }
  ]
}