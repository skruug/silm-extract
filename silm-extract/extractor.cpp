//
//  extractor.cpp
//  silm-extract
//
//  Created on 08.08.2022.
//

#include "extractor.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <ranges>

#include <png.h>
#include "utils.hpp"
#include "wav.hpp"

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
    for (int ii = 0; ii < 16; ii++)
    {
        for (int i = 0; i < 16; i++)
        {
            _default_pal[(ii * 16 * 3) + (i * 3) + 0] = i * 16;
            _default_pal[(ii * 16 * 3) + (i * 3) + 1] = i * 16;
            _default_pal[(ii * 16 * 3) + (i * 3) + 2] = i * 16;
        }
    }

    _out_dir = output;
    _override_pal = (uint8_t *)palette;
    _force_tc = force_tc;
    _list_only = list_only;
}

extractor::~extractor()
{
    
}

void extractor::set_palette(uint8_t *palette)
{
    _override_pal = (uint8_t *)palette;
}

void extractor::set_out_dir(const path& output)
{
    _out_dir = output;
}

bool extractor::is_script(const path& file)
{
    string e = utils::get_file_ext(file.string());
    transform(e.begin(), e.end(), e.begin(), ::tolower);
    for (const char * &v : extensions)
    {
        if (v == e)
        {
            return true;
        }
    }

    return false;
}

void extractor::extract_dir(const path& dir, uint32_t type)
{
    for (const auto & file : directory_iterator(dir))
    {
        if (is_script(file.path()))
            extract_file(file.path(), type);
    }
}

void extractor::extract_file(const path& file, uint32_t type, vector<uint8_t *> *pal_overrides)
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
                extract_buffer(name, unpacked, unpacked_size, type, pal_overrides);
                free(unpacked);
            }
        }
        else
        {
            // probably not gona to work, but what the hell :-)
            // it could only work on unpacked files
            
            unpacked_size = (int)length;
            unpacked = (uint8_t *)buffer;

            extract_buffer(name, unpacked, unpacked_size, type, pal_overrides);
        }
        
        delete[] buffer;
    }
    
    cout << endl;
}

void log_data(const uint8_t *p, int f, int s0, int s1, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    char t0[8] = "]";
    if (s0 > 24)
    {
        s0 = 24;
        strcpy(t0, "...");
    }

    char t1[8] = "]";
    if (s1 > 24)
    {
        s1 = 24;
        strcpy(t1, "...");
    }

    s1 += s0 + f;
    s0 += f;

    if (f < s0)
    {
        printf("[");
        
        for (int i = f; i < s0; i++)
            printf(" %.2x", p[i]);
        
        printf(" %s", t0);
    }
    
    if (s0 < s1)
    {
        printf("[");
        
        for (int i = s0; i < s1; i++)
            printf(" %.2x", p[i]);
        
        printf(" %s", t1);
    }

    printf("\n");
}

void extractor::write_buffer(const std::filesystem::path& path, const Buffer& buffer)
{
    auto file = std::fstream(path, std::ios::out | std::ios::binary);
    file.write((char *)buffer.data, buffer.size);
    file.close();
}

