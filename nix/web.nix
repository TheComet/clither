{ pkgs }:
pkgs.emscriptenStdenv.mkDerivation {
  name = "clither";
  src = ../.;
  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
    git
  ];
  buildInputs = with pkgs; [
    freetype
    harfbuzz
  ];
  configurePhase = ''
    emcmake cmake --preset web
    '';
  buildPhase = ''
    cmake --build build-web-Release --parallel $(nproc)
    '';
}
