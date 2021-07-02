{ pkgs ? import <nixpkgs> {} }:

with pkgs;
with pkgs.lib;

stdenv.mkDerivation rec {
  name = "qemu-virtine-fpga";

  nativeBuildInputs = [ python3 pkg-config flex bison meson ninja ];

  buildInputs = [
    zlib glib perl pixman vde2 texinfo makeWrapper lzo snappy libtasn1 git
    gnutls nettle curl ncurses libseccomp numactl libpulseaudio SDL2 SDL2_image
    gtk3 gettext vte libjpeg libpng libcacard spice-protocol spice usbredir
    alsaLib libaio libcap_ng libcap attr xen ceph glusterfs libuuid mesa epoxy
    libdrm virglrenderer libiscsi samba
  ];

  configureFlags = lib.concatStringsSep " " [
    "--enable-tools"
    "--enable-guest-agent"
    "--enable-numa"
    "--enable-seccomp"
    "--enable-smartcard"
    "--enable-spice"
    "--enable-usb-redir"
    "--enable-linux-aio"
    "--enable-gtk"
    "--enable-xen"
    "--enable-rbd"
    "--enable-glusterfs"
    "--enable-opengl"
    "--enable-virglrenderer"
    "--enable-tpm"
    "--enable-libiscsi"
    "--smbd=${samba}/bin/smbd"
    # Karl-added config flags
    "--enable-system"
    "--enable-user"
    "--enable-linux-user"
    "--enable-bsd-user"
    "--enable-pie"
    "--enable-modules"
    "--enable-debug-info"
    "--enable-hax"
  ];

  hardeningDisable = [ "all" ];
}

  # Configure commands
  # mkdir out build
  # cd build
