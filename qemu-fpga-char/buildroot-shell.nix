{ pkgs ? import <nixpkgs> { }
}:

with pkgs;

(pkgs.buildFHSUserEnv {
  name = "buildroot-shell";
  targetPkgs = pkgs: (with pkgs; [
    binutils coreutils-full
    gcc perl
    python39 python39Packages.tkinter
    wget curl
    autoconf automake pkg-config
    file which flock
    rsync cpio
    bc
    readline62
    ncurses libarchive openssl
    unzip xz.dev lzma.dev bzip2.dev

    breezy # Bazaar replacement
    cvs git mercurial rsync subversion
    jdk
  ]);
}).env
