# Build Barrage (barr)

A build tool for C that interfaces directly with the system compiler.  

You write a `Barrfile`, `barr` evaluates it, and invokes the compiler accordingly.

# Why `barr`? 

I built `barr` as a personal challenge to understand the build process and wanted a way to jump straight into coding without much manual setup and boilerplate.  
- Purpose-Built: It was designed to solve my workflow needs for C projects.  

- Learning Milestone: The codebase is preserved as a snapshot of my growth and understanding at the time of development.   

- Tested: Used daily for months in other projects. Reliable for my needs.   

# Project Status

- Active development is paused, and I do not provide ongoing support.  

- This project is considered feature-complete for my workflow.  

- Feel free to fork and adapt it to your needs.  

---

**Version:** 1.0.1   
**LICENSE:** Apache 2.0  

## Requirements
- Tested on Arch, Mint, Ubuntu, Fedora

**Platform:** Linux  
**Dependencies:**
- Before running bootstrap script, ensure the following are installed:
    - General: `git`, `libxxhash` (Development Headers)

    - Arch: `sudo pacman -S base-devel xxhash`

    - Ubuntu/Mint: `sudo apt install build-essential libxxhash-dev`

    - Fedora/RHEL: `sudo dnf install @development-tools xxhash-devel`
---

## Installation

```bash
git clone https://github.com/vsix8625/Build_Barrage.git
cd Build_Barrage
./scripts/build.sh
```

## How it works

The `build.sh` script is a bootstrap tool. It uses the system compiler to build a minimal version of `barr`.  
Once the bootstrap is complete, `barr` will automatically rebuild itself to produce a final, optimized binary.  

Once built, install it:

```bash
# Option A: copy to any directory in your $PATH
cp build/release/bin/barr ~/.local/bin/

# Option B: using barr install --prefix
# If you have in $PATH: $HOME/opt/usr/bin 
barr install --prefix $HOME/opt/usr # will install to ~/opt/usr/bin

# Option C: system install
sudo ./build/release/bin/barr install   # installs to /usr/local/bin
```

---

## Quick Start

Create a new project and get to a running binary:

```bash
barr new --project demo
cd demo
barr new --barrfile      # creates a default Barrfile in cwd 
barr new --main          # creates a main.c with "Hello from barr" main function in src
barr build               # read the Barrfile and build the sources
barr run                 # run the resulting binary
```

Commands can be chained with `...` or `---`, both separators are equivalent:

```bash
barr new --barrfile --- new --main --- new --pair util --- build --- run
```

---

## The Barrfile

The `Barrfile` is barr's build configuration — a declarative script with variable interpolation and a small set of built-in functions.

**Variable conventions:**
- `ALL_CAPS` — reserved keys that control barr's behavior (compiler, flags, output dirs, etc.)
- `BARR_` prefix — read-only built-in variables (OS, detected compilers, paths)
- `lowercase` / `camelCase` — your own custom variables, free to define and reference

A freshly generated `Barrfile` looks like this:

```ini
print("Barrfile execution started");

TARGET        = "myapp";
VERSION       = "0.0.1";
TARGET_TYPE   = "executable"; 

print("Building ${TARGET}-v${VERSION} | Type: ${TARGET_TYPE}");

COMPILER      = "/usr/bin/clang";
LINKER        = "lld";

BUILD_TYPE    = "debug";     
OUT_DIR       = "build/${BUILD_TYPE}/${TARGET}";  

std           = "-std=c23";
CFLAGS        = "${std} -Wall -Werror -Wextra -g";
CFLAGS_RELEASE = "${std} -Wall -O3";

DEFINES       = "-DDEBUG";

# LFLAGS      = "-lpthread";       
# LIB_PATHS   = "-L/usr/local/lib";
# GEN_COMPILE_COMMANDS = "on";    
# EXCLUDE_PATTERNS = "build test"; 
```

```ini
# src/main.c is the default main entry
MAIN_ENTRY = "barr.c";   # file containing the entry point for your executable.
```

**File discovery**:

