//
//  extractor.cpp
//  silm-extract
//
//  Created on 08.08.2022.
//

#include "extractor.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>

#include <png.h>
#include "utils.hpp"

extern "C"
{
    #include "depack.h"
}

using std::cout; using std::cin;
using std::endl; using std::string;
using std::filesystem::directory_iterator;

// known ALIS extensions
const char *extensions[] = { "ao", "co", "do", "fo", "io", "mo" };

extractor::extractor(alis_platform platform)
{
    _platform = platform;
}

extractor::extractor(const path& output, char *palette, bool force_tc, bool list_only)
{
    memset(_pallette16, 0, 256 * 3);
    for (int i = 0; i < 16; i++)
    {
        _pallette16[i * 3 + 0] = i * 16;
        _pallette16[i * 3 + 1] = i * 16;
        _pallette16[i * 3 + 2] = i * 16;
    }

    for (int i = 0; i < 256; i++)
    {
        _pallette256[i * 3 + 0] = i;
        _pallette256[i * 3 + 1] = i;
        _pallette256[i * 3 + 2] = i;
    }

    _out_dir = output;
    _paletteOverride = (uint8_t *)palette;
    _force_tc = force_tc;
    _list_only = list_only;
}

extractor::~extractor()
{
    
}

bool extractor::is_script(const path& file)
{
    string e = utils::get_file_ext(file.string());
    transform(e.begin(), e.end(), e.begin(), ::tolower);
    for (const char * &v : extensions)
    {
        if (v == e)
            return true;;
    }

    return false;
}

void extractor::extract_dir(const path& path)
{
    for (const auto & file : directory_iterator(path))
    {
        if (is_script(file.path()))
            extract_file(file.path());
    }
}

void extractor::extract_file(const path& file)
{
    std::string name = utils::get_file_name(file.string(), false);
    cout << name << endl;

    std::ifstream is(file, std::ifstream::binary);
    if (is)
    {
        is.seekg (0, is.end);
        long length = is.tellg();
        is.seekg (0, is.beg);

        cout << "Reading " << std::dec << length << " bytes... " << endl;

        char *buffer = new char [length];
        is.read(buffer, length);
        is.close();

        int unpacked_size = 0;
        uint8_t *unpacked = NULL;
        if (depack_is_packed((uint8_t *)buffer))
        {
            unpacked_size = depack_get_size((uint8_t *)buffer);
            unpacked = depack_buffer((uint8_t *)buffer);
            if (unpacked)
            {
                extract_buffer(name, unpacked, unpacked_size);
                free(unpacked);
            }
        }
        else
        {
            // probably not gona to work, but what the hell :-)
            // it could only work on unpacked files
            
            unpacked_size = (int)length;
            unpacked = (uint8_t *)buffer;

            extract_buffer(name, unpacked, unpacked_size);
        }
        
        delete[] buffer;
    }
    
    cout << endl;
}

void log_bytes(const uint8_t *p, int i, int size)
{
    cout << "[";
    int f = i < 8 ? i : 8;
    int t = size - i < 8 ? size - i : 8;
    for (int l = 0; l < f; l++) { cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)p[i+l-f]; }
    cout << " ][";
    for (int l = 0; l < t; l++) { cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)p[i+l]; }
    cout << " ]" << endl;
}

void log_bytes_to(const uint8_t *p, int i, int t, int size)
{
    cout << "[";
    for (int l = 0; l < std::min(t, 24); l++) { cout << " " << std::hex << std::setw(2) << std::setfill('0') << (int)p[i+l]; }
    if (t < 24)
    {
        cout << " ]" << endl;
    }
    else
    {
        cout << " ..." << endl;
    }
}

