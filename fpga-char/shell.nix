{ pkgs ? import <nixpkgs> {} }:

pkgs.stdenv.mkDerivation {
  name = "linux-pcie-dma-practice";
  hardeningDisable = [ "all" ];

  nativeBuildInputs = with pkgs; [
    linuxHeaders # Linux kernel headers
    linux.dev

    kmod # Load and manage kernel modules
    # keep this line if you use bash
    bashInteractive
  ];

  # Export the development tools the kernel makes available to the subshell.
  # That way, scripts and the like can find the location of the development tools.
  LINUX_DEV = pkgs.linux.dev;
}
