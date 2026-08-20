# Third-party software

This directory contains third-party software needed to build N Air Substream.
The licenses described here apply to the respective third-party components,
not automatically to the N Air Substream project as a whole.

## nlohmann/json

- File: `json.hpp`
- Version: 3.11.3
- Project: https://github.com/nlohmann/json
- License: MIT
- License text: `LICENSES/nlohmann-json-MIT.txt`

## LibOBS SDK

- Directory: `obs-libs/`
- Version: 31.1.2sl19
- Compatibility: N Air `obs-studio-node` 0.26.28
- Repository: https://github.com/streamlabs/obs-studio
- SDK archive: https://obsstudios3.streamlabs.com/libobs-windows64-release-31.1.2sl19.7z
- Source: https://github.com/streamlabs/obs-studio/tree/31.1.2sl19
- Source commit: `14cd01f72cb2a4e6b4e0e8d6da80533b44fd900d`
- License: GNU General Public License, version 2 or later
- License text: `LICENSES/libobs-GPL-2.0.txt`

The bundled SDK is the N Air-compatible LibOBS build identified in
`obs-libs/VERSION`. Its headers and Windows x64 import library must be updated
together to preserve ABI compatibility. Individual bundled headers may contain
code under additional compatible licenses; retain all copyright, SPDX, and
license notices contained in those files when redistributing them.

### pthreads-win32 headers

The LibOBS SDK includes `pthread.h` and `sched.h` from pthreads-win32 2.9.1.
Those headers state that they are licensed under the GNU Lesser General Public
License, version 2 or (at your option) any later version.

- Project: https://sourceware.org/pthreads-win32/
- License: GNU Lesser General Public License, version 2 or later
- License text: `LICENSES/pthreads-win32-LGPL-2.1.txt`
