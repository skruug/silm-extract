##  silm-extract

Tool to extract sprites, palettes and FLI videos from Silmarils games. At the moment only Big-endian games are supported (Amiga, Atari and Mac)
Bitmaps are exported as 2/8 or 32 bit PNG files, palettes as ACT files (useable directly in photoshop, among others) for FLI videos there is no conversion (you can use handbrake).

This tool require Maestun silm-depack to unpack script files, or already unpacked files.
For example Xfddecrunch on Amiga.

##  Usage

silm-extract <file> | <dir> [options]

Options:
  -h        This help info.
  -l        List all extractable assets.
  -o <dir>  Output directory.
  -p <file> Palette override.
  -f        Force 32 bit depth for all sprites.
