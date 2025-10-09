# Changelog
All notable changes to **Atlas Build Manager (ATL)** will be documented in this file.  
Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

---

## [Unreleased]
### Planned
- Custom build system
- Thread pool
- Queue job system
- Many more lua fields to come 
- Dependency graph, can be obtain with `gcc -MM -MF` for now or in-house system.  
- More CLI commands (`atl run`, `atl clean`, etc) with more options.
- A user install `atl config --install-user` that will add the binary to `$HOME/.local/bin`
- `atl new` more options such as `--file <file_name>`
- Logging system
- A system install `atl config --install-system` that will add the binary to `/usr/local/bin`
- Windows support  
- Cmake-ninja template projects support
- Valgrind and gdb support via `atl tool <tool_name>` 
    - Example: `atl tool --valgrind check` or similar command will run `valgrind --tool=memcheck <path/to/bin>`
    - Example: `atl tool --gdb launch` or similar command will run `gdb -- <path/to/bin>`
               `atl tool --gdb run <args>` maybe can be added.  

---

## [0.7.2] – 2025-10-09
### Added
- Multi-project support with a dedicated project container in C.
- `ATL_Project` now stores `source_files` as a Lua table reference, allowing C to access Lua-provided file lists.  
- Projects can be created with `atl.project.create { name=..., source_files=... }` and retrieved by name or ID.  
- ID tracking and `root_dir`, `build_type`, `compile_flags` storage per project.  
    - More to be included in future updates  
### Changed
### Fixed 
### Removed
- `ATL_Arena` allocations from `lists`.  
    - Source list and all upcoming lists will be heap based with `malloc`, `realloc`.  

---

## [0.6.0] – 2025-10-07
### Added
- `atl_platform` directory.  
- `atl_platform/linux` directory.  
-`ATL_run_process()` for running system commands, using `fork()` and `execvp()` in Linux.  
    - A `CreateProcess()` version for `Windows` can be added in future.  
### Changed
- Replaced `system(cmd)` calls with `ATL_run_process()`
### Fixed 
### Removed

---

## [0.5.1] – 2025-10-06
### Added
- Lua grounds `atl_lua_rt` directory  
- Lua global field `atl`
    - `os.info()` returns a lua object with various system information:
        - `name` system name  
        - `version` kernel version  
        - `machine` machine information  
        - `hostname` the host name
        - `cores` number of cores  
        - `big_endian` and `little_endian`: true or false
### Changed
- Command `atl build` will now check for `atl_build.lua` and initialize Lua states and fields.   
### Fixed 

### Removed

---

## [0.4.1] – 2025-10-04
### Added
### Changed
- `atl_debug.c/.h` moved `atl_dbglock` function there 
### Fixed 
### Removed
- `atl_dbg_file_append` had a bug with locks and hanged to infinty 

---

## [0.3.2] – 2025-10-03
### Added
- `atl init` initializes atl in current working directory.   
    - Alias: `atl -I`  
- `atl deinit` removes atl configuration in current working directory.   
    - Alias: `atl -D`  
    - For now it just removes `.atl` directory.  
- `atl_is_valid_name(char *)` for project and file created with `atl new --file` or `atl new --project` commands.  
- `ATL_is_initialized(void)` checks encoded version of binary vs `init.lock`
- `atl_glob_config_keys.h` to organize the global config keys.  
- `ATL_is_installed(const char *app)` that checks $PATH directories for the app.
    - returns true if a match is found.  
### Changed
- `atl new --project <project_name>` now creates `.atl` and other necessary directories inside `<project_name>`.  
- `atl new --project <project_name>` now create `.atl/init.lock` with version metadata.  
- Changed how `atl config --install` writes to .bashrc file.  
    - Now writes with BEGIN and END markers so it will be easier to cleanup correctly.  
    - And has guards to avoid duplicates.   
- Command `atl config --uninstall` now also removes `$HOME/.config/atlas_build_manager` directory.  
### Fixed 
- Various typos and bugs in `atl_glob_config_parser.c` 
    - Now correctly parses keys.  

---

## [0.2.1] – 2025-10-02
### Added
- Initial implementation of global config parser.
- Lock `.atl_is_installed.lock` will get written when `config --install` is invoked:  
  - `atl config --install` will fail if that lock file exists.  
  - `atl config --uninstall`, now requires the `atl_install.lock` to be able to run.  
  - `atl config --uninstall`, will remove the `.atl_is_installed.lock` as well.  
- Arena allocator for config entries (`ATL_arena_*`).
- CLI command `atl config` with options (install/uninstall).
- Shell RC detection (`bash`, `zsh`, `fish`, fallback).
- Install system:
  - Appends `$PATH` modification to correct rc file.
  - Supports auto-install via `./scripts/atl_build --install`.
  - Creates `atl_install.lock` and `path_to_bin_dir.atl` for safe installs.
- CMake integration:
  - Writes executable path (or bin dir) to `path_to_bin_dir.atl`.
  - Version tracking in `atl_install.lock`.
  - Separate build output directories (`build/bin/debug`, `build/bin/release`), based on the BUILD_TYPE from cmake.
- New `config` options:
  - `atl config --view` prints out atl.conf contents.  
  - `atl config --edit` opens the file with $EDITOR.  
- You can now chain commands using `...` or `---` separators.  
  - Example: `atl new --project cool_pr1 --- new --project cool_pr2`
  -          `atl new --project cool_pr1 ... new --project cool_pr2`
  -          `atl clean ... build ... run`
### Changed
- Build script now supports optional `--install` flag.
- Arena size for global config adjusted (64K default).

---

## [0.1.0] – 2025-10-01
### Added
- Bootstrapped repo with initial CMake setup.
- First working `atl` binary build.
- Core logging functions (`ATL_log`, `ATL_warnlog`, `ATL_errlog`).
- Early project structure (`src/`, `inc/`, `src/atl_commands/`).

