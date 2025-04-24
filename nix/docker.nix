{ pkgs }:
let
  clither-server = (import ./server.nix { inherit pkgs; });
in
pkgs.dockerTools.buildImage {
  name = "clither";
  tag = "latest";
  copyToRoot = [
    clither-server
    pkgs.bash
    pkgs.coreutils
  ];
  config = {
    Cmd = [ "${clither-server}/clither" "--server" ];
    WorkingDir = "/";
    ExposedPorts = {
        "5555/tcp" = {};
        "5555/udp" = {};
    };
  };
}
