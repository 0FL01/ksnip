# Build and delivery contract

## Baseline build

Prefer Ninja and submodule dependencies to minimize host-version mismatch:

```bash
git submodule update --init --recursive

cmake -S . -B build-agent -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_WITH_QT6=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON

cmake --build build-agent --parallel
```

If system libraries are intentionally used instead, record their exact package
versions in `dist/build-info.txt`.

Do not install missing packages silently. Print the exact Fedora `dnf install`
or `dnf builddep` command required, then continue after dependencies exist.

## Test build

When the existing test configuration is viable:

```bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_WITH_QT6=ON \
  -DBUILD_TESTS=ON \
  -DUSE_SUBMODULE_KCOLORPICKER=ON \
  -DUSE_SUBMODULE_KIMAGEANNOTATOR=ON

cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

Do not spend the iteration repairing unrelated ancient tests.

## D-Bus probes

Useful non-mutating checks:

```bash
busctl --user get-property \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2 \
  org.kde.KWin.ScreenShot2 \
  Version

busctl --user introspect \
  org.kde.KWin.ScreenShot2 \
  /org/kde/KWin/ScreenShot2

busctl --user get-property \
  org.freedesktop.portal.Desktop \
  /org/freedesktop/portal/desktop \
  org.freedesktop.portal.GlobalShortcuts \
  version
```

## Build metadata

`dist/build-info.txt` must include:

- UTC build date;
- repository URL;
- source commit SHA;
- dirty/clean status;
- compiler and version;
- CMake version;
- Qt version;
- kernel;
- Fedora release;
- Plasma version;
- session type;
- exact configure and build commands;
- binary path;
- `file` output;
- relevant `ldd` output.

## Patch

Produce:

```bash
git diff --binary <baseline-sha>...HEAD > dist/source.patch
```

If commits are not created, use the baseline working tree diff and state that
clearly.
