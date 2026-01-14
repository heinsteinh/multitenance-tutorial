# Step 01: vcpkg Setup and CMake Integration

This step establishes the foundation for dependency management using vcpkg with CMake.

## What You'll Learn

- Bootstrapping vcpkg as a submodule
- CMake presets for consistent builds
- vcpkg manifest mode (`vcpkg.json`)
- Toolchain integration

## Why vcpkg?

vcpkg provides:
- **Reproducible builds**: Same dependencies across all machines
- **Manifest mode**: Dependencies declared in `vcpkg.json`
- **CMake integration**: Seamless `find_package()` support
- **Cross-platform**: Windows, Linux, macOS

## Project Structure

```
step01-vcpkg-setup/
├── CMakeLists.txt           # Build configuration
├── CMakePresets.json        # Build presets
├── vcpkg.json               # Dependency manifest
├── bootstrap.sh             # Linux/macOS setup
├── bootstrap.bat            # Windows setup
├── src/
│   └── main.cpp             # Demo application
└── README.md                # This file
```

## Key Concepts

### 1. vcpkg Manifest Mode

The `vcpkg.json` file declares all dependencies:

```json
{
  "name": "multitenant-demo",
  "version": "1.0.0",
  "dependencies": [
    "spdlog",
    "nlohmann-json"
  ]
}
```

### 2. CMake Toolchain Integration

vcpkg provides a toolchain file that hooks into CMake's `find_package()`:

```cmake
# Set before project()
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
```

### 3. CMake Presets

Presets standardize build configurations:

```json
{
  "configurePresets": [{
    "name": "default",
    "binaryDir": "${sourceDir}/build",
    "cacheVariables": {
      "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    }
  }]
}
```

## Building

```bash
# Bootstrap vcpkg (first time only)
./bootstrap.sh

# Configure with preset
cmake --preset=default

# Build
cmake --build build

# Run
./build/step01_demo
```

## Code Walkthrough

### main.cpp

Demonstrates that vcpkg dependencies work:

```cpp
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

int main() {
    spdlog::info("vcpkg integration working!");

    nlohmann::json config = {
        {"database", "sqlite"},
        {"pool_size", 10}
    };

    spdlog::info("Config: {}", config.dump(2));
    return 0;
}
```

## Next Step

Step 02 adds SQLite and builds our database wrapper.
