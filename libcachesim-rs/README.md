# Rust Bindings
We generate Rust bindings using `bindgen` (https://rust-lang.github.io/rust-bindgen/). 
## Generate Bindings
In the `libcachesim-rs` directory, if we run cargo build, the `build.rs` script will generate the bindings directory and all the files inside this directory. Then we can include the `bindings.rs` to our `lib.rs` and write safe Rust wrappers in wrapper.rs. For now, I have `pub struct Cache`, `pub struct Reader`, and `pub struct Request` with all corresponding methods. With these, I write a Rust equivalent `test.c` file in `main.rs`. 


## How to handle inline functions (In Command Line)? (Tentative Incomplete) Use `build.rs` intead
```bash
bindgen --experimental --wrap-static-fns \
    --wrap-static-fns-path bindings/wrap_static_fns \
    ../libCacheSim/include/libCacheSim.h \
    -o bindings/bindings.rs \
    --blocklist-item "_Tp" \
    --blocklist-item "std::.*" \
    --blocklist-file ".*/glib-2.0/glib/.*" \
    --blocklist-file ".*zstdReader.*" \
    -- \
    -I../libCacheSim/include \
    $(pkg-config --cflags glib-2.0)
```
This creates a `wrap_static_fns.c`. For now, we manually change the `#include "../libCacheSim/include/libCacheSim.h"` to `#include "libCacheSim.h"`. I have yet to figure out a way to make `bindgen` write relative path of the header file to the `wrap_static_fns.c` when `wrap_static_fns` is used. 

Now, we compile `wrap_static_fns.c` into an object file, then bundle it into a static library — while ensuring it includes the right header file.

Step 1: Compile `wrap_static_fns.c`
Run this command to compile `wrap_static_fns.c` into an object file (`.o`):
```Bash
clang -O -c \
    -o bindings/wrap_static_fns.o \
    bindings/wrap_static_fns.c \
    -include ../libCacheSim/include/libCacheSim.h \
    -I/../libCacheSim/include \
    $(pkg-config --cflags glib-2.0)
```
where 
- `-O`: Optimizes the build.
- `-c`: Compiles without linking (produces .o file).
- `-o ...wrap_static_fns.o`: Output object file path.
- `-include ...libCacheSim.h`: Forces inclusion of the header.
- `-I...`: Adds include path for libCacheSim.
- `$(pkg-config --cflags glib-2.0)`: Ensures GLib headers are accessible.

Step 2: Create a static library (`.a`)
Now bundle the object file into a static library:
```Bash
ar rcs bindings/libwrap_static_fns.a \
    bindings/wrap_static_fns.o
```
where 
- `ar`: Archive utility to create static libraries.
- `r`: Replaces existing object files in the archive.
- `c`: Creates the archive if it doesn’t exist.
- `s`: Adds an index (helps the linker find symbols faster).





# Testing `test.c`
If we see `no LC_RPATH's found`, we first doule check the library is installed 
```Bash
brew install zstd
```
And verify the library exists:
```Bash
ls /opt/homebrew/lib/libzstd*.dylib
```

Next, we can set RPATH during compilation. 
```Bash
gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -lzstd -o test.out -Wl,-rpath,/opt/homebrew/lib
```
If we see error `... was built for newer 'macOS' version (15.1) than being linked (15.0)`, we can add `MACOSX_DEPLOYMENT_TARGET=15.1` to the Bash command. 

Now, we verify RPATH is set correctly, where we check the final binary with:
```Bash
otool -l test.out | grep -A2 RPATH
```
You should see:
```
  cmd LC_RPATH
  path /opt/homebrew/lib (offset 12)
```









