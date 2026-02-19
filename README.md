# Build Barrage (barr)

Build Barrage is developer-centric build orchestrator for Linux.  
It is designed to be as lean as possible, transparent and fast, featuring its own  
internal memory management, content addressable caching and other systems.   

# Quick Start

```bash
git clone https://github.com/vsix8625/Build_Barrage.git
cd Build_Barrage
./scripts/build.sh
```

## What does the script do?

The `build.sh` a Canonical Bootstrap:
- Stage 0: It uses `cmake` with `ninja` or `make` if not available,  
  to build a minimal version of `barr`.  
- Stage 1: This (seed) binary then executes the projects native `Barrfile`.  
  It uses `barr` own logic to compile the full optimized version of itself.  
- Stage 2: Wipes the temporary `CMake` directory leaving you with a standalone binary, 
  in `./build/release/bin/barr`.   
