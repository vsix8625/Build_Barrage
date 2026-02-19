# Olmos (for `Barrfile`)

Olmos is the scripting config layer, used by Build Barrage (`barr`) to configure project builds.  
It allows you to define compile flags, build type, output name,  
and other build variables in a human-readable syntax.  

## Syntax
- Assignment of variables: 
```olmos
   variable_name = "value";
```
- Strings must be quoted (`"` or `'`)
- Lines must end with a semicolon (`;`)
- Comments start with (`#`)

## Function calls: 
- Olmos supports function calls to perform dynamic configuration or query information.
 Currently, the following functions are supported:
`print()`
- Logs messages during the build evaluation.
```olmos
  print("Hello World");
```

`find_package()`
- Requests external packages for the build.
- The argument can be a single package or a space-separated list of package names.
- Packages are resolved via `barr`'s package scan system or a `pkg-config` fallback. 
```olmos
  find_package("zlib");
  find_package("xxhash fmt");
```
- Build Barrage will automatically tokenize the string and try resolve each package.
- If a package is found, its compiler flags(`cflags`) are added to the build context.  
- If a package is not found, a warning is issued and nothing is added for that package.  
- Failure to find a package that is required for compilation will result in a build/link error.  

`run_cmd()`
- Executes a shell command during the build evaluation.
- Accepts a single string argument containing the full command to run. 
- The command and tokenized and executed via `BARR_run_process()`, blocking the build until compilation. 
- Example_1: 
```olmos
  run_cmd("echo Building project...");
  run_cmd("mkdir -p build/temp");
```

- Example_2:
```olmos
   # Build a sub-project located in dir1
   run_cmd("barr build --dir dir1");
   
   # Define extra clean targets from the sub-project
   # NOTE: correct paths are important, wrong paths may delete unintended files
   dir1_clean_targets = "dir1/build dir1/.barr/cache/build.cache";
   CLEAN_TARGETS = "${dir1_clean_targets}";
   # These targets will used when `barr clean` is called   
```
- Useful for orchestrating multi-target builds, custom pre/post build steps, or invoking scripts.  
- Any output or errors from the command are logged; by default, errors do not stop the main build. 

## Common Variables

| Variable | Description |
|----------|-------------|
|`TARGET` | Name of the target, used to output binary and library name |
|`TARGET_TYPE` | `"executable"` or `"library"` |
|`VERSION` | Target version string |
|`CFLAGS` | Compiler flags for debug builds |
|`CFLAGS_RELEASE` | Compiler flags for release builds |
|`DEFINES` | Preprocessor defines (`-D` flags) |
|`INCLUDES` | Additional include directories (`-I` flags) |
|`AUTO_INCLUDE_DISCOVERY` | remains `on` by default (`append` or `off`) |
|`GEN_COMPILE_COMMANDS` | `yes` or `no` by default |
|`EXCLUDE_PATTERNS` | Extra scanner exclude patterns |
|`CLEAN_TARGETS` | Directories or files to be cleaned with `barr clean` command |
|`SOURCES` | Source files to compile |
|`GLOB_SOURCES` | Source directories to grab source files |
|`BUILD_TYPE` | `"debug"` or `"release"` |
|`COMPILER` | Compiler to use (`"gcc"` or `"clang"`)|
|`LINKER` | Linker to use (`"lld"` or `"gold"`)|
|`OUT_DIR` | Base output directory for build artifacts |
|`BINARY_OUT_PATH` | Overrides final binary output |
|`LFLAGS` | User defined `-l` libraries |
|`LIB_PATHS` | Additional (`-L`) paths | 

## Example Barrfile

```olmos
  print("Barrfile started");

  TARGET = "Build_Barrage";

  CFLAGS = "-Wall -Wextra -Werror -g";
  CFLAGS_RELEASE = "-O3 -Wall";

  DEFINES = "-DDEBUG";
  BUILD_TYPE = "debug";

  # Optional include directories, NOTICE: using includes var will turn off barr auto dir detection!
  # INCLUDES = "-Iinc";

  find_package("xxhash fmt");

  print("Barrfile ended");
```

- `TARGET` is used to generate the output binary path. 
- `CFLAGS` and `CFLAGS_RELEASE` are automatically selected based on the `BUILD_TYPE`.  
- `DEFINES` and `INCLUDES` override the defaults and passed to the compiler.  
- `TARGET_TYPE` library creates both shared and static lib.  
- `barr new --barrfile` generates a default Barrfile.  