`barr` scans the project root recursively for:
- C/C++ Sources: `.c`, `.cpp`,`.cc`, `.cxx`, `.C`. You can override this with `GLOB_SOURCES` or `SOURCES`.

- Headers: `.h`, `.hpp`,`.hh`, `.hxx`. You can override this with `AUTO_INCLUDE_DISCOVERY="off"`.

```ini
GLOB_SOURCES = "src engine/core";  # scan specific directories
SOURCES = "src/main.c src/util.c"; # explicit file list
AUTO_INCLUDE_DISCOVERY = "on"      # controls header scanning (`on` [default], `append`, `off`).
INCLUDES = "-I. -Iinc -Isrc";      # defining this will set AUTO_INCLUDE_DISCOVERY to "off" unless it was set to "append"
```

**System packages** via pkg-config:

```ini
pkgs = "xxhash zlib";
find_package("${pkgs}");
```

**Modules** let you build a dependency before the main target:

```ini
add_module("engine", "engine", "required");   
engine_includes = "-Iengine/include";
MODULES_INCLUDES = "${engine_includes}";
```

**Running arbitrary commands** with `run_cmd()` — barr will ask for your approval before executing:
- You can skip confirmation prompt by passing `--trust` to the command. 
- Example: `barr build --trust`.    
```ini
run_cmd("./tools/gen_headers.sh");
```

Run `barr status --vars` to see all available built-in variables for your current project.

---

## Scaffolding

```bash
barr new --project <name>          # new directory with barr initialized
barr new --barrfile                # create default Barrfile in current dir
barr new --main                    # create src/main.c
barr new --file <name> [dir]       # create a single file
barr new --pair <name> --dir <dir> # create <name>.c and <name>.h together
```

The `--pair` command also accepts `--ext` and `--public`:

```bash
barr new --pair renderer --dir graphics
# → graphics/renderer.c, graphics/renderer.h

barr new --pair graphics --ext .cpp --public true
# if --dir is not specified it writes to src by default
# → src/graphics.cpp, inc/graphics.hpp
```

## Inspecting Your Project

```bash
barr status             # fast timestamp scan of source files
barr status --all       # full file status with build summary
barr status --vars      # list all Barrfile variables (built-in + user-defined)
barr status --binfo     # show stored build info (name, type, version, paths, timestamp)
```

```bash
barr debug --cache      # show content hashes for all tracked source files
barr debug --fsinfo     # directory summary: file count, sizes, executables
```

## Forward Observer (fo)

`fo` is an experimental file watcher bundled with barr. It monitors source file modification times and triggers `barr build` automatically when changes are detected.

Typical workflow — run these in two separate terminal panes:

```bash
# Pane 1: deploy fo in your project directory
barr fo deploy
# [barr_log]: Forward Observer deployed: 19114

# Pane 2: watch the live log (requires shell 'watch')
barr fo watch
# [19:53:36]: idle — no changes detected
# ...
# [19:56:16]: TRIGGERED — modified sources detected
# [19:56:16]: barr build exit: 0
```

Save a source file and fo picks it up within its tick interval and rebuilds. Then `barr run` to test.

```bash
barr fo report    # check fo status and last build timestamp
barr fo dismiss   # dismiss (stop) the watcher
```

`fo` is not required for normal use and is considered experimental.

---

## Tools

`barr tool` (alias: `-tl`) launches external development tools against the last build binary.  
No need to locate the path manually, `barr` resolves it from the build info automatically.  

```bash
barr tool --gdb        # Launch GDB with the last built binary loaded
barr tool --perf       # Run perf record on the binary, then use perf report to inspect
barr tool --valgrind   # Run Valgrind memcheck on the binary
barr tool --strace     # Run strace -c and write syscall summary to a .txt file in the project root
```

`--gdb` opens GDB with the binary loaded and ready to `run`. Useful for debug builds where symbols are present `-g` in `CFLAGS`.  

`--perf` runs `perf record` against the binary. After it completes, inspect the results with: 
```bash
perf report
# or
perf report --stdio > perf_report.txt # your file name
```

