{ pkgs }:
pkgs.stdenv.mkDerivation {
  name = "clither";
  src = ../.;
  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
    git
  ];
  buildInputs = with pkgs; [
    freetype
    harfbuzz
    glfw
  ];
  configurePhase = ''
    cmake --preset client
    '';
  buildPhase = ''
    cmake --build build-Release
    '';
}
