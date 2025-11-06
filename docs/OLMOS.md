# Olmos DSL (for `barr`)

Olmos is the domain-specific language (DSL) used by Build Barrage (`barr`) to configure project builds.  
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
   # NOTE: this is dangerous correct path is important
   dir1_clean_targets = "dir1/build dir1/.barr/cache/build.cache";
   clean_targets = "${dir1_clean_targets}";
   # clean targets will used when barr clean is called   
```
- Useful for orchestrating multi-target builds, custom pre/post build steps, or invoking scripts.  
- Any output or errors from the command are logged; by default, errors do not stop the main build. 

## Common Variables

| Variable | Description |
|----------|-------------|
|`target` | Name of the target, used to output binary and library name |
|`target_type` | `"executable"` or `"library"` |
|`version` | Target version string |
|`cflags` | Compiler flags for debug builds |
|`cflags_release` | Compiler flags for release builds |
|`defines` | Preprocessor defines (`-D` flags) |
|`includes` | Additional include directories (`-I` flags) |
|`auto_include_discovery` | remains `on` by default (`append` or `off`) |
|`gen_compile_commands` | `yes` or `no` by default |
|`exclude_patterns` | Extra scanner exclude patterns |
|`clean_targets` | Directories or files to be cleaned with `barr clean` command |
|`sources` | Source files to compile |
|`glob_sources` | Source directories to grab source files |
|`build_type` | `"debug"` or `"release"` |
|`compiler` | Compiler to use (`"gcc"` or `"clang"`)|
|`linker` | Linker to use (`"lld"` or `"gold"`)|
|`out_dir` | Base output directory for build artifacts |
|`binary_out_path` | Overrides final binary output |
|`lflags` | User defined `-l` libraries |
|`lib_paths` | Additional (`-L`) paths | 

## Example Barrfile

```olmos
  print("Barrfile started");

  project = "Aurora";

  cflags = "-Wall -Wextra -Werror -g";
  cflags_release = "-O3 -Wall";

  defines = "-DDEBUG";
  build_type = "debug";

  # Optional include directories, NOTICE: using includes var will turn off barr auto dir detection!
  # includes = "-Iinc";

  find_package("xxhash fmt");

  print("Barrfile ended");
```

- `project` is used to generate the output binary path. 
- `cflags` and `cflags_release` are automatically selected based on the `build_type`.  
- `defines` and `includes` override the defaults and passed to the compiler.  
- `target_type` library creates both shared and static lib.  


