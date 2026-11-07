# Pd-else – Build commands (macOS)

## 1. Go to the repository

```bash
cd ~/Desktop/pd-else
```

## 2. Initial configuration (only once)

```bash
cmake -S . -B Build \
  -DPD_SOURCES_PATH="/Applications/Pd-0.57-0.app/Contents/Resources/src" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
```

This creates the `Build` folder and saves the configuration, including the Pd headers path.

## 3. Normal build (after making changes)

```bash
cmake --build Build -j7
```

No need to reconfigure every time.

## 4. Clean rebuild (recompile everything)

```bash
cmake --build Build --target clean
cmake --build Build -j7
```

Use this when you want to remove previous compiled object files and rebuild from scratch.

## 5. Complete reset (delete Build and configure again)

```bash
rm -rf Build

cmake -S . -B Build \
  -DPD_SOURCES_PATH="/Applications/Pd-0.57-0.app/Contents/Resources/src" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"

cmake --build Build -j7
```

Use this if you want to regenerate the entire build configuration.

## 6. Output location

The compiled ELSE externals are placed in:

```text
Build/else/
```

## Quick reference

| Situation | Command |
|---|---|
| Normal build | `cmake --build Build -j7` |
| Clean compiled files | `cmake --build Build --target clean` |
| Clean and rebuild | Run clean, then build |
| Full reset | Delete Build, configure, then build |

**Remember:** For everyday development, you only need:

```bash
cd ~/Desktop/pd-else
cmake --build Build -j7
```