#!/bin/bash
set -e

echo "=== Step 01: Bootstrapping vcpkg ==="

# Clone vcpkg if not present
if [ ! -d "vcpkg" ]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/Microsoft/vcpkg.git
fi

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
./vcpkg/bootstrap-vcpkg.sh

echo ""
echo "=== vcpkg Ready ==="
echo ""
echo "Next steps:"
echo "  cmake --preset=default"
echo "  cmake --build build"
echo "  ./build/step01_demo"
echo ""
