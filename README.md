# Build Barrage (barr)

A build tool for C that interfaces directly with the system compiler.  

You write a `Barrfile`, barr evaluates it, and invokes the compiler accordingly.

**Version:** 0.25.0  
**Platform:** Linux

# State: Stable Milestone

**Build Barrage** is now considered a complete milestone project.  
- Current status: Stable and functional.  
- Maintenance: This repository is a snapshot of my first self-hosted build tool. While I use it in my daily workflow, active feature development (including the "Planned" items in the changelog) is currently paused.
- The Goal: This project served as a playground to implement and understand core systems concepts

---

## Requirements

- GCC or Clang
- `libxxhash` (system package)
- `mold` or `lld` linker (optional but recommended)

---

## Installation

```bash
git clone <repo_url>
cd Build_Barrage
./scripts/build.sh
```

The bootstrap process uses your system compiler to build a minimal version of `barr`, which then takes over to build the final, optimized production binary.

Once built, install it:

```bash
# Option A: copy to any directory in your $PATH
cp build/release/bin/barr ~/.local/bin/

# Option B: system install
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
barr build ... run
barr rebuild --- run
```

---

## The Barrfile

The `Barrfile` is barr's build configuration — a declarative script with variable interpolation and a small set of built-in functions.

**Variable conventions:**
- `ALL_CAPS` — reserved keys that control barr's behavior (compiler, flags, output dirs, etc.)
- `BARR_` prefix — read-only built-in variables (OS, detected compilers, paths)
- `lowercase` / `camelCase` — your own custom variables, free to define and reference

A freshly generated `Barrfile` looks like this:

```bash
print("Barrfile execution started");

TARGET        = "myapp";
VERSION       = "0.0.1";
TARGET_TYPE   = "executable"; 

print("Building ${TARGET}-v${VERSION} | Type: ${TARGET_TYPE}");

COMPILER      = "/usr/bin/clang";
LINKER        = "lld";

BUILD_TYPE    = "debug";     
OUT_DIR       = "build/${TARGET}/${BUILD_TYPE}";  

std           = "-std=c23";
CFLAGS        = "${std} -Wall -Werror -Wextra -g";
CFLAGS_RELEASE = "${std} -Wall -O3";

DEFINES       = "-DDEBUG";

# LFLAGS      = "-lpthread";       
# LIB_PATHS   = "-L/usr/local/lib";
# GEN_COMPILE_COMMANDS = "on";    
# EXCLUDE_PATTERNS = "build test"; 
```

```bash
MAIN_ENTRY = "barr.c";   # file containing the entry point for your executable.
```

**File discovery**:

`barr` scans the project root recursively for:
- C/C++ Sources: `.c`, `.cpp`,`.cc`, `.cxx`, `.C`. You can override this with `GLOB_SOURCES` or `SOURCES`.
- Headers: `.h`, `.hpp`,`.hh`, `.hxx`. You can override this with `AUTO_INCLUDE_DISCOVERY="off"`.

```bash
GLOB_SOURCES = "src engine/core";  # scan specific directories
SOURCES = "src/main.c src/util.c"; # explicit file list
AUTO_INCLUDE_DISCOVERY = "on"      # controls header scanning (`on` [default], `append`, `off`).
INCLUDES = "-I. -Iinc -Isrc";      # defining this will set AUTO_INCLUDE_DISCOVERY to "off" unless it was set to "append"
```

**System packages** via pkg-config:

```bash
pkgs = "xxhash zlib";
find_package("${pkgs}");
```

**Modules** let you build a dependency before the main target:

```bash
add_module("engine", "engine", "required");   # name, path/to/dir, relevance
engine_includes = "-Iengine/include";
MODULES_INCLUDES = "${engine_includes}";
```

**Running arbitrary commands** with `run_cmd()` — barr will ask for your approval before executing:

```bash
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
barr new --pair renderer --dir src
# → src/renderer.c, src/renderer.h

barr new --pair graphics --dir src --ext .cpp --public true
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

# Pane 2: watch the live log
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

barr tracks what it installed so `uninstall` knows exactly what to clean up.

---

## Configuration

```bash
barr config open        # open the local Barrfile in $EDITOR (requires initialized project)
```

---

## Commands Reference

```
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

- **Linux only.** No macOS or Windows support.  
- **`fo` is experimental.** Treat it as a convenience tool, not a stable feature.
- Changing `BUILD_TYPE` in the Barrfile does not automatically trigger a rebuild. Barr logs a warning advising a rebuild, but you need to manually run `barr rebuild`.
- When running a target with `barr run` (`-r`), flags intended for the executable may sometimes be interpreted by barr itself (e.g., `--verbose`). Works in most cases and does not affect normal usage.
- `barr status` uses modification time for quick checks. In some cases (e.g., edit + undo + save), a file may appear “modified” even if content hasn’t changed. The actual build (`barr build`) uses content hashing and will correctly skip unchanged files.
- `--turbo` batch build is experimental. Gains are situational: many small files with no globals/static data may benefit, but in real projects it often provides little advantage.

## Note

I explored many ideas while building Barr. This version is stable, fully functional, and serves my daily workflow. I’m very happy with how it turned out — it’s my first completed project and a personal reference for my approach to building a self-hosted build tool.

For a deeper dive into Barr’s compilation stages, modules, and internal workflow, see [docs/technical.md](docs/technical.md).
