{ pkgs ? import <nixpkgs> {} }:

let
  top-level = import ../shell.nix { inherit pkgs; };

in pkgs.stdenv.mkDerivation {
  name = "hello-kernel-module";
  inherit (top-level) LINUX_DEV;

  src = ./.;
  phases = [ "unpackPhase" "buildPhase" "postBuildPhase" ];
  postBuildPhase = ''
    mkdir -p $out
    cp *.ko $out/
  '';
}
