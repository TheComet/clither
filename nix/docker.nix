{ pkgs, settings }: let
  clither-server = (import ./server.nix { inherit pkgs; });
  settingsFileExists = builtins.readFileType settings == "regular";
in pkgs.dockerTools.buildImage {
  name = "clither";
  tag = "latest";

  copyToRoot = [
    clither-server
    pkgs.fakeNss
  ] ++ (pkgs.lib.optional settingsFileExists
    (pkgs.runCommand "mechasnek-settings" {} ''
      mkdir -p $out/var/empty/.local/share/mechasnek
      cp ${settings} $out/var/empty/.local/share/mechasnek/settings.ini
    ''));

  config = {
    Cmd = [ "${clither-server}/mechasnek" "--server" ];
    WorkingDir = "/";
    ExposedPorts = {
        "5555/tcp" = {};
        "5555/udp" = {};
    };
  };
}