void extractor::write_png_file(const char *filename, int width, int height, png_byte color_type, png_byte bit_depth, uint8_t *data, uint8_t *palette)
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
    int h0 = buffer[0];
    int h1 = buffer[1];

    uint8_t bytes[4];

    switch (h0)
    {
        case 0x01:
        {
            return 4 * 2;
        }
        case 0x00:
        case 0x02:
        {
            bytes[1] = buffer[2 + 0];
            bytes[0] = buffer[2 + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[2 + 2];
            bytes[0] = buffer[2 + 3];
            int height = *(uint16_t *)(bytes) + 1;

            return (width / 2) * height;;
        }
        case 0x10:
        case 0x12:
        {
            bytes[1] = buffer[2 + 0];
            bytes[0] = buffer[2 + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[2 + 2];
            bytes[0] = buffer[2 + 3];
            int height = *(uint16_t *)(bytes) + 1;

            return (width / 2) * height;
        }
        case 0x14:
        case 0x16:
        {
            bytes[1] = buffer[2 + 0];
            bytes[0] = buffer[2 + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = buffer[2 + 2];
            bytes[0] = buffer[2 + 3];
            int height = *(uint16_t *)(bytes) + 1;

            return width * height;
        }
        case 0x40:
        {
            bytes[3] = buffer[2 + 0];
            bytes[2] = buffer[2 + 1];
            bytes[1] = buffer[2 + 2];
            bytes[0] = buffer[2 + 3];
            return (*(uint32_t *)(bytes)) - 1;
        }
        case 0xfe:
        {
            return h1 == 0x00 ? 32 : (h1 + 1) * 3;
        }
        case 0xff:
        {
            return h1 * 8;
        }
        case 0x100:
        case 0x101:
        case 0x102:
        case 0x104:
        {
            bytes[3] = buffer[2 + 0];
            bytes[2] = buffer[2 + 1];
            bytes[1] = buffer[2 + 2];
            bytes[0] = buffer[2 + 3];
            return (*(uint32_t *)(bytes)) - 1;
        }

        default:
        {
            break;
        }
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

            uint32_t eloc = position + 2 + (*(uint32_t *)(bytes));
            uint32_t esize = asset_size(buffer + eloc - 2);
            if (eloc + esize > location && eloc < location + length)
            {
                if (buffer[eloc - 2] == 0x00 || buffer[eloc - 2] == 0x02)
                {
                    esize /= 2;
                    if (eloc + esize > location && eloc < location + length)
                    {
                        return true;
                    }
                }
                else
                {
                    return true;
                }
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

int compare_arrays(const uint8_t *a, const uint8_t *b, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return 0;
    }

    return 1;
}

bool extractor::find_assets(const uint8_t *buffer, int length, uint32_t& address, uint32_t& entries, uint32_t& mod)
{
    uint8_t bytes[4];
    uint32_t location;
    
    mod = 0;

    // NOTE: looks like header is different using amiga decruncher
    // for the moment do not use!
    // bytes[1] = buffer[4];
    // bytes[0] = buffer[5];
    // uint16_t start = (*(uint16_t *)(bytes));

    uint8_t pattern1[] = { 0x44, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58 };
    uint8_t pattern2[] = { 0x44, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58 };

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
                    cout << "Found address block [0x" << std::hex << std::setw(6) << std::setfill('0') << (i + 2 + i % 2) << "]";
                    
                    address = a;
                    entries = e;
                    return true;
                }
            }
            
            int location = 0;
            if (compare_arrays(buffer + 1 + i, pattern1, sizeof(pattern1)))
            {
                location = 1 + i + sizeof(pattern1);
            }
            
            if (compare_arrays(buffer + 1 + i, pattern2, sizeof(pattern2)))
            {
                location = 1 + i + sizeof(pattern2);
            }
            
            if (location)
            {
                bytes[1] = buffer[location + 0];
                bytes[0] = buffer[location + 1];
                e = (*(uint16_t *)(bytes));
                
                location = location + 6;
                for (int idx = location; idx < length; idx ++)
                {
                    if (buffer[idx] != 0)
                    {
                        // address
                        
                        address = location = idx - (2 + (idx % 2));
                        
                        bool reset = false;
                        
                        for (int s = 0; s < e; s++)
                        {
                            bytes[3] = buffer[location + 0];
                            bytes[2] = buffer[location + 1];
                            bytes[1] = buffer[location + 2];
                            bytes[0] = buffer[location + 3];
                            a = (*(uint32_t *)(bytes));

                            uint32_t test = (location + a + 8);
                            if (test >= length)
                            {
                                reset = true;
                                break;
                            }

                            bytes[3] = buffer[location + a + 2 + 0];
                            bytes[2] = buffer[location + a + 2 + 1];
                            bytes[1] = buffer[location + a + 2 + 2];
                            bytes[0] = buffer[location + a + 2 + 3];
                            uint32_t len = (*(uint32_t *)(bytes)) - 1;
                            
                            if ((uint32_t)(location + a + len) >= length || len >= length)
                            {
                                reset = true;
                                break;
                            }
                            
                            location += 4;
                        }
                        
                        if (reset == false)
                        {
                            // sucess
                            
                            mod = 0x100;
                            entries = e;
                            return true;
                        }
                        
                        reset = false;
                    }
                }
            }
        }
    }

    cout << "Can't found address block";
    return false;
}