void write_png_file(const char *filename, int width, int height, png_byte color_type, png_byte bit_depth, uint8_t *data, uint8_t *palette = NULL)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        abort();

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        abort();

    png_infop info = png_create_info_struct(png);
    if (!info)
        abort();

    if (setjmp(png_jmpbuf(png)))
        abort();

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(png, info, width, height, bit_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    int bytewidth = width * 4;
    
    if (color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_colorp png_palette = new png_color[256];
        for (int c = 0; c < 256; c++)
        {
            png_palette[c].red = palette[c * 3 + 0];
            png_palette[c].green = palette[c * 3 + 1];
            png_palette[c].blue = palette[c * 3 + 2];
        }
        
        png_set_PLTE(png, info, png_palette, 256);
        
        bytewidth = width;
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY)
    {
        bytewidth = width / (8 / bit_depth);
    }
    else if (bit_depth == 1)
    {
        bytewidth = width / 8;
    }
    else if (bit_depth == 2)
    {
        bytewidth = width / 4;
    }
    else if (bit_depth == 4)
    {
        bytewidth = width / 2;
    }

    png_write_info(png, info);

    png_bytep row_pointers[sizeof(png_bytep) * height]; // (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++)
        row_pointers[y] = data + y * bytewidth;
    
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    fclose(fp);

    // png_data_freer(&png, &info, PNG_USER_WILL_FREE_DATA, PNG_FREE_PLTE|PNG_FREE_TRNS|PNG_FREE_HIST);
    // png_destroy_write_struct(&png, &info);
    png_uint_32 mask = PNG_FREE_ALL;
    mask &= ~PNG_FREE_ROWS;
    png_free_data(png, info, mask, -1);
}

int asset_size(const uint8_t *buffer)
{
    if (buffer[0] == 0xfe && buffer[1] == 0x00)
    {
        return 32;
    }
    else if (buffer[0] == 0xfe && buffer[1] == 0xff)
    {
        return 768;
    }
    else if (buffer[0] == 0xff && buffer[1] > 0)
    {
        return buffer[1] * 8;
    }
    else if (buffer[0] == 0x00 && buffer[1] == 0x2C)
    {
        return 12;
    }
    else if (buffer[0] == 0x40 && buffer[1] == 0x00)
    {
        uint8_t bytes[4];
        bytes[3] = buffer[2];
        bytes[2] = buffer[3];
        bytes[1] = buffer[4];
        bytes[0] = buffer[5];
        return (*(uint32_t *)(bytes));
    }

    return 0;
}

bool does_it_overlap(const uint8_t *buffer, int address, int entries, int skip_entry, int location, int length)
{
    uint8_t bytes[4];
    for (int e = 0; e < entries; e ++)
    {
        if (skip_entry != e)
        {
            uint32_t position = address + e * 4;
            bytes[3] = buffer[position + 0];
            bytes[2] = buffer[position + 1];
            bytes[1] = buffer[position + 2];
            bytes[0] = buffer[position + 3];

            int eloc = position + 2 + (*(uint32_t *)(bytes));
            int esize = asset_size(buffer - 2);
            if (eloc + esize > location && eloc < location + length)
            {
                return true;
            }
        }
    }
    
    return false;
}

bool does_it_fit(const uint8_t *buffer, int length, int a, int e)
{
    uint8_t bytes[4];
    uint32_t value;

    for (int i = 0; i < e; i ++)
    {
        uint32_t position = a + i * 4;
        bytes[3] = buffer[position + 0];
        bytes[2] = buffer[position + 1];
        bytes[1] = buffer[position + 2];
        bytes[0] = buffer[position + 3];
        value = (*(uint32_t *)(bytes));
        if (value <= 0 || (position + 2 + value) >= length)
        {
            return false;
        }
    }
    
    return true;
}

