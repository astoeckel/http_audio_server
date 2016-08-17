# HTTP Audio Streaming Server

This repository contains a proof-of-concept audio streaming server which gaplessly streams on-the-fly transcoded Opus audio to a web browser and plays it back using the Media Source Extensions (MSE).

## Features
* **C++ application** which streams audio files via HTTP REST API to a web application
* **Multiples files per stream** (playlist) with gapless playback
* **FFmpeg** used to decodes input files (no compile-time dependency)
* **Opus and WebM** handled by the media source extensions MSE. Encoding and packing is done using low-level libraries directly in the C++ code.
* **Adaptive bitrate** allows to dynamically change the bitrate during streaming (not yet implemented in the JavaScript client)

## How to build

This project requires a C++14 compliant compiler, such as GCC 6. Additional compile-time dependencies are `libopus`. On Fedora, you can install `libopus` using
```
sudo dnf install libopus-devel
```
Alternatively compile a recent version from a stable source-code release of `libopus`, which can be found at the [Opus Codec Homepage](https://opus-codec.org/downloads/).

All other dependencies ([libwebm](https://github.com/webmproject/libwebm)) are included as Git submodule or directly stored in the repository ([json](https://github.com/nlohmann/json), [mongoose](https://github.com/cesanta/mongoose/)). As runtime dependency, an installation of `ffmpeg` is required. `ffmpeg` is automatically spawned as a background process to decode audio files.

Build `http_audio_server` using
```bash
git clone https://github.com/astoeckel/http_audio_server --recursive
cd http_audio_server && mkdir build && cd build
cmake ..
make
```
and start the server with
```bash
./http_audio_server
```
You are now ready to go to [http://localhost:4851/](http://localhost:4851/) and follow the on-screen instructions.

## License

**HTTP Streaming Audio Server – Copyright (C) 2016  Andreas Stöckel**

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or  (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.  You should have received a copy of the GNU Affero General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.

