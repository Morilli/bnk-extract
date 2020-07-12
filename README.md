# bnk-extract

Build depends on the libogg library, which is not included and must be installed manually (e.g. sudo apt install libogg-dev).

Shoutouts to the original creators of [ww2ogg](https://github.com/hcs64/ww2ogg) and [revorb](https://github.com/jonboydell/revorb-nix), which I use in a modified version for this program (they are included in their respective subfolders).

The libvorbis.a file that I included is here to allow static compilation. If it doesn't work on build, use an own one I guess.

Linux users should be able to build using a simple ``make``, whereas mingw is supported using ``make mingw``.
