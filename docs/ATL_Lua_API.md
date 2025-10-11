# Atlas Build Manager Lua Fields / DSL Reference 
This document describes all Lua functions and fields exposed by Atlas Build Manager (ATL) to Lua scripts.

## Global Structure
### All fields are accessible via the `atl` global table:
```lua
  atl.<module>.<function>
```

- Example: `local sys_info = atl.os.info()`

### OS Module
`atl.os.info()` - Returns a table with system information.  

| Field    | Type     | Description |
|----------|----------|-------------|
| name     | string   | OS name(Linux, Windows, etc)|
| version  | string   | OS or kernel version |
| machine  | string   | Architecture (x86_64, arm, etc.) |
| cores  | integer   | Number of CPU cores |
| cache_line_size  | integer   | CPU cache line in bytes |
| little_endian  | boolean   | `true` if little-endian |
| big_endian  | boolean   | `true` if big-endian |
| hostname  | string   | Hostname of the machine |

- Example:
```lua
   local info = atl.os.info()
   print(info.name, info.cores)
```

### Environment Module

| Function | Description |
|----------|-------------|
| atl.env.get(key) | Get environment variable `key` |
| atl.env.set(key, value) | Set environment variable `key` to `value` |
| atl.env.all() | Returns a table of all environment variables |

Example: 
```lua
  print(atl.env.get("PATH")
  atl.env.set("MY_VAR",42)
```

### Source Files / List Module
`atl.list.source_files([dir])` - Returns a Lua table of source files in the specific directory.  
If no directory is specified, uses the current working directory.  

Example:
```lua
   local files = atl.list.source_files("src")
   for i, f in pairs(files) do
       print(f)
   end
```

### Project Module

Projects are represented as a container in C and Lua. You can create and retrieve projects.

#### Create a project 
```lua
   local src_files = atl.list.source_files("src")

   project_id = atl.project.create {
       name = "my_project",
       root_dir = ".",
       build_type = "release",
       compile_flags = "-Wall -Werror -O2",
       source_files = src_files
   }
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| name  | string | `"atlas_project"` | Project name |
| root_dir  | string | Current working directory | Project root directory |
| build_type  | string | `"debug"` | Build type (`debug` or `release`, etc) |
| compile_flags  | string | `"-Wall -Wextra -Werror -g -DDEBUG"` | Compiler flags  |
| source_files  | table | {} | Lua table reference to source files |

#### Retrieve a project 
```lua
   local p = atl.project.get("my_project")
   print(p.build_type)
   print(p.source_files[1])
```

### Notes
- Multiple projects can coexist in a single Lua script "at least in theory".  
- Source files are passed as **table references**, allowing C code to access Lua-generated lists efficiently.  
- Windows support is minimal in this version.
- ATL version: `0.7.2`
- “Fields and functions are subject to change in future ATL releases.”
