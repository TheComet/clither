{ pkgs, settings }:
let
  clither-server = (import ./server.nix { inherit pkgs settings; });
in
pkgs.dockerTools.buildImage {
  name = "clither";
  tag = "latest";
  copyToRoot = [
    clither-server
    pkgs.fakeNss  # fs_appdata_dir() calls getpwuid(), which requires /etc/passwd
  ];
  config = {
    Cmd = [ "${clither-server}/mechasnek" "--server" ];
    WorkingDir = "/";
    ExposedPorts = {
        "5555/tcp" = {};
        "5555/udp" = {};
    };
  };
}
