{ pkgs }:
pkgs.stdenv.mkDerivation {
  name = "clither-server";
  src = ../.;
  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
  ];
  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCLITHER_ASSETS=OFF"
    "-DCLITHER_BOT_API=OFF"
    "-DCLITHER_CLIENT=OFF"
    "-DCLITHER_DOC=OFF"
    "-DCLITHER_GFX=OFF"
    "-DCLITHER_TESTS=OFF"
  ];
}
