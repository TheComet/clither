{
  description = "clither -- a better slither.io";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.11";
    clither-assets = {
      url = "github:thecomet/clither-assets";
      flake = false;
    };
    settings = {
      url = "path:.";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, clither-assets, settings }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      win64 = pkgs.pkgsCross.mingwW64;
      win32 = pkgs.pkgsCross.mingw32;
    in {
      packages.${system} = 
      let
        client = import ./nix/client.nix { inherit pkgs clither-assets; };
        docker = import ./nix/docker.nix { inherit pkgs; };
        server = import ./nix/server.nix { inherit pkgs settings; };
        web    = import ./nix/web.nix    { inherit pkgs; };

        client-win64 = import ./nix/client.nix { pkgs = win64; inherit clither-assets; };
        server-win64 = import ./nix/server.nix { pkgs = win64; inherit settings; };

        client-win32 = import ./nix/client.nix { pkgs = win32; inherit clither-assets; };
        server-win32 = import ./nix/server.nix { pkgs = win32; inherit settings; };
      in {
        inherit client docker server web;
        inherit client-win64 server-win64;
        inherit client-win32 server-win32;
        default = client;
      };
    };
}
