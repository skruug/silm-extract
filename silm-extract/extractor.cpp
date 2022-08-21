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
#include <map>
#include <set>
#include <ranges>

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

void extractor::extract_dir(const path& dir)
{
    for (const auto & file : directory_iterator(dir))
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

void log_data(const uint8_t *p, int f, int s0, int s1, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    char t0[3] = "]";
    if (s0 > 24)
    {
        s0 = 24;
        strcpy(t0, "...");
    }

    char t1[3] = "]";
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
    else if ((buffer[0] == 0x00 && buffer[1] == 0x2C) || (buffer[0] == 0x00 && buffer[1] == 0x6C))
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
    else if (buffer[0] == 0x01)
    {
        return 4;
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
            int esize = asset_size(buffer + eloc - 2);
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
    // uint16_t start = (*(uint16_t *)(bytes));

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

// 0xbe, ctopalet ( 2b entry index 2b duration ? )

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
        if (h0 == 0xfe && h1 == 0x00)
        {
            get_entry_data(script, address, entries, i);
            return;
        }
        else if (h0 == 0xfe && h1 == 0xff)
        {
            get_entry_data(script, address, entries, i);
            return;
        }
    }
}

Entry *extractor::get_entry_data(Buffer& script, uint32_t address, uint32_t entries, uint32_t index)
{
    if (_entry_map[index])
    {
        return _entry_map[index];
    }
    
    uint8_t bytes[4];

    int width;
    int height;
    int h0;
    int h1;

    uint32_t position = address + index * 4;
    bytes[3] = script[position + 0];
    bytes[2] = script[position + 1];
    bytes[1] = script[position + 2];
    bytes[0] = script[position + 3];
    uint32_t value = (*(uint32_t *)(bytes));

    uint32_t location = position + 2 + value;

    h0 = script[location - 2];
    h1 = script[location - 1];
    if (h0 == 0xfe && h1 == 0x00)
    {
        uint8_t *palette_data = new uint8_t[256 * 3];
        memcpy(palette_data, _default_pal, 256 * 3);

        int to = 0;
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
        return (_entry_map[index] = new Entry(Type::palette4, location, Buffer(palette_data, 256 * 3)));
    }
    else if (h0 == 0xfe && h1 == 0xff)
    {
        uint8_t *palette_data = new uint8_t[256 * 3];
        memcpy(palette_data, _default_pal, 256 * 3);

        int to = 0;
        for (int f = 0; f < 256; f++)
        {
            palette_data[to++] = script[2 + location + (f * 3) + 0];
            palette_data[to++] = script[2 + location + (f * 3) + 1];
            palette_data[to++] = script[2 + location + (f * 3) + 2];
        }
        
        _active_pal = palette_data;
        return (_entry_map[index] = new Entry(Type::palette8, location, Buffer(palette_data, 256 * 3)));
    }
    else if (h0 == 0xff && h1 > 0)
    {
        // draw call
        // uint8    command (0 normal, )
        // uint8    entry
        // uint16   x origin (from left side of screen to bitmap center)
        // uint16   draw order
        // uint16   y origin (from bottom side of screen to bitmap center)
        
        uint8_t *data = new uint8_t[320 * 200];
        memset(data, 0, 320 * 200);
        
        std::map<int, uint8_t *> layers;

        // HACK: we don't know where on screen script wants to draw
        // so, to actually display anything, check if positions fit screen, if not center it.
        
        int minX = 320;
        int minY = 200;
        int maxX = 0;
        int maxY = 0;
        
        for (int b = 0; b < h1; b++)
        {
            bytes[0] = script[b * 8 + location + 1];
            uint8_t idx = (*(uint8_t *)(bytes));

            bytes[1] = script[b * 8 + location + 2];
            bytes[0] = script[b * 8 + location + 3];
            int16_t x = (*(int16_t *)(bytes));

            bytes[1] = script[b * 8 + location + 6];
            bytes[0] = script[b * 8 + location + 7];
            int16_t y = (*(int16_t *)(bytes));
            
            if (idx >= 0 && idx < entries)
            {
                Entry *entry = get_entry_data(script, address, entries, idx);
                if (entry->type == Type::image2 || entry->type == Type::image4old || entry->type == Type::image4 || entry->type == Type::image8)
                {
                    bytes[1] = script[entry->position + 0];
                    bytes[0] = script[entry->position + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[entry->position + 2];
                    bytes[0] = script[entry->position + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    int xx = 1 + x - (width / 2);
                    if (xx < minX)
                        minX = xx;
                    
                    int yy = (200 - y) - (height / 2);
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
        if (minX < 0 || maxX >= 320 || minY < 0 || maxY >= 200)
        {
            modX = 160 - (((maxX - minX) / 2) + minX);
            modY = 100 - (((maxY - minY) / 2) + minY);
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
                layers[d] = new uint8_t[320 * 200];
                memset(layers[d], 0, 320 * 200);
            }
            
            if (idx >= 0 && idx < entries)
            {
                Entry *entry = get_entry_data(script, address, entries, idx);
                if (entry->type == Type::image2 || entry->type == Type::image4old || entry->type == Type::image4 || entry->type == Type::image8)
                {
                    int clearIndex = 0;
                    if (entry->type == Type::image4)
                    {
                        clearIndex = script[entry->position + 5] + script[entry->position + 4];
                    }

                    if (entry->type == Type::image8)
                    {
                        clearIndex = script[entry->position + 5];
                    }

                    bytes[1] = script[entry->position + 0];
                    bytes[0] = script[entry->position + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[entry->position + 2];
                    bytes[0] = script[entry->position + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    int xx = 1 + x - (width / 2);
                    xx += modX;
                    
                    int yy = (200 - y) - (height / 2);
                    yy += modY;
                    
                    uint8_t *layer = layers[d];
                    
                    for (int p = 0; p < height; p++)
                    {
                        uint8_t *ptr = layer + std::max(0, xx) + ((yy + p) * 320);
                        for (int a = std::max(0, -xx); a < std::min(width, 320 -xx); a++, ptr++)
                        {
                            uint8_t color = entry->buffer.data[(cmd ? width - (a + 1) : a) + p * width];
                            if (color != clearIndex)
                            {
                                *ptr = color;
                            }
                        }
                    }
                }
                else if (entry->type == Type::rectangle)
                {
                    bytes[1] = script[entry->position + 0];
                    bytes[0] = script[entry->position + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[entry->position + 2];
                    bytes[0] = script[entry->position + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    int xx = 1 + x - (width / 2);
                    xx += modX;
                    
                    int yy = (200 - y) - (height / 2);
                    yy += modY;

                    uint8_t *layer = layers[d];

                    for (int p = 0; p < height; p++)
                    {
                        memset(layer + std::max(0, xx) + ((yy + p) * 320), 0, std::min(width, 320 -xx));
                    }
                }
            }
        }
        
        for (auto it = layers.rbegin(); it != layers.rend(); it++)
        {
            for (int p = 0; p < 200; p++)
            {
                uint8_t *ptr = data + p * 320;
                for (int a = 0; a < 320; a++, ptr++)
                {
                    uint8_t color = it->second[a + p * 320];
                    if (color)
                    {
                        *ptr = color;
                    }
                }
            }
            
            delete [] it->second;
        }
        
        return (_entry_map[index] = new Entry(Type::draw, location, Buffer(data, 320 * 200)));
    }
    else if (h0 == 0x00 && script[location + 0] == 0x00 && script[location + 1] == 0x0f && script[location + 2] == 0x00 && script[location + 3] == 0x00)// (h1 == 0x1a || h1 == 0x1b || h1 == 0x1c || h1 == 0x1d || h1 == 0x1e || h1 == 0x2C || h1 == 0x6C))
    {
        // NOTE: following 12 bytes of unknown data
        return (_entry_map[index] = new Entry(Type::unknown12, location, Buffer()));
    }
    else if (h0 == 0x40 && h1 == 0x00)
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

        return (_entry_map[index] = new Entry(Type::video, location, Buffer(data, size)));
    }
    else if (h0 == 0x01)
    {
        return (_entry_map[index] = new Entry(Type::rectangle, location, Buffer()));
    }
    else
    {
        bytes[1] = script[location + 0];
        bytes[0] = script[location + 1];

        width = *(uint16_t *)(bytes) + 1;

        bytes[1] = script[location + 2];
        bytes[0] = script[location + 3];

        height = *(uint16_t *)(bytes) + 1;

        if (width > 1 && height > 1 && width <= 320 && height <= 200)
        {
            if ((h0 == 0x10 || h0 == 0x12) && script.size - location > width * height / 2)
            {
                if (does_it_overlap(script.data, address, entries, index, location, width * height / 2) == false)
                {
                    bytes[1] = script[location + 0];
                    bytes[0] = script[location + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[location + 2];
                    bytes[0] = script[location + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    uint8_t *data = new uint8_t[width * height];

                    // int clearIndex = script[location + 5];
                    int palIndex = script[location + 4];
                    
                    int at = location + 4 + 2;
                    int to = 0;
                    for (int x = 0; x < (width / 2) * height; x++, at++)
                    {
                        uint8_t r = script[at];
                        uint8_t a = palIndex + ((r & 0b11110000) >> 4);
                        uint8_t b = palIndex + (r & 0b00001111);

                        data[to++] = a;
                        data[to++] = b;
                    }

                    return (_entry_map[index] = new Entry(Type::image4, location, Buffer(data, width * height)));
                }
            }
            else if ((h0 == 0x14 || h0 == 0x16) && script.size - location > width * height)
            {
                if (does_it_overlap(script.data, address, entries, index, location, width * height) == false)
                {
                    bytes[1] = script[location + 0];
                    bytes[0] = script[location + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[location + 2];
                    bytes[0] = script[location + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    uint8_t *data = new uint8_t[width * height];
                    memcpy(data, script.data + location + 4 + 2, width * height);

                    return (_entry_map[index] = new Entry(Type::image8, location, Buffer(data, width * height)));
                }
            }
            else if (script.size - location > width * height / 2) // if (h0 == 0x00 && h1 == 0x40)
            {
                if (does_it_overlap(script.data, address, entries, index, location, width * height / 2) == false)
                {
                    bytes[1] = script[location + 0];
                    bytes[0] = script[location + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[location + 2];
                    bytes[0] = script[location + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    // NOTE: use correct palette for correct image
                    uint8_t *data = new uint8_t[width * height];

                    int at = location + 4;
                    int to = 0;
                    for (int x = 0; x < (width / 2) * height; x++, at++)
                    {
                        uint8_t r = script[at];
                        uint8_t a = ((r & 0b11110000) >> 4);
                        uint8_t b = (r & 0b00001111);

                        data[to++] = a;
                        data[to++] = b;
                    }

                    return (_entry_map[index] = new Entry(Type::image4old, location, Buffer(data, width * height)));
                }
            }
        }
        
        if ((h0 == 0x00 || h0 == 0x02) && width > 1 && height > 1 && width <= 640 && height <= 400)
        {
            if (does_it_overlap(script.data, address, entries, index, location, (width / 4) * height) == false)
            {
                bytes[1] = script[location + 0];
                bytes[0] = script[location + 1];

                width = *(uint16_t *)(bytes) + 1;

                bytes[1] = script[location + 2];
                bytes[0] = script[location + 3];

                height = *(uint16_t *)(bytes) + 1;
                
                uint8_t *data = new uint8_t[width * height];

                int at = location + 4;
                int to = 0;

                uint8_t colors[16];

                for (int x = 0; x < width * height; x+=16)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        uint8_t rot = (7 - c);
                        uint8_t mask = 1 << rot;
                        colors[0 + c] = (((script[at + 0] & mask) >> rot) << 7) | ((script[at + 2] & mask) >> rot);
                        colors[8 + c] = (((script[at + 1] & mask) >> rot) << 7) | ((script[at + 3] & mask) >> rot);
                    }
                    
                    for (int d = 0; d < 64; d++)
                    {
                        data[to + d] = colors[d];
                    }

                    at+=4;
                    to+=64;
                }

                return (_entry_map[index] = new Entry(Type::image2, location, Buffer(data, width * height)));
            }
        }
    }
    
    return (_entry_map[index] = new Entry());
}

void extractor::extract_buffer(const std::string& name, uint8_t *buffer, int length)
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
    // (for the moment bitmaps, rectangles, palettes, draw command and fli video are recognized)
    
    int width;
    int height;
    int h0;
    int h1;

    _active_pal = _default_pal;
    
    Buffer script(buffer, length);
    set_palette(script, address, entries);

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

            Entry *entry = get_entry_data(script, address, entries, i);
            switch (entry->type)
            {
                case Type::palette4:
                    cout << "palette 16" << endl;
                    
                    if (_list_only == false)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                    
                case Type::palette8:
                    cout << "palette 256" << endl;

                    if (_list_only == false)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                    
                case Type::image2:
                case Type::image4old:
                case Type::image4:
                case Type::image8: {
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 0, "bitmap Id %d %d bit, %d x %d ", h1, (h0 < 13 ? 4 : 8), width, height);

                    if (_list_only == false)
                    {
                        std::filesystem::path out = _out_dir / (name + " " + std::to_string(i) + ".png");
                        write_png_file(out.string().c_str(), width, height, PNG_COLOR_TYPE_PALETTE, 8, entry->buffer.data, _override_pal ? _override_pal : _active_pal);
                    }
                    break; }
                    
                case Type::video: {
                    bytes[3] = buffer[location + 0];
                    bytes[2] = buffer[location + 1];
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    uint32_t size = (*(uint32_t *)(bytes));
                    char *fliname = (char *)&buffer[location + 4];
                    uint32_t size2 = (*(uint32_t *)(&buffer[location + 30]));
                    uint16_t frames = (*(uint16_t *)(&buffer[location + 36]));

                    log_data(buffer, location - 2, 2, 4, "FLI video (%s) %d bytes [size: %d frames: %d]", fliname, size, size2, frames);

                    if (_list_only == false)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".fli"), entry->buffer);
                    }
                    break; }
                    
                case Type::draw: {
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
                        
                        cout << "  cmd: " << std::dec << std::setw(3) << (int)cmd << " index: " << std::dec << std::setw(3) << (int)index << " at: " << std::dec << x << " x " << std::dec << y << " order: " << std::dec << o << endl;
                    }
                    
                    if (_list_only == false)
                    {
                        std::filesystem::path out = _out_dir / (name + " " + std::to_string(i) + " (draw cmd)" + ".png");
                        write_png_file(out.string().c_str(), 320, 200, PNG_COLOR_TYPE_PALETTE, 8, entry->buffer.data, _override_pal ? _override_pal : _active_pal);
                    }
                    break; }
                    
                case Type::rectangle:
                    // NOTE: following 4 bytes describing size of black rectangle
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 4, "rectangle Id %d, %d x %d ", h1, width, height);
                    break;

                case Type::unknown12:
                    // NOTE: following 12 bytes of unknown data
                    log_data(buffer, location - 2, 2, 12, "12 bytes of unknown data ");
                    break;

                default:
                    
                    log_data(buffer, location - 2, 2, 8, "unknown ");
                    break;
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
