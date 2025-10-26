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
- Function calls: 
```olmos
  print("Hello World");
```
Only `print()` is currently supported for logging purposes.  

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

  print("Barrfile ended");
```

- `project` is used to generate the output binary path. 
- `cflags` and `cflags_release` are automatically selected based on the `build_type`.  
- `defines` and `includes` override the defaults and passed to the compiler.  


