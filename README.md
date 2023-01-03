##  silm-extract

Tool to extract sprites, palettes, sounds and FLI videos from Silmarils games. At the moment only Big-endian games are supported (Amiga, Atari and Mac)
Bitmaps are exported as 2/8 or 32 bit PNG files, palettes as ACT files (useable directly in photoshop, among others) for FLI videos there is no conversion (you can use handbrake).

This tool require Maestun silm-depack to unpack script files, or already unpacked files.
For example Xfddecrunch on Amiga.

##  Compiling from source

At the moment the only method to get it running. :-)
Clone repo with submodules: git clone --recurse-submodules https://github.com/skruug/silm-extract.git
If you are mac user, just use xcode project. If you are not, you have to create makefile yourself.
Link with libpng.

##  Usage
```shell
silm-extract <file> | <dir> [options]

Options:
  -h            This help info.
  -l            List all extractable assets.
  -t <options>  Specify types of data to extract.
                ( all | img | pal | cmp | snd )
  -o <dir>      Output directory.
  -p <file>     Palette override.
  -f            Force 32 bit depth for all sprites.
```
