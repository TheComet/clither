{ pkgs }:
pkgs.dockerTools.buildImage {
  name = "clither";
  tag = "latest";
  copyToRoot = [
    (import ./server.nix { inherit pkgs; })
  ];
  config = {
    Cmd = [ "${pkgs.stdenv.cc}/bin/bash" "-c" "echo 'Hello, world!'" ];
    WorkingDir = "/app";
  };
}
