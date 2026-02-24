## Core Build Pipeline

The `barr` build process is divided into three distinct stages to ensure incremental efficiency and clean linking.

### Stage 1: Compilation

1. Discovery: 
-`barr` auto identifies sources automatically by default. Specifying `GLOB_SOURCES` or `SOURCES` will disable auto scan.   
- Header discovery follow a similar pattern: `AUTO_INCLUDE_DISCOVERY` defaults to `on`, but can be set to `append` or `off`. 
- Setting `INCLUDES` manually will disable auto-scan for headers, unless `AUTO_INCLUDE_DISCOVERY` is set to `append`, in which case they are merged.  

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

1. It links `MAIN_ENTRY` object , `libbar.a`, any specified `MODULES`(built earlier) and system `pkgs` found by `find_package`.  

2. `TARGET`: The resulting binary is placed in `OUT_DIR`.  
