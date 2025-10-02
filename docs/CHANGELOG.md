# Changelog
All notable changes to **Atlas Build Manager (ATL)** will be documented in this file.  
Format inspired by [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic Versioning](https://semver.org/).

---

## [Unreleased]
### Planned
- Project-level `atl.build` based in lua script for project builds
- More CLI commands (`atl new`, `atl clean`, etc) with more options.
- Config merging with environment variables.
- Custom build system
- Cmake-ninja template projects support
- `atl new` more options such as `--file <file_name>`

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

