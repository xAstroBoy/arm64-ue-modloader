# Contributing to Quest UE4 Modloader

Thank you for your interest in contributing! This guide will help you get started.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Mod Contributions](#mod-contributions)

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## How Can I Contribute?

### 🐛 Bug Reports
- Use the [Bug Report](https://github.com/xAstroBoy/quest-ue4-modloader/issues/new?template=bug_report.yml) issue template
- Include your Quest model, game version, and modloader version
- Attach relevant logs (`UEModLoader.log`) and crash dumps (tombstones)
- Describe steps to reproduce

### ✨ Feature Requests
- Use the [Feature Request](https://github.com/xAstroBoy/quest-ue4-modloader/issues/new?template=feature_request.yml) template
- Explain the use case and why it benefits the community

### 🔧 Code Contributions
- **Modloader core** (C++) — new Lua bindings, hook improvements, performance
- **Lua mods** — new mods or improvements to existing ones
- **Tools** — deploy script improvements, new utilities
- **Documentation** — wiki pages, API docs, examples

### 📝 Documentation
- Wiki improvements and new pages
- Code comments and docstrings
- Example mods with detailed explanations

## Development Setup

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| Android NDK | r23c (23.1.7779620) | ARM64 cross-compilation |
| CMake | 3.22+ | Build system |
| Ninja | latest | Build executor |
| Python | 3.8+ | Deploy tools |
| ADB | latest | Device communication |
| Meta Quest | 2/3/Pro | Target device (rooted) |

### Building the Modloader

```bash
# Clone the repo
git clone --recursive https://github.com/xAstroBoy/quest-ue4-modloader.git
cd quest-ue4-modloader

# Set your NDK path
export NDK=/path/to/android/ndk/23.1.7779620  # Linux/macOS
set NDK=C:\Android\ndk\23.1.7779620            # Windows

# Build
cd modloader
./build.sh    # Linux/macOS
.\build.bat   # Windows
```

### Testing Changes

```bash
# Push to device
python tools/deploy.py all
python tools/deploy.py launch

# Check logs
python tools/deploy.py log

# Interactive testing
python tools/deploy.py console
> exec_lua return "hello"
```

## Coding Standards

### C++ (modloader core)

- **Standard**: C++17
- **Style**: K&R braces, 4-space indent
- **Naming**: `snake_case` for functions/variables, `PascalCase` for classes
- **Headers**: Include guards (`#pragma once`)
- **Memory**: No raw `new`/`delete` — use RAII, `std::unique_ptr`, or stack allocation
- **Error handling**: No exceptions (`-fno-exceptions`). Use return codes and null checks.
- **Logging**: Use `LOG_INFO`/`LOG_WARN`/`LOG_ERROR` macros
- All UObject access via reflection API (never raw pointer offsets for game objects)

### Lua (mods)

- **Style**: 4-space indent, `snake_case` for locals, `PascalCase` for API functions
- **Safety**: Always wrap UObject calls in `pcall()`
- **Hooks**: Use separate `pcall` blocks for independent operations
- **Properties**: Use `obj:Get()`/`obj:Set()` — never raw memory for UObject fields
- **TArray**: Remember 1-based indexing
- **Documentation**: Header comment with version, description, changelog

### Python (tools)

- **Standard**: Python 3.8+
- **Style**: PEP 8, 4-space indent
- **Dependencies**: Minimal (stdlib only for deploy tools)

## Submitting Changes

### Pull Request Process

1. **Fork** the repository
2. **Create a branch** from `main`:
   ```bash
   git checkout -b feature/my-feature
   # or
   git checkout -b fix/my-bugfix
   ```
3. **Make your changes** following the coding standards
4. **Test** on a real Quest device if possible
5. **Commit** with clear, descriptive messages:
   ```
   feat: add FVector math operators to LuaUStruct
   fix: crash in ProcessEvent hook when UObject is GC'd
   docs: add TArray usage examples to wiki
   mod: add InfiniteStamina mod example
   ```
6. **Push** and open a Pull Request against `main`
7. Fill out the PR template completely

### Commit Message Format

```
<type>: <short description>

[optional body with more detail]

[optional footer with breaking changes or issue refs]
```

Types: `feat`, `fix`, `docs`, `mod`, `tools`, `refactor`, `test`, `ci`, `chore`

## Mod Contributions

### Submitting a New Mod

1. Create your mod in `mods/<ModName>/main.lua`
2. Include a descriptive header comment with:
   - Version number
   - What the mod does
   - How it works (hooks used, etc.)
3. Use `ModConfig` for any persistent settings
4. Register with `SharedAPI.DebugMenu` if the mod has toggleable features
5. Test thoroughly on device
6. Submit a PR with:
   - The mod files
   - A brief description in the PR
   - Any PAK files needed (keep them small)

### Mod Quality Checklist

- [ ] All UObject access uses reflection (`Get`/`Set`/`Call`)
- [ ] Critical calls wrapped in `pcall`
- [ ] Separate `pcall` blocks for independent operations
- [ ] `ModConfig` used for persistent settings
- [ ] Debug menu integration (if applicable)
- [ ] Clean logging with mod name tag
- [ ] No hardcoded memory offsets for game objects
- [ ] Tested on device

---

Thank you for contributing! 🎮
