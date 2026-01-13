# bnk-extract

Build depends on the libogg and libvorbis libraries, which are not included and must be installed manually (e.g. sudo apt install libogg-dev libvorbis-dev).

Shoutouts to the original creators of [ww2ogg](https://github.com/hcs64/ww2ogg) and [revorb](https://github.com/jonboydell/revorb-nix), which I use in a modified version for this program (they are included in their respective subfolders).


## Building the Source

Linux systems and mingw should be able to build out-of-the-box using a simple ``make`` (after installing the needed packages). If the compilation fails, try compiling dynamically instead of statically (I've had troubles with the static libvorbis package on linux).


## Installing the Source (Linux)

After building the source, linux systems can install the `bnk-extract` executable by running `make install`.

The default installation `prefix` is `/usr/local`.
The `DESTDIR` override is supported, per GNU standards.

```bash
make install
```
