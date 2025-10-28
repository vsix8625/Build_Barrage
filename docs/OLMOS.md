# Olmos DSL (for `barr`)

Olmos is the domain-specific language (DSL) used by Build Barrage (barr) to configure project builds.  
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


## Common Variables

| Variable | Description |
|----------|-------------|
|`project` | Name of the project, used to output binary name |
|`cflags` | Compiler flags for debug builds |
|`cflags_release` | Compiler flags for release builds |
|`defines` | Preprocessor defines (`-D` flags) |
|`includes` | Additional include directories (`-I` flags) |
|`build_type` | `"debug"` or `"release"` |
|`compiler` | Compiler to use (`"gcc"` or `"clang"`)|
|`out_dir` | Base output directory for build artifacts |
|`user_libs` | User defined `-l` librarys |
|`lib_paths` | Additional (`-L`) paths | 
|`target_type` | `"executable"` or `"library"` |

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


