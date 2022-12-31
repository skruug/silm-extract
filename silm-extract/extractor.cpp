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
    if (buffer[0] == 0xfe)
    {
        return buffer[1] == 0x00 ? 32 : (buffer[1] + 1) * 3;
    }
    else if (buffer[0] == 0xff)
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

int compare_arrays(const uint8_t *a, const uint8_t *b, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return 0;
    }

    return 1;
}

bool extractor::find_assets(const std::string& name, const uint8_t *buffer, int length, uint32_t& address, uint32_t& entries)
{
    uint8_t bytes[4];
    uint32_t location;

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
                
                location = location + 2;
                bytes[3] = buffer[location + 0];
                bytes[2] = buffer[location + 1];
                bytes[1] = buffer[location + 2];
                bytes[0] = buffer[location + 3];
                a = (*(uint32_t *)(bytes));
                
                cout << "Found " << std::dec << e << " PCM sound samples (" << a << ")" << endl;
                
                location = location + 4;
                for (int idx = location; idx < length; idx ++)
                {
                    if (buffer[idx] != 0)
                    {
                        // address
                        
                        location = idx - (2 + (idx % 2));
                        
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
                                // cout << "   sample doesn't fit!" << endl;
                                reset = true;
                                break;
                            }
                            
                            int typeA = buffer[location + a + 0];
                            int typeB = buffer[location + a + 1];

                            int typeC = buffer[location + a + 4];
                            int typeD = buffer[location + a + 5];

                            bytes[3] = buffer[location + a + 2 + 0];
                            bytes[2] = buffer[location + a + 2 + 1];
                            bytes[1] = buffer[location + a + 2 + 2];
                            bytes[0] = buffer[location + a + 2 + 3];
                            uint32_t len = (*(uint32_t *)(bytes)) - 1;

                            cout << "   sample " << std::dec << s + 1 << " [0x" << std::hex << std::setw(6) << std::setfill('0') << a + location << "] data " << std::dec << std::setw(2) << std::setfill('0') << typeA << ", " <<  std::dec << std::setw(2) << std::setfill('0') << typeB << ", " << std::dec << std::setw(2) << std::setfill('0') << typeC << ", " <<  std::dec << std::setw(2) << std::setfill('0') << typeD << " length " << std::dec << len << " b" << endl;
                            
                            if ((uint32_t)(location + a + len) >= length || len >= length)
                            {
                                // cout << "   sample doesn't fit!" << endl;
                                reset = true;
                                break;
                            }

                            // save
                            
                            if (typeA == 0)
                            {
                                // patern

                                std::filesystem::path filename = std::filesystem::path(_out_dir) / (name + " " + std::to_string(s) + ".pattern");
                                
                                FILE *fp = fopen(filename.string().c_str(), "wb");
                                if (!fp)
                                    abort();
                                
                                uint8_t sample[len];
                                memcpy(sample, buffer + location + a + 2 + 4, len);
                                if (fwrite(sample, len, 1, fp) != 1)
                                    abort();
                                
                                fclose(fp);
                            }
                            else
                            {
                                // sample
                                
                                std::filesystem::path filename = std::filesystem::path(_out_dir) / (name + " " + std::to_string(s) + ".wav");
                                
                                FILE *fp = fopen(filename.string().c_str(), "wb");
                                if (!fp)
                                    abort();
                                
                                if (typeB < 3 || typeB > 23)
                                {
                                    typeB = 8;
                                }
                                
                                int res = write_wav_header(fp, typeB * 1000, len);
                                if (res)
                                    abort();
                                
                                uint8_t sample[len];
                                memcpy(sample, buffer + location + a + 2 + 4, len);
                                
                                // try to find out if sample is signed or unsigned
                                
                                int smp_signed = 0;
                                int smp_unsigned = 0;

                                for (int b = 0; b < len; b++)
                                {
                                    uint8_t tui = sample[b];
                                    uint8_t tsi = sample[b] ^ 0x80;

                                    if (tui == tsi)
                                    {
                                        
                                    }
                                    else if (tui > tsi)
                                    {
                                        smp_unsigned++;
                                    }
                                    else
                                    {
                                        smp_signed++;
                                    }
                                }

                                if (smp_unsigned > smp_signed)
                                {
                                    // convert from signed to unsigned PCM
                                    // NOTE: wav doesn't support signed 8 bit sample

                                    for (int b = 0; b < len; b++)
                                    {
                                        sample[b]  ^= 0x80;
                                    }
                                }
                                    
                                if (fwrite(sample, len, 1, fp) != 1)
                                    abort();

                                fclose(fp);
                            }
                            
                            location += 4;
                        }
                        
                        if (reset == false)
                            break;
                        
                        reset = false;
                    }
                }
                
                return false;
            }
        }
    }

    cout << "Can't found address block";
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
        else if (h0 == 0xfe && h1 == 0x0f)
        {
            get_entry_data(script, address, entries, i);
            return;
        }
        else if (h0 == 0xfe && h1 == 0x1f)
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
    if (h0 == 0xfe)
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
    else if (h0 == 0xff && h1 == 0)
    {
        uint8_t *data = new uint8_t[320 * 200];
        memset(data, 0, 320 * 200);

        // clear screen?
        return (_entry_map[index] = new Entry(data_type::draw, location, Buffer(data, 320 * 200)));
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

            bytes[1] = script[b * 8 + location + 4];
            bytes[0] = script[b * 8 + location + 5];
            int16_t d = (*(int16_t *)(bytes));

            bytes[1] = script[b * 8 + location + 6];
            bytes[0] = script[b * 8 + location + 7];
            int16_t y = (*(int16_t *)(bytes));
            
            if (idx >= 0 && idx < entries)
            {
                Entry *entry = get_entry_data(script, address, entries, idx);
                if (entry->type == data_type::image2 || entry->type == data_type::image4old || entry->type == data_type::image4 || entry->type == data_type::image8)
                {
                    bytes[1] = script[entry->position + 0];
                    bytes[0] = script[entry->position + 1];
                    width = *(uint16_t *)(bytes) + 1;

                    bytes[1] = script[entry->position + 2];
                    bytes[0] = script[entry->position + 3];
                    height = *(uint16_t *)(bytes) + 1;

                    int xx = 1 + x - ((width + 1) / 2);
                    if (xx < minX)
                        minX = xx;
                    
                    int yy = 200 - (d + y + ((height + 1) / 2));
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
        if (minX < 0 || maxX >= 320)
        {
            modX = 160 - (((maxX - minX) / 2) + minX);
        }

        if (minY < 0 || maxY >= 200)
        {
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

                bytes[1] = script[entry->position + 0];
                bytes[0] = script[entry->position + 1];
                width = *(uint16_t *)(bytes) + 1;

                bytes[1] = script[entry->position + 2];
                bytes[0] = script[entry->position + 3];
                height = *(uint16_t *)(bytes) + 1;

                int xx = 1 + x - ((width + 1) / 2);
                xx += modX;
                
                int yy = 200 - (d + y + ((height + 1) / 2));
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
                
                if (vt + vf >= 200)
                {
                    vt = 200 - vf;
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
                
                if (ht + hf >= 320)
                {
                    ht = 320 - hf;
                }
                
                uint8_t *layer = layers[d];
                
                if (entry->type == data_type::image2 || entry->type == data_type::image4old || entry->type == data_type::image4 || entry->type == data_type::image8)
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
                                uint8_t *ptr = layer + xx + w + ((yy + h) * 320);
                                *ptr = color;
                            }
                        }
                    }
                }
                else if (entry->type == data_type::rectangle)
                {
                    for (int h = vs; h < vt; h++)
                    {
                        memset(layer + hf + ((yy + h) * 320), 0, ht);
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
        
        return (_entry_map[index] = new Entry(data_type::draw, location, Buffer(data, 320 * 200)));
    }
    else if (h0 == 0x00 && script[location + 0] == 0x00 && script[location + 1] == 0x0f && script[location + 2] == 0x00 && script[location + 3] == 0x00)// (h1 == 0x1a || h1 == 0x1b || h1 == 0x1c || h1 == 0x1d || h1 == 0x1e || h1 == 0x2C || h1 == 0x6C))
    {
        // NOTE: following 12 bytes of unknown data
        return (_entry_map[index] = new Entry(data_type::unknown12, location, Buffer()));
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

        return (_entry_map[index] = new Entry(data_type::video, location, Buffer(data, size)));
    }
    else if (h0 == 0x01)
    {
        return (_entry_map[index] = new Entry(data_type::rectangle, location, Buffer()));
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

                    // int clear = script[location + 5];
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

                    return (_entry_map[index] = new Entry(data_type::image4, location, Buffer(data, width * height)));
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

                    return (_entry_map[index] = new Entry(data_type::image8, location, Buffer(data, width * height)));
                }
            }
            else if ((h0 == 0x00 || h0 == 0x02) && script.size - location > width * height / 2)
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

                    return (_entry_map[index] = new Entry(data_type::image4old, location, Buffer(data, width * height)));
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

                return (_entry_map[index] = new Entry(data_type::image2, location, Buffer(data, width * height)));
            }
        }
    }
    
    return (_entry_map[index] = new Entry());
}

