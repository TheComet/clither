{
  description = "clither -- a better slither.io";
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      packages.${system} = 
      let
        web = (import ./nix/web.nix { inherit pkgs; });
        client = (import ./nix/client.nix { inherit pkgs; });
        server = (import ./nix/server.nix { inherit pkgs; });
        docker = (import ./nix/docker.nix { inherit pkgs; });
      in {
        inherit web;
        inherit client;
        inherit server;
        inherit docker;
        default = server;
      };
    };
}
