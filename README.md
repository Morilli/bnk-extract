# bnk-extract

Build depends on the libogg library, which is not included and must be installed manually (e.g. sudo apt install libogg-dev).

The libvorbis.a file that I included is here to allow static compilation. If it doesn't work on build, use an own one I guess.

Linux users should be able to build using a simple ``make``, whereas mingw is supported using ``make mingw``.
