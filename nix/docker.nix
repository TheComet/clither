{ pkgs, settings }:
let
  clither-server = (import ./server.nix { inherit pkgs settings; });
in
pkgs.dockerTools.buildImage {
  name = "clither";
  tag = "latest";
  copyToRoot = [ clither-server ];
  config = {
    Cmd = [ "${clither-server}/mechasnek" "--server" ];
    WorkingDir = "/";
    ExposedPorts = {
        "5555/tcp" = {};
        "5555/udp" = {};
    };
  };
}
