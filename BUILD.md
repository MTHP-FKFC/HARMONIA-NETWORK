# 🏗️ Building Cohera Saturator

This project supports two primary build modes: **Development** and **Release**.

## 🚀 Release Build (Production)

Use this for creating distributable binaries (VST3, AU, Standalone). This mode:
- Enables optimizations (`-O3`, `-DNDEBUG`)
- **Disables** all test suites and helper executables
- Creates a clean `build_release` directory

```bash
./build_release.sh
```

**Artifacts Location:**
- VST3: `build_release/Cohera_Saturator_artefacts/VST3/`
- AU: `build_release/Cohera_Saturator_artefacts/AU/`
- App: `build_release/Cohera_Saturator_artefacts/Standalone/`

---

## 🛠️ Development Build (Debug)

Use this for development, debugging, and running tests. This mode:
- Enables debug symbols (`-g`)
- **Enables** all test suites (`StressTest`, `TestAudioRegression`, etc.)
- Uses the `build` directory

```bash
./build_dev.sh
```

**Artifacts Location:**
- Plugins: `build/Cohera_Saturator_artefacts/`
- Tests: `build/tests/`

---

## ⚙️ Manual CMake Configuration

You can also configure CMake manually:

**Release (No Tests):**
```bash
cmake -B build_release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build build_release --target Cohera_Saturator -j8
```

**Development (With Tests):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --target all -j8
```
