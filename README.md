# Clither
[slither.io](http://slither.io/) but not shit. Rollback netcode. No lag.

A cross-platform multi-player 2D arcade game implemented in plain C89.

## How to Build

The tools you will need to build this project are:
  + [CMake](http://www.cmake.org/).
  + A C89 compliant C compiler.
  + [Make](http://www.gnu.org/software/make/) (If you're on Mac/linux).
  + [Git](http://git-scm.com/) (if you want to make to make updating the code easy).

**Windows MSVC Developers** should start the "Visual Studio Command Prompt" from the
start menu instead of CMD. It sets the required environment variables. If you are
using MSYS then you can use regular CMD (or bash).

The general procedure is as follows:
  + cd into the source directory of clither (where this README is located).
  + Type ```cmake --preset client```

If you're on Windows, there will now be Visual Studio project files, inside the *build-client*
directory. So go in there and open them up.

Mac/Linux users can type ```cmake --build build-client --parallel $(nproc)``` to compile the project.

## Emscripten

Install [emscripten](https://emscripten.org/docs/getting_started/downloads.html#installation-instructions):
```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
git pull
./emsdk update-tags
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Next, configure:
```sh
cd path/to/clither
emcmake cmake --preset web
cmake --build build-web/ --parallel $(nproc)
```

To run the client, you need to serve  the  files  in  the  bin/  directory. For
example, using python:
```sh
cd build-web-Release/bin
python3 -m http.server 8000
```

The client  will  try to connect to a dedicated clither server. By default, the
port will be 5555. This means for local testing, you will also  need to start a
server.
```sh
cmake --preset server
cmake --build build-server --parallel $(nproc)
cd build-server/bin
./mechasnek --server
```

Then open your browser and go to [http://localhost:8000/mechasnek.html](http://localhost:8000/mechasnek.html).

## Nix

Nix is the easiest for building the server and cross compiling to windows: [https://nixos.org/download/](https://nixos.org/download/).

You can install nix locally (--no-daemon). If you are worried  about cluttering
your system with unnecessary files, don't worry. Everyting is contained  within
the ```/nix``` directory.

Once installed, you will also  want  to  create ```/etc/nix/nix.conf``` and add
the following lines:
```
extra-experimental-features = nix-command flakes
```

### Build server as a docker image

```sh
nix build .#docker
docker load < result
docker run clither:latest
```

To include a custom settings.ini file:

```sh
nix build .#docker --override-input settings path:./path/to/settings.ini
docker load < result
docker run clither:latest
```

### Cross-compile to windows

```sh
nix build .#client-win64
zip -r clither-win64.zip result/
```

## Usage

There are 3 different modes:
  + **Host mode:** ```./clither --host``` will create a server process in the background, then launch the client and join your local server. Other players on your network will be able to join your server if they want.
  + **Server mode:** ```./clither --server``` will only run the server component. No window will open.
  + **Client mode:** ```./clither --ip <server ip>``` will only run the client component and join the specified server.