void extractor::set_palette(Buffer& script, uint32_t address, uint32_t entries)
{
    uint8_t bytes[4];

    int h0;
    int h1;

    for (int i = 0; i < entries; i ++)
    {
        uint32_t position = address + i * 4;
        bytes[3] = script[position + 0];
        bytes[2] = script[position + 1];
        bytes[1] = script[position + 2];
        bytes[0] = script[position + 3];
        uint32_t value = (*(uint32_t *)(bytes));

        uint32_t location = position + 2 + value;

        h0 = script[location - 2];
        h1 = script[location - 1];
        
        // NOTE: we are interested in full palette only
        
        if (h0 == 0xfe && h1 == 0x00)
        {
            get_entry_data(script, 0, address, entries, i);
            return;
        }
        else if (h0 == 0xfe && h1 == 0xff)
        {
            get_entry_data(script, 0, address, entries, i);
            return;
        }
    }
}

const int composite_width = 320;
const int composite_height = 200;

Entry *extractor::get_entry_data(Buffer& script, uint32_t mod, uint32_t address, uint32_t entries, uint32_t index)
{
    if (_entry_map[index])
    {
        return _entry_map[index];
    }
    
    uint8_t bytes[4];

    int h0;
    int h1;

    uint32_t position = address + index * 4;
    bytes[3] = script[position + 0];
    bytes[2] = script[position + 1];
    bytes[1] = script[position + 2];
    bytes[0] = script[position + 3];
    uint32_t value = (*(uint32_t *)(bytes));

    uint32_t location = position + 2 + value;

    h0 = mod + script[location - 2];
    h1 = script[location - 1];
    
    switch (h0)
    {
        case 0x01:
        {
            return (_entry_map[index] = new Entry(data_type::rectangle, location, Buffer()));
        }
        case 0x00:
        case 0x02:
        {
            bytes[1] = script[location + 0];
            bytes[0] = script[location + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            int height = *(uint16_t *)(bytes) + 1;
            
            int at = location + 4;
            int to = 0;

            if (does_it_overlap(script.data, address, entries, index, location, (width / 2) * height) == false && at + (width / 2) * height < script.size)
            {
                uint8_t *data = new uint8_t[width * height];

                for (int x = 0; x < (width / 2) * height; x++, at++)
                {
                    uint8_t r = script[at];
                    uint8_t a = ((r & 0b11110000) >> 4);
                    uint8_t b = (r & 0b00001111);

                    data[to++] = a;
                    data[to++] = b;
                }

                return (_entry_map[index] = new Entry(data_type::image4ST, location, Buffer(data, width * height)));
            }
            else if (does_it_overlap(script.data, address, entries, index, location, (width / 4) * height) == false && at + (width / 4) * height < script.size)
            {
                uint8_t *data = new uint8_t[width * height];
                uint8_t pixels[16];

                for (int b = 0; b < width * height; b+=16)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        uint8_t rot = (7 - c);
                        uint8_t mask = 1 << rot;
                        pixels[0 + c] = (((script[at + 0] & mask) >> rot) << 7) | ((script[at + 2] & mask) >> rot);
                        pixels[8 + c] = (((script[at + 1] & mask) >> rot) << 7) | ((script[at + 3] & mask) >> rot);
                    }

                    for (int d = 0; d < 16; d++)
                    {
                        data[to + d] = pixels[d];
                    }

                    at+=4;
                    to+=16;
                }

                return (_entry_map[index] = new Entry(data_type::image2, location, Buffer(data, width * height)));
            }
            
            break;
        }
        case 0x10:
        case 0x12:
        {
            bytes[1] = script[location + 0];
            bytes[0] = script[location + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            int height = *(uint16_t *)(bytes) + 1;

            uint8_t *data = new uint8_t[width * height];

            int at = location + 4 + 2;
            int to = 0;

            // int clear = script[location + 5];
            int palIndex = script[location + 4];
            
            for (int x = 0; x < (width / 2) * height; x++, at++)
            {
                uint8_t r = script[at];
                uint8_t a = palIndex + ((r & 0b11110000) >> 4);
                uint8_t b = palIndex + (r & 0b00001111);
                
                data[to++] = a;
                data[to++] = b;
            }
            
            return (_entry_map[index] = new Entry(data_type::image4, location, Buffer(data, width * height)));
        }
        case 0x14:
        case 0x16:
        {
            bytes[1] = script[location + 0];
            bytes[0] = script[location + 1];
            int width = *(uint16_t *)(bytes) + 1;

            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            int height = *(uint16_t *)(bytes) + 1;

            uint8_t *data = new uint8_t[width * height];
            memcpy(data, script.data + location + 4 + 2, width * height);
                
            return (_entry_map[index] = new Entry(data_type::image8, location, Buffer(data, width * height)));
        }
        case 0x40:
        {
            bytes[3] = script[location + 0];
            bytes[2] = script[location + 1];
            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            uint32_t size = (*(uint32_t *)(bytes));
            char *fliname = (char *)&script[location + 4];
            cout << "FLI video (" << fliname << ") " << std::dec << size << " bytes [";

            size = (*(uint32_t *)(&script[location + 30]));
            uint16_t frames = (*(uint16_t *)(&script[location + 36]));

            cout << "size: " << std::dec << size << " frames: "  << std::dec << frames << "]" << endl;
            
            uint8_t *data = new uint8_t[size];
            memcpy(data, &script[location + 30], size);

            return (_entry_map[index] = new Entry(data_type::video, location, Buffer(data, size)));
        }
        case 0xfe:
        {
            uint8_t *palette_data = new uint8_t[256 * 3];
            memcpy(palette_data, _default_pal, 256 * 3);

            int to = 0;

            if (h1 == 0x00)
            {
                for (int f = 0; f < 16; f++)
                {
                    uint8_t r = script[location + f * 2 + 0];
                    r = (r & 0b00000111) << 5;
                    uint8_t g = script[location + f * 2 + 1];
                    g = (g >> 4) << 5;
                    uint8_t b = script[location + f * 2 + 1];
                    b = (b & 0b00000111) << 5;
                    
                    palette_data[to++] = r;
                    palette_data[to++] = g;
                    palette_data[to++] = b;
                }
                
                _active_pal = palette_data;
                return (_entry_map[index] = new Entry(data_type::palette4, location, Buffer(palette_data, 256 * 3)));
            }
            else
            {
                for (int f = 0; f < h1 + 1; f++)
                {
                    palette_data[to++] = script[2 + location + (f * 3) + 0];
                    palette_data[to++] = script[2 + location + (f * 3) + 1];
                    palette_data[to++] = script[2 + location + (f * 3) + 2];
                }
                
                _active_pal = palette_data;
                return (_entry_map[index] = new Entry(data_type::palette8, location, Buffer(palette_data, 256 * 3)));
            }
        }
        case 0xff:
        {
            uint8_t *data = new uint8_t[composite_width * composite_height];
            memset(data, 0, composite_width * composite_height);
            
            if (h1 == 0)
            {
                // clear screen?
                return (_entry_map[index] = new Entry(data_type::composite, location, Buffer(data, composite_width * composite_height)));
            }
            else
            {
                // draw call
                // uint8    command (0 normal, )
                // uint8    entry
                // uint16   x origin (from left side of screen to bitmap center)
                // uint16   draw order
                // uint16   y origin (from bottom side of screen to bitmap center)
                
                std::map<int, uint8_t *> layers;
                
                // HACK: we don't know where on screen script wants to draw
                // so, to actually display anything, check if positions fit screen, if not center it.
                
                int minX = composite_width;
                int minY = composite_height;
                int maxX = 0;
                int maxY = 0;
                
                for (int b = 0; b < h1; b++)
                {
                    bytes[0] = script[b * 8 + location + 1];
                    uint8_t idx = (*(uint8_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 2];
                    bytes[0] = script[b * 8 + location + 3];
                    int16_t x = (*(int16_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 4];
                    bytes[0] = script[b * 8 + location + 5];
                    int16_t d = (*(int16_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 6];
                    bytes[0] = script[b * 8 + location + 7];
                    int16_t y = (*(int16_t *)(bytes));
                    
                    if (idx >= 0 && idx < entries)
                    {
                        Entry *entry = get_entry_data(script, mod, address, entries, idx);
                        if (entry->type == data_type::image2 || entry->type == data_type::image4ST || entry->type == data_type::image4 || entry->type == data_type::image8)
                        {
                            bytes[1] = script[entry->position + 0];
                            bytes[0] = script[entry->position + 1];
                            int width = *(uint16_t *)(bytes) + 1;
                            
                            bytes[1] = script[entry->position + 2];
                            bytes[0] = script[entry->position + 3];
                            int height = *(uint16_t *)(bytes) + 1;
                            
                            int xx = 1 + x - ((width + 1) / 2);
                            if (xx < minX)
                                minX = xx;
                            
                            // NOTE: works in colorado
                            // int yy = composite_height - (d + y + ((height + 1) / 2));
                            int yy = composite_height - (y + ((height + 1) / 2));
                            if (yy < minY)
                                minY = yy;
                            
                            if (xx + width > maxX)
                                maxX = xx + width;
                            
                            if (yy + height > maxY)
                                maxY = yy + height;
                        }
                    }
                }
                
                int modX = 0;
                int modY = 0;
                if (minX < 0 || maxX >= composite_width)
                {
                    modX = (composite_width / 2) - (((maxX - minX) / 2) + minX);
                }
                
                if (minY < 0 || maxY >= composite_height)
                {
                    modY = (composite_height / 2) - (((maxY - minY) / 2) + minY);
                }
                
                for (int b = 0; b < h1; b++)
                {
                    bytes[0] = script[b * 8 + location + 0];
                    uint8_t cmd = (*(uint8_t *)(bytes));
                    
                    bytes[0] = script[b * 8 + location + 1];
                    uint8_t idx = (*(uint8_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 2];
                    bytes[0] = script[b * 8 + location + 3];
                    int16_t x = (*(int16_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 4];
                    bytes[0] = script[b * 8 + location + 5];
                    int16_t d = (*(int16_t *)(bytes));
                    
                    bytes[1] = script[b * 8 + location + 6];
                    bytes[0] = script[b * 8 + location + 7];
                    int16_t y = (*(int16_t *)(bytes));
                    
                    if (layers[d] == NULL)
                    {
                        layers[d] = new uint8_t[composite_width * composite_height];
                        memset(layers[d], 0, composite_width * composite_height);
                    }
                    
                    if (idx >= 0 && idx < entries)
                    {
                        Entry *entry = get_entry_data(script, mod, address, entries, idx);
                        if (entry->type != none && entry->type != unknown)
                        {
                            bytes[1] = script[entry->position + 0];
                            bytes[0] = script[entry->position + 1];
                            int width = *(uint16_t *)(bytes) + 1;
                            
                            bytes[1] = script[entry->position + 2];
                            bytes[0] = script[entry->position + 3];
                            int height = *(uint16_t *)(bytes) + 1;
                            
                            int xx = 1 + x - ((width + 1) / 2);
                            xx += modX;
                            
                            // int yy = composite_height - (d + y + ((height + 1) / 2));
                            int yy = composite_height - (y + ((height + 1) / 2));
                            yy += modY;
                            
                            int vs = 0;
                            int vf = yy;
                            int vt = height;
                            if (vf < 0)
                            {
                                vf = 0;
                                vt += yy;
                                vs -= yy;
                            }
                            
                            if (vt + vf >= composite_height)
                            {
                                vt = composite_height - vf;
                            }
                            
                            int hs = 0;
                            int hf = xx;
                            int ht = width;
                            if (hf < 0)
                            {
                                hf = 0;
                                ht += xx;
                                hs -= xx;
                            }
                            
                            if (ht + hf >= composite_width)
                            {
                                ht = composite_width - hf;
                            }
                            
                            uint8_t *layer = layers[d];
                            
                            if (entry->type == data_type::image2 || entry->type == data_type::image4ST || entry->type == data_type::image4 || entry->type == data_type::image8)
                            {
                                int clear = 0;
                                if (entry->type == data_type::image4)
                                {
                                    clear = script[entry->position + 5] + script[entry->position + 4];
                                }
                                
                                if (entry->type == data_type::image8)
                                {
                                    clear = script[entry->position + 5];
                                }
                                
                                for (int h = vs; h < vs + vt; h++)
                                {
                                    for (int w = hs; w < hs + ht; w++)
                                    {
                                        uint8_t color = entry->buffer.data[(cmd ? width - (w + 1) : w) + h * width];
                                        if (color != clear)
                                        {
                                            uint8_t *ptr = layer + xx + w + ((yy + h) * composite_width);
                                            *ptr = color;
                                        }
                                    }
                                }
                            }
                            else if (entry->type == data_type::rectangle)
                            {
                                for (int h = vs; h < vt; h++)
                                {
                                    memset(layer + hf + ((yy + h) * composite_width), 0, ht);
                                }
                            }
                        }
                    }
                }
                
                for (auto it = layers.rbegin(); it != layers.rend(); it++)
                {
                    for (int p = 0; p < composite_height; p++)
                    {
                        uint8_t *ptr = data + p * composite_width;
                        for (int a = 0; a < composite_width; a++, ptr++)
                        {
                            uint8_t color = it->second[a + p * composite_width];
                            if (color)
                            {
                                *ptr = color;
                            }
                        }
                    }
                    
                    delete [] it->second;
                }
                
                return (_entry_map[index] = new Entry(data_type::composite, location, Buffer(data, composite_width * composite_height)));
            }
        }
        case 0x100:
        case 0x104:
        {
            bytes[3] = script[location + 0];
            bytes[2] = script[location + 1];
            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            uint32_t len = (*(uint32_t *)(bytes)) - 1;

            uint8_t *data = new uint8_t[len];
            memcpy(data, script.data + location + 4, len);

            return (_entry_map[index] = new Entry(data_type::pattern, location, Buffer(data, len)));
        }
        case 0x101:
        case 0x102:
        {
            bytes[3] = script[location + 0];
            bytes[2] = script[location + 1];
            bytes[1] = script[location + 2];
            bytes[0] = script[location + 3];
            uint32_t len = (*(uint32_t *)(bytes)) - 1;

            uint8_t *data = new uint8_t[len];
            memcpy(data, script.data + location + 4, len);

            return (_entry_map[index] = new Entry(data_type::sample, location, Buffer(data, len)));
        }

        default:
        {
            if (h0 > 0x100)
            {
                cout << "Unknown sound type!" << endl;
            }
            
            break;
        }
    }
    
    return (_entry_map[index] = new Entry());
}

const char *string_for_type(data_type type)
{
    switch (type)
    {
        case none:
            return "none";
        case image2:
            return "bitmap 2 bit";
        case image4ST:
            return "bitmap 4 bit v1";
        case image4:
            return "bitmap 4 bit v2";
        case image8:
            return "bitmap 8 bit";
        case video:
            return "video";
        case palette4:
            return "4 bit palette";
        case palette8:
            return "8 bit palette";
        case composite:
            return "composite";
        case rectangle:
            return "rectangle";
        default:
            break;
    };

    return "unknown";
}

void extractor::extract_buffer(const std::string& name, uint8_t *buffer, int length, uint32_t etype, vector<uint8_t *> *pal_overrides)
{
    uint8_t bytes[4];
    uint32_t value = 0;
    uint32_t location = 0;
    uint32_t address = 0;
    uint32_t entries = 0;
    uint32_t mod = 0;

    // find adresses for all assets in file
    
    if (find_assets(buffer, length, address, entries, mod))
    {
        cout << " containing " << std::dec << entries << " assets" << endl;
    }
    else
    {
        return;
    }
    
    // identify known types and save them
    // (for the moment bitmaps, rectangles, palettes, draw commands, samples and fli videos are recognized)
    
    int width;
    int height;
    int h0;
    int h1;

    uint8_t *active_pal = _override_pal ? _override_pal : _default_pal;
    
    Buffer script(buffer, length);
    set_palette(script, address, entries);

    vector<Entry *> entryList;
    
    for (int i = 0; i < entries; i ++)
    {
        entryList.push_back(NULL);
        
        uint32_t position = address + i * 4;
        bytes[3] = buffer[position + 0];
        bytes[2] = buffer[position + 1];
        bytes[1] = buffer[position + 2];
        bytes[0] = buffer[position + 3];
        value = (*(uint32_t *)(bytes));
        
        location = position + 2 + value;
        if (value > 0 && location < length)
        {
            entryList[i] = get_entry_data(script, mod, address, entries, i);
        }
    }
    
    for (int i = 0; i < entries; i ++)
    {
        uint32_t position = address + i * 4;
        bytes[3] = buffer[position + 0];
        bytes[2] = buffer[position + 1];
        bytes[1] = buffer[position + 2];
        bytes[0] = buffer[position + 3];
        value = (*(uint32_t *)(bytes));

        location = position + 2 + value;
        if (value > 0 && location < length)
        {
            printf("Entry %d [0x%.6x => 0x%.6x]: ", i, position, location);

            h0 = buffer[location - 2];
            h1 = buffer[location - 1];
            
            active_pal = pal_overrides && pal_overrides->size() > i && (*pal_overrides)[i] ? (*pal_overrides)[i] : _override_pal ? _override_pal : _default_pal;

            Entry *entry = entryList[i];
            switch (entry->type)
            {
                case data_type::palette4:
                {
                    cout << "palette 16" << endl;
                    
                    if (_list_only == false && etype & ex_palette)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                }
                case data_type::palette8:
                {
                    cout << "palette 256" << endl;
                    
                    if (_list_only == false && etype & ex_palette)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                }
                case data_type::image2:
                case data_type::image4ST:
                case data_type::image4:
                case data_type::image8:
                {
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
                    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 0, "%s bit, %d x %d ", string_for_type(entry->type), width, height);
                    
                    if (_list_only == false && etype & ex_image)
                    {
                        std::filesystem::path out = _out_dir / (name + " " + std::to_string(i) + ".png");
                        
                        if (_force_tc)
                        {
                            uint8_t *data = new uint8_t[width * height * 4];
                            
                            int clear = -1;
                            if (entry->type == data_type::image4)
                            {
                                clear = buffer[location + 5] + buffer[location + 4];
                            }
                            
                            if (entry->type == data_type::image8)
                            {
                                clear = buffer[location + 5];
                            }
                            
                            int to = 0;
                            for (int x = 0; x < width * height; x++)
                            {
                                int index = entry->buffer.data[x];
                                data[to++] = active_pal[index * 3 + 0];
                                data[to++] = active_pal[index * 3 + 1];
                                data[to++] = active_pal[index * 3 + 2];
                                data[to++] = index == clear ? 0x00 : 0xff;
                            }
                            
                            write_png_file(out.string().c_str(), width, height, PNG_COLOR_TYPE_RGBA, 8, data);
                        }
                        else
                        {
                            write_png_file(out.string().c_str(), width, height, PNG_COLOR_TYPE_PALETTE, 8, entry->buffer.data, active_pal);
                        }
                    }
                    break;
                }
                case data_type::video:
                {
                    bytes[3] = buffer[location + 0];
                    bytes[2] = buffer[location + 1];
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    uint32_t size = (*(uint32_t *)(bytes));
                    char *fliname = (char *)&buffer[location + 4];
                    uint32_t size2 = (*(uint32_t *)(&buffer[location + 30]));
                    uint16_t frames = (*(uint16_t *)(&buffer[location + 36]));
                    
                    log_data(buffer, location - 2, 2, 4, "FLI video (%s) %d bytes [size: %d frames: %d]", fliname, size, size2, frames);
                    
                    if (_list_only == false && etype & ex_video)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".fli"), entry->buffer);
                    }
                    break;
                }
                case data_type::composite:
                {
                    log_data(buffer, location - 2, 2, 0, "%d draw instructions ", h1);
                    
                    for (int b = 0; b < h1; b++)
                    {
                        bytes[0] = buffer[b * 8 + location + 0];
                        uint8_t cmd = (*(uint8_t *)(bytes));

                        bytes[0] = buffer[b * 8 + location + 1];
                        uint8_t index = (*(uint8_t *)(bytes));
    
                        bytes[1] = buffer[b * 8 + location + 2];
                        bytes[0] = buffer[b * 8 + location + 3];
                        int16_t x = (*(int16_t *)(bytes));
    
                        bytes[1] = buffer[b * 8 + location + 4];
                        bytes[0] = buffer[b * 8 + location + 5];
                        int16_t o = (*(int16_t *)(bytes));
    
                        bytes[1] = buffer[b * 8 + location + 6];
                        bytes[0] = buffer[b * 8 + location + 7];
                        int16_t y = (*(int16_t *)(bytes));
                        
                        // cmd
                        // 0        = draw
                        // 1        = ???
                        // 128      = invert x
                        // 129      = ???
                        // 134      = ???
                        // 34       = ???

                        Entry *e = entryList[index];
                        if (e->type != none && e->type != unknown)
                        {
                            bytes[1] = script[e->position + 0];
                            bytes[0] = script[e->position + 1];
                            width = *(uint16_t *)(bytes) + 1;
                            
                            bytes[1] = script[e->position + 2];
                            bytes[0] = script[e->position + 3];
                            height = *(uint16_t *)(bytes) + 1;
                            
                            cout << "  cmd: " << std::dec << std::setw(3) << (int)cmd << " index: " << std::dec << std::setw(3) << (int)index << " type: " << string_for_type(e->type) << " x " << std::dec << x << " y " << std::dec << y  << " w " << std::dec << width << " h " << std::dec << height << " order: " << std::dec << o << endl;
                        }
                        else
                        {
                            cout << "  cmd: " << std::dec << std::setw(3) << (int)cmd << " index: " << std::dec << std::setw(3) << (int)index << " type: " << string_for_type(e->type) << " x " << std::dec << x << " y " << std::dec << y  << " w ? h ? order: " << std::dec << o << endl;
                        }
                    }
                    
                    // TODO: do composition in 32 bit
                    if (_list_only == false && etype & ex_draw)
                    {
                        std::filesystem::path out = _out_dir / (name + " " + std::to_string(i) + " (composite)" + ".png");

                        if (_force_tc)
                        {
                            uint8_t *data = new uint8_t[composite_width * composite_height * 4];
                    
                            int to = 0;
                            for (int x = 0; x < composite_width * composite_height; x++)
                            {
                                int index = entry->buffer.data[x];
                                data[to++] = active_pal[index * 3 + 0];
                                data[to++] = active_pal[index * 3 + 1];
                                data[to++] = active_pal[index * 3 + 2];
                                data[to++] = 0xff;
                            }
                    
                            write_png_file(out.string().c_str(), composite_width, composite_height, PNG_COLOR_TYPE_RGBA, 8, data);
                        }
                        else
                        {
                            write_png_file(out.string().c_str(), composite_width, composite_height, PNG_COLOR_TYPE_PALETTE, 8, entry->buffer.data, active_pal);
                        }
                    }
                    break;
                }
                case data_type::rectangle:
                {
                    // NOTE: following 4 bytes describing size of black rectangle
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
                    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 4, "rectangle Id %d, %d x %d ", h1, width, height);
                    break;
                }
                case data_type::pattern:
                {
                    if (_list_only == false && etype & ex_sound)
                    {
                        std::filesystem::path filename = std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".pattern");
                        
                        FILE *fp = fopen(filename.string().c_str(), "wb");
                        if (!fp)
                            abort();
                        
                        if (fwrite(entry->buffer.data, entry->buffer.size, 1, fp) != 1)
                            abort();
                        
                        fclose(fp);
                    }
                    
                    log_data(buffer, location - 2, 2, 4, "Possible mod pattern? (%d bytes)", entry->buffer.size);
                    break;
                }
                case data_type::sample:
                {
                    int freq = buffer[location - 1];
                    if (freq < 3 || freq > 23)
                        freq = 8;
                    
                    int len = entry->buffer.size;

                    if (_list_only == false && etype & ex_sound)
                    {
                        std::filesystem::path filename = std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".wav");
                        
                        FILE *fp = fopen(filename.string().c_str(), "wb");
                        if (!fp)
                            abort();
                        
                        int res = write_wav_header(fp, freq * 1000, len);
                        if (res)
                            abort();
                        
                        // try to find out if sample is signed or unsigned
                        
                        int smp_signed = 0;
                        int smp_unsigned = 0;
                        
                        for (int b = 0; b < len; b++)
                        {
                            uint8_t tui = entry->buffer.data[b];
                            uint8_t tsi = entry->buffer.data[b] ^ 0x80;
                            
                            if (tui != tsi)
                            {
                                if (tui > tsi)
                                {
                                    smp_unsigned++;
                                }
                                else
                                {
                                    smp_signed++;
                                }
                            }
                        }
                        
                        if (smp_unsigned > smp_signed)
                        {
                            // convert from signed to unsigned PCM
                            // NOTE: wav doesn't support signed 8 bit sample
                            
                            for (int b = 0; b < len; b++)
                            {
                                entry->buffer.data[b]  ^= 0x80;
                            }
                        }
                        
                        if (fwrite(entry->buffer.data, len, 1, fp) != 1)
                            abort();
                        
                        fclose(fp);
                    }
                    
                    log_data(buffer, location - 2, 2, 4, "PCM sample %d bytes %d Hz", len, freq);
                    break;
                }
                default:
                {
                    log_data(buffer, location - 2, 2, 8, "unknown ");
                    break;
                }
            }
        }
        else
        {
            cout << "OUT OF BOUNDS!" << endl;
        }
    }

    // cleanup
    
    for (auto & entryPair : _entry_map)
    {
        Entry *e = entryPair.second;
        if (e->buffer.data)
            delete [] e->buffer.data;
        
        delete e;
    }

    _entry_map.clear();
}
