{ pkgs, clither-assets }: let
  isWindows = builtins.match ".*-windows" pkgs.stdenv.hostPlatform.system != null;
in pkgs.stdenv.mkDerivation {
  name = "clither";
  src = ../.;

  nativeBuildInputs = with pkgs.buildPackages; [
    cmake
  ] ++ lib.optional pkgs.stdenv.isLinux [
    texliveFull
    makeWrapper
  ];

  buildInputs = with pkgs; [
    freetype
    glfw3
  ] ++ lib.optional pkgs.stdenv.isLinux [
    # On windows we use the included version of harfbuzz because nix pulls in
    # the ENTIRE gtk here
    harfbuzz
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DCLITHER_FETCH_ASSETS=OFF"  # Assets come in from flake.nix
    "-DCLITHER_TESTS=OFF"
  ] ++ pkgs.lib.optional pkgs.stdenv.is32bit [
    "-DCLITHER_ASM_OPTIMIZATIONS=OFF"
  ];

  postInstall = ''
    cp -r ${clither-assets}/packs $out/packs
    '';

  postFixup =
    pkgs.lib.optional pkgs.stdenv.isLinux ''
      # Hack so GLFW finds the X11 system libraries
      wrapProgram $out/mechasnek \
        --set LD_LIBRARY_PATH "/usr/lib64"
      '' ++
    pkgs.lib.optional isWindows ''
      # Windows DLLs that are not system DLLs
      cp -u ${pkgs.freetype}/bin/*freetype*.dll $out/
      cp -u ${pkgs.windows.mcfgthreads}/bin/*mcfgthread*.dll $out/
      cp -u ${pkgs.libpng}/bin/*png*.dll $out/
      cp -u ${pkgs.brotli}/bin/*brotli*.dll $out/
      cp -u ${pkgs.bzip2}/bin/*bz2*.dll $out/
      cp -u ${pkgs.zlib}/bin/*zlib*.dll $out/
      cp -u ${pkgs.stdenv.cc.cc.lib}/lib/libgcc_s_seh-1.dll $out/
      cp -u ${pkgs.stdenv.cc.cc.lib}/lib/libstdc++-6.dll $out/
      '';

  shellHook = pkgs.lib.concatStringsSep "\n" (
    pkgs.lib.optional isWindows ''
      # Windows DLLs that are not system DLLs
      cp -u ${pkgs.freetype}/bin/*freetype*.dll build-win64/bin/
      cp -u ${pkgs.windows.mcfgthreads}/bin/*mcfgthread*.dll build-win64/bin/
      cp -u ${pkgs.libpng}/bin/*png*.dll build-win64/bin/
      cp -u ${pkgs.brotli}/bin/*brotli*.dll build-win64/bin/
      cp -u ${pkgs.bzip2}/bin/*bz2*.dll build-win64/bin/
      cp -u ${pkgs.zlib}/bin/*zlib*.dll build-win64/bin/
      cp -u ${pkgs.stdenv.cc.cc.lib}/lib/libgcc_s_seh-1.dll build-win64/bin/
      cp -u ${pkgs.stdenv.cc.cc.lib}/lib/libstdc++-6.dll build-win64/bin/
      cmake -DCMAKE_SYSTEM_NAME=Windows -B build-win64 --preset client
      echo "You can build the client with 'cmake --build build-win64 --parallel $(nproc)'"
      ''
    );
}
