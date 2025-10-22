# Changelog
All notable changes to **Build Barrage (BARR)** will be documented in this file.  
Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

---

## [Unreleased]
### Planned
- Custom build: hash + dependency track + cache systems (core systems implemented) 
- Garbage collector (initial system implemented)
    - Test gc functionality with a subsytem 
- Custom DSL with a C-like syntax
- Optimize build process. 
- A system install `barr config --install-system` that will add the binary to `/usr/local/bin`
- Project sessions and possibly `live-build` daemon.  
- Create compile_commands.json 
- More CLI commands (`barr run`, `barr clean`, etc) with more options.
- Logging system
- Valgrind and gdb support via `barr tool <tool_name>` 
    - Example: `barr tool --valgrind check` or similar command will run `valgrind --tool=memcheck <path/to/bin>`
    - Example: `barr tool --gdb launch` or similar command will run `gdb -- <path/to/bin>`
               `barr tool --gdb run <args>` maybe can be added.  
- Windows support  

---

## [0.11.1] – 2025-10-19
### Added
- Add `barr_gc.c/.h` Garbage collector for memory allocations
### Changed
### Fixed 
### Removed

---

## [0.10.1] – 2025-10-15
### Added
- `modes` added unnecessary `barr mode` command.  
    - Example: `barr mode WAR` will initiate war mode.  
    - `barr mode OFF` will turn off any active modes.   
- Add `barr_thread_pool.c/.h` basic thread pool created that runs based on amount of physical cores found by `sysconf`.  
### Changed
### Fixed 
### Removed

---

## [0.9.1] – 2025-10-14
### Added
- `SHA-256` hashing functions
- `BARR_HashMap`
- `barr build` now scans the current working directory and sub directories for source files.  
    - Also it creates a cache with `sha256` digest with the contents of source file, its includes and compile flags.  
    - And finally it runs the compiler building object files for changed source files.  
    - NOTE: compile flags passed on the function are a placeholder for now until the DSL would be ready.  
    - NOTE: This is the pre-alpha stage for the build system as a whole and changes will happen.   
### Changed
### Fixed 
### Removed

---

## [0.8.0] – 2025-10-11
### Added
- Began design of **custom BARR DSL** (C-like syntax) to replace Lua for defining projects.
- DSL will integrate natively with BARR’s core — no VM or script bridge required.
### Changed
### Fixed 
### Removed
- **Lua DSL integration**: all Lua dependencies and bindings removed.
  - `barr_lua_rt` directory deprecated.
  - Lua stack and state handling functions (`luaL_newstate`, etc.) eliminated.
  - DSL scripts (`barr_build.lua`) no longer supported.


---

## [0.7.2] – 2025-10-09
### Added
- Multi-project support with a dedicated project container in C.
- `BARR_Project` now stores `source_files` as a Lua table reference, allowing C to access Lua-provided file lists.  
- Projects can be created with `barr.project.create { name=..., source_files=... }` and retrieved by name or ID.  
- ID tracking and `root_dir`, `build_type`, `compile_flags` storage per project.  
    - More to be included in future updates  
### Changed
### Fixed 
### Removed
- `BARR_Arena` allocations from `lists`.  
    - Source list and all upcoming lists will be heap based with `malloc`, `realloc`.  

---

## [0.6.0] – 2025-10-07
### Added
- `barr_platform` directory.  
- `barr_platform/linux` directory.  
-`BARR_run_process()` for running system commands, using `fork()` and `execvp()` in Linux.  
    - A `CreateProcess()` version for `Windows` can be added in future.  
### Changed
- Replaced `system(cmd)` calls with `BARR_run_process()`
### Fixed 
### Removed

---

## [0.5.1] – 2025-10-06
### Added
- Lua grounds `barr_lua_rt` directory  
- Lua global field `barr`
    - `os.info()` returns a lua object with various system information:
        - `name` system name  
        - `version` kernel version  
        - `machine` machine information  
        - `hostname` the host name
        - `cores` number of cores  
        - `big_endian` and `little_endian`: true or false
### Changed
- Command `barr build` will now check for `barr_build.lua` and initialize Lua states and fields.   
### Fixed 

### Removed

---

## [0.4.1] – 2025-10-04
### Added
### Changed
- `barr_debug.c/.h` moved `barr_dbglock` function there 
### Fixed 
### Removed
- `barr_dbg_file_append` had a bug with locks and hanged to infinty 

---

## [0.3.2] – 2025-10-03
### Added
- `barr init` initializes barr in current working directory.   
    - Alias: `barr -I`  
- `barr deinit` removes barr configuration in current working directory.   
    - Alias: `barr -D`  
    - For now it just removes `.barr` directory.  
- `barr_is_valid_name(char *)` for project and file created with `barr new --file` or `barr new --project` commands.  
- `BARR_is_initialized(void)` checks encoded version of binary vs `init.lock`
- `barr_glob_config_keys.h` to organize the global config keys.  
- `BARR_is_installed(const char *app)` that checks $PATH directories for the app.
    - returns true if a match is found.  
### Changed
- `barr new --project <project_name>` now creates `.barr` and other necessary directories inside `<project_name>`.  
- `barr new --project <project_name>` now create `.barr/init.lock` with version metadata.  
- Changed how `barr config --install` writes to .bashrc file.  
    - Now writes with BEGIN and END markers so it will be easier to cleanup correctly.  
    - And has guards to avoid duplicates.   
- Command `barr config --uninstall` now also removes `$HOME/.config/build_barrage` directory.  
### Fixed 
- Various typos and bugs in `barr_glob_config_parser.c` 
    - Now correctly parses keys.  

---

## [0.2.1] – 2025-10-02
### Added
- Initial implementation of global config parser.
- Lock `.barr_is_installed.lock` will get written when `config --install` is invoked:  
  - `barr config --install` will fail if that lock file exists.  
  - `barr config --uninstall`, now requires the `barr_install.lock` to be able to run.  
  - `barr config --uninstall`, will remove the `.barr_is_installed.lock` as well.  
- Arena allocator for config entries (`BARR_arena_*`).
- CLI command `barr config` with options (install/uninstall).
- Shell RC detection (`bash`, `zsh`, `fish`, fallback).
- Install system:
  - Appends `$PATH` modification to correct rc file.
  - Supports auto-install via `./scripts/barr_build --install`.
  - Creates `barr_install.lock` and `path_to_bin_dir.barr` for safe installs.
- CMake integration:
  - Writes executable path (or bin dir) to `path_to_bin_dir.barr`.
  - Version tracking in `barr_install.lock`.
  - Separate build output directories (`build/bin/debug`, `build/bin/release`), based on the BUILD_TYPE from cmake.
- New `config` options:
  - `barr config --view` prints out barr.conf contents.  
  - `barr config --edit` opens the file with $EDITOR.  
- You can now chain commands using `...` or `---` separators.  
  - Example: `barr new --project cool_pr1 --- new --project cool_pr2`
  -          `barr new --project cool_pr1 ... new --project cool_pr2`
  -          `barr clean ... build ... run`
### Changed
- Build script now supports optional `--install` flag.
- Arena size for global config adjusted (64K default).

---

## [0.1.0] – 2025-10-01
### Added
- Bootstrapped repo with initial CMake setup.
- First working `barr` binary build.
- Core logging functions (`BARR_log`, `BARR_warnlog`, `BARR_errlog`).
- Early project structure (`src/`, `inc/`, `src/barr_commands/`).

