# Contributing to Nothing Browser

Thanks for wanting to contribute. Here's what you need to know before you start.

## ⚠️ Do not touch the identity core

PRs that modify anything in the fingerprint spoofing, identity generation, or identity store will be **rejected without review**. This includes:

- `core/engine/FingerprintSpoofer.cpp`
- `core/engine/IdentityGenerator.cpp`
- `core/engine/IdentityStore.cpp`

This part of the codebase is intentionally controlled. If you think there's a genuine bug there, open an issue and describe it — don't send a PR.

## Building locally

```bash
# Install Qt6 + deps (Debian/Ubuntu)
sudo apt-get install qt6-base-dev qt6-webengine-dev libqt6webenginewidgets6 cmake ninja-build build-essential

# Build the JAR
cd newpipe-bridge && ./gradlew shadowJar --no-daemon && cd ..

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel --target nothing-browser nothing-browser-headless nothing-browser-headful
```

## What we welcome

- Bug fixes
- New tabs or UI features
- CI / build improvements
- Documentation
- Performance improvements outside the identity core

## Submitting a PR

- Keep it focused — one thing per PR
- Make sure it builds before opening it
- Fill out the PR template
- Be patient, this is a small team

## Questions

Open an issue or email **ernestteschhouse@gmail.com**