void extractor::extract_buffer(const std::string& name, uint8_t *buffer, int length, uint32_t etype, vector<uint8_t *> *pal_overrides)
{
    uint8_t bytes[4];
    uint32_t value = 0;
    uint32_t location = 0;
    uint32_t address = 0;
    uint32_t entries = 0;

    // find adresses for all assets in file
    
    if (find_assets(name, buffer, length, address, entries))
    {
        cout << " containing " << std::dec << entries << " assets" << endl;
    }
    else
    {
        return;
    }
    
    // identify known types and save them
    // (for the moment bitmaps, rectangles, palettes, draw command and fli video are recognized)
    
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
            entryList[i] = get_entry_data(script, address, entries, i);
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

            Entry *entry = entryList[i];//get_entry_data(script, address, entries, i);
            switch (entry->type)
            {
                case data_type::palette4:
                    cout << "palette 16" << endl;
                    
                    if (_list_only == false && etype & ex_palette)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                    
                case data_type::palette8:
                    cout << "palette 256" << endl;

                    if (_list_only == false && etype & ex_palette)
                    {
                        write_buffer(std::filesystem::path(_out_dir) / (name + " " + std::to_string(i) + ".act"), entry->buffer);
                    }
                    break;
                    
                case data_type::image2:
                case data_type::image4old:
                case data_type::image4:
                case data_type::image8: {
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 0, "bitmap Id %d %d bit, %d x %d ", h1, (h0 < 13 ? 4 : 8), width, height);

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
                    break; }
                    
                case data_type::video: {
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
                    break; }
                    
                case data_type::draw: {
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
                        
                        char type[16] = "";
                        switch (e->type)
                        {
                            case none:
                                strcpy(type, "none");
                                break;
                            case image2:
                                strcpy(type, "image 2 bit");
                                break;
                            case image4old:
                                strcpy(type, "image 4 bit v1");
                                break;
                            case image4:
                                strcpy(type, "image 4 bit v2");
                                break;
                            case image8:
                                strcpy(type, "image 8 bit");
                                break;
                            case video:
                                strcpy(type, "video");
                                break;
                            case palette4:
                                strcpy(type, "16c palette");
                                break;
                            case palette8:
                                strcpy(type, "256c palette");
                                break;
                            case draw:
                                strcpy(type, "draw");
                                break;
                            case rectangle:
                                strcpy(type, "rectangle");
                                break;
                            case unknown12:
                                strcpy(type, "unknown 12 bit");
                                break;
                            case unknown:
                                strcpy(type, "unknown");
                                break;
                        };
                        
                        bytes[1] = script[e->position + 0];
                        bytes[0] = script[e->position + 1];
                        width = *(uint16_t *)(bytes) + 1;

                        bytes[1] = script[e->position + 2];
                        bytes[0] = script[e->position + 3];
                        height = *(uint16_t *)(bytes) + 1;
                       
                        cout << "  cmd: " << std::dec << std::setw(3) << (int)cmd << " index: " << std::dec << std::setw(3) << (int)index << " type: " << type << " x " << std::dec << x << " y " << std::dec << y  << " w " << std::dec << width << " h " << std::dec << height << " order: " << std::dec << o << endl;
                    }
                    
                    // TODO: do composition in 32 bit
                    if (_list_only == false && etype & ex_draw)
                    {
                        std::filesystem::path out = _out_dir / (name + " " + std::to_string(i) + " (draw cmd)" + ".png");

                        if (_force_tc)
                        {
                            uint8_t *data = new uint8_t[320 * 200 * 4];
                    
                            int to = 0;
                            for (int x = 0; x < 320 * 200; x++)
                            {
                                int index = entry->buffer.data[x];
                                data[to++] = active_pal[index * 3 + 0];
                                data[to++] = active_pal[index * 3 + 1];
                                data[to++] = active_pal[index * 3 + 2];
                                data[to++] = 0xff;
                            }
                    
                            write_png_file(out.string().c_str(), 320, 200, PNG_COLOR_TYPE_RGBA, 8, data);
                        }
                        else
                        {
                            write_png_file(out.string().c_str(), 320, 200, PNG_COLOR_TYPE_PALETTE, 8, entry->buffer.data, active_pal);
                        }
                    }
                    break; }
                    
                case data_type::rectangle:
                    // NOTE: following 4 bytes describing size of black rectangle
                    bytes[1] = buffer[location + 0];
                    bytes[0] = buffer[location + 1];
                    width = *(uint16_t *)(bytes) + 1;
    
                    bytes[1] = buffer[location + 2];
                    bytes[0] = buffer[location + 3];
                    height = *(uint16_t *)(bytes) + 1;
                    log_data(buffer, location - 2, 2, 4, "rectangle Id %d, %d x %d ", h1, width, height);
                    break;

                case data_type::unknown12:
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
