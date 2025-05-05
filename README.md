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
  + Make a new directory called *build*.
  + cd into that directory with your command prompt/terminal/whatever.
  + Type ```cmake ..``` (Two dots are important)

This will configure the project for your platform. Linux/Mac users now type *make* to build.

If you're on Windows, there will now be Visual Studio project files, inside the *build*
directory. So go in there and open them up.

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
cmake --build build-web-Release/ --paralell $(nproc)
```

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

