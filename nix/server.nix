{ pkgs, settings }:
pkgs.stdenv.mkDerivation {
  name = "clither-server";
  src = ../.;
  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
  ];
  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCLITHER_FETCH_ASSETS=OFF"
    "-DCLITHER_BOT_API=OFF"
    "-DCLITHER_CLIENT=OFF"
    "-DCLITHER_DOC=OFF"
    "-DCLITHER_GFX=OFF"
    "-DCLITHER_HOT_RELOAD=OFF"
    "-DCLITHER_TESTS=OFF"
  ];
  postInstall = let
    settingsFileExists = builtins.readFileType settings == "regular";
  in pkgs.lib.optional (settingsFileExists) ''
    cp ${settings} $out/settings.ini
    '';
  postFixup = let
    isWindows = builtins.match ".*-windows" pkgs.stdenv.hostPlatform.system != null;
  in pkgs.lib.optional (isWindows) ''
    # Windows DLLs that are not system DLLs
    cp ${pkgs.windows.mcfgthreads}/bin/*mcfgthread*.dll $out/
    '';
}
