{ pkgs }:
pkgs.emscriptenStdenv.mkDerivation {
  name = "clither";
  src = ../.;
  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
    #texliveFull
  ];
  buildInputs = with pkgs.emscriptenPackages; [
    zlib
  ];
  configurePhase = ''
    emcmake cmake -B build \
      -DCLITHER_ASM_OPTIMIZATIONS=OFF \
      -DCLITHER_FETCH_ASSETS=OFF \
      -DCLITHER_BOT_API=OFF \
      -DCLITHER_DOC=OFF \
      -DCLITHER_LOG=OFF \
      -DCLITHER_SERVER=OFF \
      -DCLITHER_TESTS=OFF
    '';
  buildPhase = ''
    cmake --build build --parallel $(nproc)
    '';
  installPhase = ''
    cmake --install build --prefix $out
    '';
}