`--valgrind` runs `--tool=memcheck` against the binary. Useful for catching heap errors and confirming clean exits.  

`--strace` runs `strace -c` and writes the syscall summary to `<target>_strace_syscall.txt` in the project root rather than printing to terminal.  

---

## Debug

```bash
barr debug --cache      # Show hashes for all tracked source files
barr debug --fsinfo     # Directory summary: file count, sizes, executables  
```

---

## Installing Your Project

`barr install` works for any project. It reads your build info and installs to the appropriate system directories under a prefix (default `/usr/local`):

- **Executables** → `<prefix>/bin/<name>`
- **Shared libraries** → `<prefix>/lib/<name>.so`
- **Static libraries** → `<prefix>/lib/<name>.a`
- **Public headers** — any `-I` paths from your `CFLAGS` are copied to `<prefix>/include/<name>/`

```bash
sudo barr install             # installs to /usr/local
sudo barr uninstall           # removes installed files
```

`barr` tracks what it installed so `uninstall` knows exactly what to clean up.

---

## Configuration

```bash
barr config open        # open the local Barrfile in $EDITOR (requires initialized project)
```

---

## Commands Reference

```bash
build        -b     Build project (incremental)
rebuild      -rb    Clean and build
run          -r     Run latest binary
clean        -c     Remove build directory
init         -I     Initialize barr in current directory
deinit       -D     Remove barr from current directory
new          -n     Create project, files, or scaffolding
config       -C     Manage configuration
status       -s     Project status and variable inspection
fo                  Forward observer (file watcher)
debug        -db    Cache, filesystem, and build diagnostics
install      -i     Install project to system (executables → bin/, libraries → lib/, headers → include/)
uninstall    -uni   Remove installed files from system

barr help <command>   # detailed options for any command
```

---

## Limitations & Known Issues

- `Linux` only: No `macOS` or `Windows` support.  

- Changing `BUILD_TYPE` in the Barrfile does not flag the cache change for recompilation. A manual rebuild is advised.    

- When running a target with `barr run` (`-r`), flags intended for the executable may sometimes be interpreted by barr itself (e.g., `--verbose`). Works in most cases and does not affect normal usage.  

- `barr status` uses modification time for quick checks. In some cases (e.g., edit + undo + save), a file may appear “modified” even if content hasn’t changed. The actual build (`barr build`) uses content hashing and will correctly skip unchanged files.  

- `barr build --turbo` batch build is experimental. Gains are situational: many small files with no `global` or `static` data may benefit, but in real projects it often provides little advantage.  

- `Barrfile` script layer:
    - One `TARGET` per `Barrfile`.  
    - No multi-line strings.  
    - No conditional statements.  
    - Numbers should be enclosed in quotes: 

Example: 
```ini
major  = "0";  
minor  = "0";  
patch  = "12";  

VERSION  = "${major}.${minor}.${patch}";    

file_write("#define FOO_VERSION_MAJOR ${major}","${config_file}","true");  
file_write("#define FOO_VERSION_MINOR ${minor}","${config_file}","true");  
file_write("#define FOO_VERSION_PATCH ${patch}","${config_file}","true");  
               
file_write('#define FOO_VERSION_STRING "${major}.${minor}.${patch}"',"${config_file}","true");  

# Failure to do so may result in truncated digits.  

``` 

- When writing `#define` statements or C-strings to a file, use single quotes for the `file_write` function to wrap the double quotes.  
    - Example: `file_write('#define TYPE "${VAR}"', "file.h", "true");` # true for append file

- For C++ main.cpp has to be set with `MAIN_ENTRY=main.cpp` otherwise `barr` expects main.c.  

- `Barrfile` must contain at least one variable assignment (e.g `TARGET`). A file containing only comments or whitespace may trigger an arena allocation error.  

- `fo` is experimental. Treat it as a convenience tool, not a stable feature.

For a deeper dive into Barr’s compilation stages, modules, and internal workflow, see [docs/technical.md](docs/technical.md).
