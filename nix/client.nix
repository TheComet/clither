{ pkgs, clither-assets }:
pkgs.stdenv.mkDerivation {
  name = "clither";
  src = ../.;

  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
  ] ++ lib.optional (pkgs.stdenv.isLinux) [
    texliveFull
    makeWrapper
  ];

  buildInputs = with pkgs; [
    freetype
    glfw3
  ] ++ lib.optional (pkgs.stdenv.isLinux) [
    # On windows we use the included version of harfbuzz because nix pulls in
    # the ENTIRE gtk here
    harfbuzz
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCLITHER_ASSETS=OFF"  # Assets come in from flake.nix
    "-DCLITHER_TESTS=OFF"
  ];

  postInstall = ''
    cp -r ${clither-assets}/packs $out/packs
    '';

  postFixup = let
    lib = pkgs.lib;
    isWindows = builtins.match ".*-windows" pkgs.stdenv.hostPlatform.system != null;
  in
  lib.optional(pkgs.stdenv.isLinux) ''
      # Hack so GLFW finds the X11 system libraries
      wrapProgram $out/clither \
        --set LD_LIBRARY_PATH "/usr/lib64"
    '' ++
  lib.optional (isWindows) ''
      # Windows DLLs that are not system DLLs
      cp ${pkgs.freetype}/bin/*freetype*.dll $out/
      cp ${pkgs.windows.mcfgthreads}/bin/*mcfgthread*.dll $out/
      cp ${pkgs.libpng}/bin/*png*.dll $out/
      cp ${pkgs.brotli}/bin/*brotli*.dll $out/
      cp ${pkgs.bzip2}/bin/*bz2*.dll $out/
      cp ${pkgs.zlib}/bin/*zlib*.dll $out/
    '';
}