bool find_assets(const uint8_t *buffer, int length, uint32_t& address, uint32_t& entries)
{
    uint8_t bytes[4];
    uint32_t location;

    // NOTE: looks like header is different using amiga decruncher
    // for the moment do not use!
    // bytes[1] = buffer[4];
    // bytes[0] = buffer[5];
    // uint32_t start = (*(uint16_t *)(bytes));

    int a, e;
    for (int i = 8/*start*/; i < length; i ++)
    {
        // NOTE: osadd
        if (buffer[i + 1] == 0x44)
        {
            location = i + 2 + i % 2;
            bytes[3] = buffer[location + 0];
            bytes[2] = buffer[location + 1];
            bytes[1] = buffer[location + 2];
            bytes[0] = buffer[location + 3];
            a = (*(uint32_t *)(bytes));

            location = location + 4;
            bytes[1] = buffer[location + 0];
            bytes[0] = buffer[location + 1];
            e = (*(uint16_t *)(bytes));

            if (a && e)
            {
                a += location - 4;

                uint32_t test = a + e * 4;
                if (test < length && does_it_fit(buffer, length, a, e))
                {
                    address = a;
                    entries = e;
                    return true;
                }
            }
        }
    }

    return false;
}

void extractor::extract_buffer(const std::string& name, const uint8_t *buffer, int length)
{
    uint8_t bytes[4];
    uint32_t value = 0;
    uint32_t location = 0;
    uint32_t address = 0;
    uint32_t entries = 0;

    // find adresses for all assets in file
    
    if (find_assets(buffer, length, address, entries))
    {
        cout << "Found " << std::dec << entries << " assets in " << name << endl;
    }
    else
    {
        cout << name << ": No assets to found!" << endl;
        return;
    }
    
    // identify known types and save them
    // (for the moment only bitmaps, palettes and fli video is recognized)
    
    int width;
    int height;
    int h0;
    int h1;

    uint8_t *activePalette16 = _pallette16;
    uint8_t *activePalette256 = _pallette256;
    uint8_t paletteData[256 * 3];

    int n = 0;
    
    std::set<int32_t> indexes2bit;
    std::set<int32_t> indexes4bitOld;
    std::set<int32_t> indexes4bit;
    std::set<int32_t> indexes8bit;

    bool isImage = false;
    for (int i = 0; i < entries; i ++)
    {
        isImage = false;
        uint32_t position = address + i * 4;
        bytes[3] = buffer[position + 0];
        bytes[2] = buffer[position + 1];
        bytes[1] = buffer[position + 2];
        bytes[0] = buffer[position + 3];
        value = (*(uint32_t *)(bytes));

        location = position + 2 + value;
        if (value > 0 && location < length)
        {
            cout << "Entry " << std::dec << i + 1 << " [0x" << std::hex << std::setw(6) << std::setfill('0') << position << " => 0x" << std::hex << std::setw(6) << std::setfill('0') << location << "]: ";

            h0 = buffer[location - 2];
            h1 = buffer[location - 1];
            if (h0 == 0xfe && h1 == 0x00)
            {
                if (_list_only == false)
                {
                    memset(paletteData, 0, 256 * 3);

                    int to = 0;
                    for (int f = 0; f < 16; f++)
                    {
                        uint8_t r = buffer[location + f * 2 + 0];
                        r = (r & 0b00000111) << 5;
                        uint8_t g = buffer[location + f * 2 + 1];
                        g = (g >> 4) << 5;
                        uint8_t b = buffer[location + f * 2 + 1];
                        b = (b & 0b00000111) << 5;

                        paletteData[to++] = r;
                        paletteData[to++] = g;
                        paletteData[to++] = b;
                    }

                    std::string palname = name + " " + std::to_string(++n) + ".act";
                    std::filesystem::path out = _out_dir;
                    out.append(palname);
                    
                    auto file = std::fstream(out, std::ios::out | std::ios::binary);
                    file.write((char *)paletteData, 256 * 3);
                    file.close();
                    
                    activePalette16 = paletteData;
                }
                
                cout << "palette 16" << endl;
            }
            else if (h0 == 0xfe && h1 == 0xff)
            {
                if (_list_only == false)
                {
                    int to = 0;
                    for (int f = 0; f < 256; f++)
                    {
                        paletteData[to++] = buffer[2 + location + (f * 3) + 0];
                        paletteData[to++] = buffer[2 + location + (f * 3) + 1];
                        paletteData[to++] = buffer[2 + location + (f * 3) + 2];
                    }

                    std::string palname = name + " " + std::to_string(++n) + ".act";
                    std::filesystem::path out = _out_dir;
                    out.append(palname);
                    
                    auto file = std::fstream(out, std::ios::out | std::ios::binary);
                    file.write((char *)paletteData, 256 * 3);
                    file.close();
                    
                    activePalette256 = paletteData;
                }
                
                cout << "palette 256" << endl;
            }
            else if (h0 == 0xff && h1 > 0)
            {
                // NOTE: following N * 8 bytes of unknown data
                cout << std::dec << h1 * 8 << " bytes of unknown data ";
                log_bytes_to(buffer, location, h1 * 8, length);
            }
            else if (h0 == 0x00 && h1 == 0x2C)
            {
                // NOTE: following 12 bytes of unknown data
                cout << "12 bytes of unknown data ";
                log_bytes_to(buffer, location, 12, length);
            }
            else if (h0 == 0x40 && h1 == 0x00)
            {
                bytes[3] = buffer[location + 0];
                bytes[2] = buffer[location + 1];
                bytes[1] = buffer[location + 2];
                bytes[0] = buffer[location + 3];
                uint32_t size = (*(uint32_t *)(bytes));
                char *fliname = (char *)&buffer[location + 4];
                cout << "FLI video (" << fliname << ") " << std::dec << size << " bytes [";
                
                size = (*(uint32_t *)(&buffer[location + 30]));
                uint16_t frames = (*(uint16_t *)(&buffer[location + 36]));

                cout << "size: " << std::dec << size << " frames: "  << std::dec << frames << "]" << endl;
                
                if (_list_only == false)
                {
                    std::filesystem::path out = _out_dir;
                    out.append(fliname);
                    
                    auto file = std::fstream(out, std::ios::out | std::ios::binary);
                    file.write((char *)(&buffer[location + 30]), size);
                    file.close();
                }
            }
            else
            {
                bytes[1] = buffer[location + 0];
                bytes[0] = buffer[location + 1];

                width = *(uint16_t *)(bytes) + 1;

                bytes[1] = buffer[location + 2];
                bytes[0] = buffer[location + 3];

                height = *(uint16_t *)(bytes) + 1;

                if (width > 1 && height > 1 && width <= 320 && height <= 200)
                {
                    if ((h0 == 0x10 || h0 == 0x12) && length - location > width * height / 2)
                    {
                        if (does_it_overlap(buffer, address, entries, i, location, width * height / 2) == false)
                        {
                            indexes4bit.insert(location);
                            isImage = true;
                        }
                    }
                    else if ((h0 == 0x14 || h0 == 0x16) && length - location > width * height)
                    {
                        if (does_it_overlap(buffer, address, entries, i, location, width * height) == false)
                        {
                            indexes8bit.insert(location);
                            isImage = true;
                        }
                    }
                    else if (length - location > width * height / 2) // if (h0 == 0x00 && h1 == 0x40)
                    {
                        if (does_it_overlap(buffer, address, entries, i, location, width * height / 2) == false)
                        {
                            indexes4bitOld.insert(location);
                            isImage = true;
                        }
                    }
                }
                
                if (isImage == false && (h0 == 0x00 || h0 == 0x02) && width > 1 && height > 1 && width <= 640 && height <= 400)
                {
                    if (does_it_overlap(buffer, address, entries, i, location, (width / 4) * height) == false)
                    {
                        indexes2bit.insert(location);
                        isImage = true;
                    }
                }
                
                if (isImage)
                {
                    cout << "bitmap Id " << std::dec << h1 << ", " << (h0 < 13 ? 4 : 8) << " bit, " << width << "x" << height << " ";
                    log_bytes_to(buffer, location - 2, 2, length);
                }
                else
                {
                    cout << "unknown ";
                    log_bytes(buffer, location, length);
                }
            }
        }
        else
        {
            cout << "OUT OF BOUNDS!" << endl;
        }
    }

    if (_list_only == false)
    {
        // save images
        
        if (_paletteOverride)
        {
            activePalette16 = _paletteOverride;
            activePalette256 = _paletteOverride;
        }
        
        n = 0;
        
        for (auto p : indexes2bit)
        {
            bytes[1] = buffer[p + 0];
            bytes[0] = buffer[p + 1];

            width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[p + 2];
            bytes[0] = buffer[p + 3];

            height = *(uint16_t *)(bytes) + 1;
            
            std::string imgname = name + " " + std::to_string(++n) + ".png";
            std::filesystem::path imgpath = _out_dir;
            imgpath.append(imgname);

            uint8_t *data = new uint8_t[width * height * 8];

            long at = p + 4;
            long to = 0;

            uint8_t colors[16];

            if (_force_tc)
            {
                for (int x = 0; x < width * height; x+=16)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        uint8_t rot = (7 - c);
                        uint8_t mask = 1 << rot;
                        colors[0 + c] = (((buffer[at + 0] & mask) >> rot) << 7) | ((buffer[at + 2] & mask) >> rot);
                        colors[8 + c] = (((buffer[at + 1] & mask) >> rot) << 7) | ((buffer[at + 3] & mask) >> rot);
                    }
                    
                    int c = 0;
                    for (int d = 0; d < 64; d+=4)
                    {
                        data[to + d + 3] = colors[c] == 1 ? 0x00 : 0xff;
                        data[to + d + 0] = data[to + d + 1] = data[to + d + 2] = colors[c++];
                    }

                    at+=4;
                    to+=64;
                }

                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_RGBA, 8, data);
            }
            else
            {
                for (int x = 0; x < width * height; x+=16)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        uint8_t rot = (7 - c);
                        uint8_t mask = 1 << rot;
                        colors[0 + c] = (((buffer[at + 0] & mask) >> rot) << 1) | ((buffer[at + 2] & mask) >> rot);
                        colors[8 + c] = (((buffer[at + 1] & mask) >> rot) << 1) | ((buffer[at + 3] & mask) >> rot);
                    }

                    data[to + 0] = (colors[ 0] << 6) | (colors[ 1] << 4) | (colors[ 2] << 2) | (colors[ 3]);
                    data[to + 1] = (colors[ 4] << 6) | (colors[ 5] << 4) | (colors[ 6] << 2) | (colors[ 7]);
                    data[to + 2] = (colors[ 8] << 6) | (colors[ 9] << 4) | (colors[10] << 2) | (colors[11]);
                    data[to + 3] = (colors[12] << 6) | (colors[13] << 4) | (colors[14] << 2) | (colors[15]);

                    at+=4;
                    to+=4;
                }

                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_GRAY, 2, data);
            }

            delete [] data;
        }

        for (auto p : indexes4bitOld)
        {
            bytes[1] = buffer[p + 0];
            bytes[0] = buffer[p + 1];

            width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[p + 2];
            bytes[0] = buffer[p + 3];

            height = *(uint16_t *)(bytes) + 1;

            int clearIndex = 0;
            int palIndex = 0;

            std::string imgname = name + " " + std::to_string(++n) + ".png";
            std::filesystem::path imgpath = _out_dir;
            imgpath.append(imgname);
            
            uint8_t *data;

            // NOTE: use correct palette for correct image
            if (_force_tc)
            {
                data = new uint8_t[width * height * 4];
        
                long at = p + 4;
                long to = 0;
                for (int x = 0; x < (width / 2) * height; x++, at++)
                {
                    uint8_t r = buffer[at];
                    uint8_t a = palIndex + ((r & 0b11110000) >> 4);
                    uint8_t b = palIndex + (r & 0b00001111);
    
                    data[to++] = activePalette16[a * 3 + 0];
                    data[to++] = activePalette16[a * 3 + 1];
                    data[to++] = activePalette16[a * 3 + 2];
                    data[to++] = a == palIndex + clearIndex ? 0x00 : 0xff;
    
                    data[to++] = activePalette16[b * 3 + 0];
                    data[to++] = activePalette16[b * 3 + 1];
                    data[to++] = activePalette16[b * 3 + 2];
                    data[to++] = b == palIndex + clearIndex ? 0x00 : 0xff;
                }
        
                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_RGBA, 8, data);
            }
            else
            {
                data = new uint8_t[width * height];

                long at = p + 4;
                long to = 0;
                for (int x = 0; x < (width / 2) * height; x++, at++)
                {
                    uint8_t r = buffer[at];
                    uint8_t a = ((r & 0b11110000) >> 4);
                    uint8_t b = (r & 0b00001111);

                    data[to++] = a;
                    data[to++] = b;
                }

                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_PALETTE, 8, data, activePalette16);
            }
            
            delete [] data;
        }

        for (auto p : indexes4bit)
        {
            bytes[1] = buffer[p + 0];
            bytes[0] = buffer[p + 1];

            width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[p + 2];
            bytes[0] = buffer[p + 3];

            height = *(uint16_t *)(bytes) + 1;

            int clearIndex = buffer[p + 5];
            int palIndex = buffer[p + 4];

            std::string imgname = name + " " + std::to_string(++n) + ".png";
            std::filesystem::path imgpath = _out_dir;
            imgpath.append(imgname);
            
            uint8_t *data;

            // NOTE: use correct palette for correct image
            if (_force_tc)
            {
                data = new uint8_t[width * height * 4];
        
                long at = p + 4 + 2;
                long to = 0;
                for (int x = 0; x < (width / 2) * height; x++, at++)
                {
                    uint8_t r = buffer[at];
                    uint8_t a = palIndex + ((r & 0b11110000) >> 4);
                    uint8_t b = palIndex + (r & 0b00001111);
    
                    data[to++] = activePalette256[a * 3 + 0];
                    data[to++] = activePalette256[a * 3 + 1];
                    data[to++] = activePalette256[a * 3 + 2];
                    data[to++] = a == palIndex + clearIndex ? 0x00 : 0xff;
    
                    data[to++] = activePalette256[b * 3 + 0];
                    data[to++] = activePalette256[b * 3 + 1];
                    data[to++] = activePalette256[b * 3 + 2];
                    data[to++] = b == palIndex + clearIndex ? 0x00 : 0xff;
                }
        
                std::string imgname = name + " " + std::to_string(++n) + ".png";
                std::filesystem::path imgpath = _out_dir;
                imgpath.append(imgname);
        
                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_RGBA, 8, data);
            }
            else
            {
                data = new uint8_t[width * height];

                long at = p + 4 + 2;
                long to = 0;
                for (int x = 0; x < (width / 2) * height; x++, at++)
                {
                    uint8_t r = buffer[at];
                    uint8_t a = palIndex + ((r & 0b11110000) >> 4);
                    uint8_t b = palIndex + (r & 0b00001111);

                    data[to++] = a;
                    data[to++] = b;
                }

                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_PALETTE, 8, data, activePalette256);
            }

            delete [] data;
        }

        for (auto p : indexes8bit)
        {
            bytes[1] = buffer[p + 0];
            bytes[0] = buffer[p + 1];

            width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[p + 2];
            bytes[0] = buffer[p + 3];

            height = *(uint16_t *)(bytes) + 1;
            
            std::string imgname = name + " " + std::to_string(++n) + ".png";
            std::filesystem::path imgpath = _out_dir;
            imgpath.append(imgname);

            uint8_t *data;
            if (_force_tc)
            {
                data = new uint8_t[width * height * 4];
        
                long at = p + 4 + 2;
                long to = 0;
                for (int x = 0; x < width * height; x++, at++)
                {
                    int index = buffer[at];
                    data[to++] = activePalette256[index * 3 + 0];
                    data[to++] = activePalette256[index * 3 + 1];
                    data[to++] = activePalette256[index * 3 + 2];
                    data[to++] = index == 16 ? 0x00 : 0xff;
                }
        
                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_RGBA, 8, data);
            }
            else
            {
                data = new uint8_t[width * height];
                memcpy(data, buffer + p + 4 + 2, width * height);
                write_png_file(imgpath.string().c_str(), width, height, PNG_COLOR_TYPE_PALETTE, 8, data, activePalette256);
            }
            
            delete [] data;
        }
    }
}
