## Core Build Pipeline

The `barr` build process is divided into three distinct stages to ensure incremental efficiency and clean linking.

### Stage 1: Compilation

1. Discovery: 
-`barr` auto identifies sources automatically by default. Specifying `GLOB_SOURCES` or `SOURCES` will disable auto scan.   

- Header discovery follows a similar pattern: `AUTO_INCLUDE_DISCOVERY` defaults to `on`, but can be set to `append` or `off`. 

- Setting `INCLUDES` manually will disable auto-scan for headers, unless `AUTO_INCLUDE_DISCOVERY` is set to `append`, in which case they are merged.  

- Directories with the following patterns are excluded:

```ini
"Barrfile", "Makefile", "pyproject.toml", "CMakeLists.txt", "setup.py", "package.json", 

"Cargo.toml", "meson.build", "SConstruct", "go.md", ".csproj", "WORKSPACE", "WORSPACE.baze",

"BUILD", "BUILD.bazel", "configure.ac", "build", "bin", "obj", ".git", "cache", ".vs", ".idea", "test",

"CMakeFiles", "Debug",   "Release", ".barr",   "docs",  "assets", "scripts", "modes",

"pycache", ".vscode", "objects", "out",   "target", ".svn", ".hg",

"vendor", ".trash"
```
- Additional to this any specified `EXCLUDE_PATTERNS`

2. Hashing: `barr` runs a 64-bit `xxhash` on every source file, include file and finally, compiler flags themselves.  

3. Diffing: it compares these hashes against `.barr/cache/build.cache`.    
- Files with matching hashes are skipped.  

- Changed or new files are added to `compile_list`.  

4. Parallel Execution: `barr` spawns a Multi-Threaded (MT) pool to compile the `compile_list`.
- It invokes the `COMPILER` based on the configuration specified in `Barrfile`, producing `.o` object files.

### Stage 2: Archive 

`barr` uses an intermediate archiving step:
- Bundles all objects except `MAIN_ENTRY` into a local static library: `libbarr.a`.  

- Why? This makes the final stage faster, more organized, and ensures the core logic is treated as a single unit.  

### Stage 3: Link 

The final binary is produced by `LINKER` (e.g: `ld`, `lld`, `mold`): 

1. It links `MAIN_ENTRY` object , `libbarr.a`, any specified `MODULES`(built earlier) and system `pkgs` found by `find_package`.  

2. `TARGET`: The resulting binary is placed in `OUT_DIR`.  

---

## Library Example: Creating and Using `libfoo`

To show how `barr` handles libraries for system installs, lets look at a project called `foo`.  

1. The library structure: 
Headers should be separated into a public `include` directory. This allows `barr` to install them correctly without mixing private source files.  

```ruby
.
├── Barrfile
├── include/       <-- Public API
│   ├── foo.h
│   └── io.h
└── src/core/      <-- Implementation
    └── foo.c

```

2. The Installation Phase  
When you run `sudo barr install` on a project with `TARGET_TYPE=static` (or `shared`), `barr` maps your project to your system hierarchy:  
```bash
$ sudo barr install
[barr_log]: Prefix defaulted to: /usr/local
[barr_log]: Installing include directory: .../foo/include -> /usr/local/include/foo
[barr_log]: Installing library: .../libfoo.a -> /usr/local/lib/libfoo.a
[barr_log]: Added to manifest: /usr/local/include/foo
[barr_log]: Added to manifest: /usr/local/lib/libfoo.a
[barr_log]: Install complete
```

3. Using the Installed Library

Now, any other project on your system can use `foo` just like a standard system library.  

The source (`main.c`):
Note the name-spaced include `<foo/foo.h>`. This is possible because `barr` installed your headers into a subdirectory named after the project.  
```c
#include <foo/foo.h>

int main(void) {
    foo_printf("Hello from foo-barr\n");
    return 0;
}
```

### The New Project's `Barrfile`:
Since `/usr/loca/include` and `/usr/local/lib` are standard paths for GCC/Clang on Linux, you just need to tell the linker to look for `foo`:  
```ini
TARGET = "my_app";
LFLAGS = "-lfoo";
# We explicitly include the library path because some linkers (like lld) 
# may not search /usr/local/lib by default.
LIB_PATHS = "-L/usr/local/lib"; 
```

### Install Paths Resolution

`--prefix`: defines the final install location on the system (default: `/usr/local`).  
`--destdir`: adds a temporary staging root in front of that location.  

**Example: Installing and executable**

Assume:
```ini
TARGET_TYPE = "executable"
TARGET      = "my_app"
```

**Normal Install**
```bash
sudo barr install # installs to /usr/local by default
```

Result:
```ruby
/usr/local/bin/my_app
```

**Staged Install**
```bash
barr install --destdir ./pkgroot/ --prefix usr
```

Result:
```ruby
./pkgroot/usr/bin/my_app
```

Notice: 
```ruby
final_path = DESTDIR + PREFIX + bin/my_app
```

**Uninstall Behavior**

While installing `barr` writes to project's `.barr/data/install_info` file.  
At uninstall, it will read the info file and remove exactly those  files. 


---

## Module System: Using `add_module`

The module system allows one `barr` project to build and integrate another `barr` project as part of the same build flow.  

A module is simply another directory containing its own `Barrfile`. When declared, `barr` will:
- Invoke a nested `barr build` inside that directory.  

- If the module is required the parent build process will fail.  

---

### Basic Usage  

Inside your `Barrfile`:

```ini
add_module("foo","tools/foo","no"); # name, path to directory, required? 
```

Arguments: 
`name` = Logical module name (for registry)
`path` = Directory containing the module's `Barrfile`  
`required` = Optional: `"true"`,`"yes"` or `"required"`  

Only first two arguments are mandatory.  

If `required` is omitted, the module is treated as optional.  

---

```ruby
.
├── Barrfile
├── src/
│   └── main.c
└── tools/
    └── foo/
        ├── Barrfile
        └── src/
            └── foo.c
```

---

### What happens internally

When `barr` encounters"

```ini
add_module("foo","tools/foo","yes"); 
```

It performs:
```bash
barr build --dir tools/foo
```

Behavior:  
- If the module builds successfully → it is registered and integrated.  

- If it fails and is required → the parent build fails.  

- If it fails and is optional → the parent build continues.  

---

### Minimal Example  

Root `Barrfile`:
```ini
TARGET = "app";
TARGET_TYPE = "executable";

add_module("math", "modules/math", "required");
```

modules/math/Barrfile: 
```ini
TARGET = "math";
TARGET_TYPE = "static";
```

Build flow:
1. Root build starts  
2. `modules/math` is built  
3. If successful -> root continues  
4. If failed and required -> root aborts

