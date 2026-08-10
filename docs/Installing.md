Before installing AURORA, ensure all system level dependencies are installed:
![[Building#External Dependencies]]

Optional dependencies are not needed to install AURORA as tests are not installed to the system.
To install AURORA, first read the [[Building#Build Options|build configuration options]].
Then, utilize CMake's Install framework to specify the installation directory.

1. Generate the CMake Configuration
```sh
cmake \
	-S . \
	-DCMAKE_TOOLCHAIN_FILE=cmake/host-linux-gnu-toolchain.cmake \
	-B/tmp/aurora_build \
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
	-DBUILD_TESTING=OFF \
	-DACR_MAX_WORKERS=16 \
	-DAIM_MAX_WORKERS=256 \
	-DCMAKE_INSTALL_PREFIX=~/.local/ \
	-G "Unix Makefiles"
```

2. Execute the Build
```sh
cmake --build /tmp/aurora_build
```

3. Install
```sh
cmake --install /tmp/aurora_build
```

## Build Artifacts:
### Binaries
- `aurora_remote_engine` - AURORA's back-end executable. See [[Server Docs/Running ARE|Running ARE]]
### Includes
- `aul/aul.h` - Main AURORA User Library Include
- `aul/aul_configuration.h` - Configuration structures used for the AUL
- `aroc/aci` - AURORA Common Lib Connection Interface Headers
- `aroc/arm` - AURORA Common Lib Region Management Headers
- `aroc/afv` - AURORA Common Lib File Versioning Headers
- `aroc/acn` - AURORA Common Lib  Completion Notifier Headers
- `aroc/ads` - AURORA Common Lib Discovery Service Headers
- `aroc/log.h` - AURORA Common Lib LogLog Library Header
### Libraries
- `libaul.so` - The AURORA User Library (typically included in applications)
- `libaroc.a` - The AURORA Remote Offload Common library (internal dependency)
### CMake Targets
- `AURORA` - The main AURORA target
	- `AURORA::aul` - The User Library
	- `AURORA::aroc` - Common Library (typically not needed by applications)