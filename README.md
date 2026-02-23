# Build Barrage (barr)

A build tool for C that talks directly to compilers — no Makefile magic, no CMake abstraction. You write a `Barrfile`, barr reads it, barr calls the compiler. That's it.

**Version:** 0.25.0 · **Linux only**

---

## Requirements

- GCC or Clang
- CMake (bootstrap only — used once to build barr, then barr takes over)
- `libxxhash` (system package)
- `mold` or `lld` linker (optional but recommended)

---

## Installation

```bash
git clone <repo_url>
cd Build_Barrage
./scripts/build.sh
```

The script uses CMake to compile an initial `barr` binary, then immediately uses that binary to rebuild itself with full optimizations. You'll see some warnings during the bootstrap — they're expected on a fresh run.

Once built, install it:

```bash
# Option A: copy to any directory in your $PATH
cp build/release/bin/barr ~/.local/bin/

# Option B: system install
sudo ./build/release/bin/barr install   # installs to /usr/local/bin
```

---

## Quick Start

Create a new project and get to a running binary in one command:

```bash
barr new --project demo
cd demo
barr new --barrfile ... new --main ... build ... run
```

`...` chains commands sequentially. Output:

```
[barr_log]: Barrfile created
[barr_log]: Created src/main.c
Building barr_default-v0.0.1 | Type: executable
[1/1] >>>> src/main.c
[barr_log]: Total: 215.7 ms
[barr_log]: Running: build/barr_default/debug/bin/barr_default-v0.0.1
Hello, from barr
```

The resulting project structure:

```
demo/
├── .barr/              # barr internal state (cache, metadata)
├── Barrfile            # build configuration script
├── build/
│   └── barr_default/
│       └── debug/
│           ├── bin/barr_default
│           ├── libbarr.a
│           └── obj/
├── inc/
└── src/
    └── main.c
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
TARGET_TYPE   = "executable";   # "executable" | "static" | "shared"

print("Building ${TARGET}-v${VERSION} | Type: ${TARGET_TYPE}");

COMPILER      = "/usr/bin/clang";
LINKER        = "lld";

BUILD_TYPE    = "debug";                          # "debug" | "release"
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

**Specifying a single entry point** instead of full source discovery:

```bash
MAIN_SOURCE = "barr.c";   # barr builds this file and discovers the rest automatically
```

 — barr scans the project root recursively for `.c` files. You can override this:

```bash
GLOB_SOURCES = "src engine/core";    # scan specific directories
SOURCES = "src/main.c src/util.c";   # explicit file list
```

**System packages** via pkg-config:

```bash
pkgs = "xxhash zlib";
find_package("${pkgs}");
```

**Modules** let you build a dependency before the main target:

```bash
add_module("engine", "engine", "required");
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

---

## Building & Running

```bash
barr build          # compile changed files only (content + flags hashing)
barr rebuild        # clean then full build
barr run            # run the latest binary
barr clean          # remove build directory and cache
```

`barr build --turbo` enables unity builds (merges sources before compiling). Faster for large projects but experimental — results may vary.

Commands can be chained with `...` or `---`:

```bash
barr build ... run
barr rebuild --- run
```

---

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

---

## Recovery

Save and restore project state (build artifacts + barr metadata + Barrfile):

```bash
barr recovery save "pre-big-change"
barr recovery list        # shows numbered index of all saved states
barr recovery restore 1   # restore by index (e.g. barr recovery restore 6)
barr recovery destroy 1
```

Useful before risky refactors or config changes. Restore by the index shown in `list`.

---

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

`barr install` works for any project, not just barr itself. It reads your build info and installs to the appropriate system directories under a prefix (default `/usr/local`):

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

The CPU governor commands are a convenience for squeezing out faster build times on machines where the governor defaults to a power-saving mode.

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
recovery     -ry    Save and restore project state
fo                  Forward observer (file watcher)
debug        -db    Cache, filesystem, and build diagnostics
install      -i     Install project to system (executables → bin/, libraries → lib/, headers → include/)
uninstall    -uni   Remove installed files from system

barr help <command>   # detailed options for any command
```

---

## Limitations & Known Issues

- **Linux only.** No macOS or Windows support at this time.
- **`fo` is experimental.** Treat it as a convenience tool, not a stable feature.
- **Corrupted metadata edge case.** If `.barr/data/source_files_log` is missing, `barr build` may report "nothing to compile" instead of erroring and rebuilding. Use `barr recovery restore` if your build state gets into a bad place.
