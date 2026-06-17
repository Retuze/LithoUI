# LithoUI

Embedded UI framework built on Arm-2D rendering core. Android-like window framework for MCU/embedded platforms.

## C++ Usage Rules

**Allowed:**
- `class` with virtual functions (single inheritance only)
- constructor / destructor
- `namespace`
- `enum class`
- member functions, `new` / `delete` (through custom allocator)

**Forbidden:**
- STL containers
- Exceptions
- RTTI
- Multiple inheritance
- Template metaprogramming
- Smart pointers

Every design must remain expressible in plain C without architecture changes — this is a design check, not a code comment.

## Build (PC / Linux)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./apps/hello_litho/hello_litho
```

Requires: cmake, g++/clang, libx11-dev, Arm-2D cloned alongside.

## Architecture

```
apps/          — application layer
framework/    — Activity, View tree, Window, Intent, Animation
core/          — C++ wrappers over Arm-2D C API
port/          — platform abstraction (X11 for PC dev)
third_party/   — Arm-2D Library (git submodule)
```
