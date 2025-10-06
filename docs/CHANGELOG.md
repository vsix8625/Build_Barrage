# Changelog
All notable changes to **Atlas Build Manager (ATL)** will be documented in this file.  
Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

---

## [Unreleased]
### Planned
- Many more lua fields to come 
- More CLI commands (`atl run`, `atl clean`, etc) with more options.
- A user install `atl config --install-user` that will add the binary to `$HOME/.local/bin`
- A system install `atl config --install-system` that will add the binary to `/usr/local/bin`
- Custom build system
- Cmake-ninja template projects support
- Logging system
- `atl new` more options such as `--file <file_name>`
- Win32 support  

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

