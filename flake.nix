{
  description = "clither -- a better slither.io";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.11";
    clither-assets = {
      url = "github:thecomet/clither-assets";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, clither-assets }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      win64 = pkgs.pkgsCross.mingwW64;
    in {
      packages.${system} = 
      let
        client = import ./nix/client.nix { inherit pkgs clither-assets; };
        docker = import ./nix/docker.nix { inherit pkgs; };
        server = import ./nix/server.nix { inherit pkgs; };
        web    = import ./nix/web.nix    { inherit pkgs; };

        client-win64 = import ./nix/client.nix { pkgs = win64; inherit clither-assets; };
        server-win64 = import ./nix/server.nix { pkgs = win64; };
      in {
        inherit client docker server web;
        inherit client-win64 server-win64;
        default = client;
      };
    };
}
