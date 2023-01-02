//
//  extractor.hpp
//  silm-extract
//
//  Created on 08.08.2022.
//

#ifndef extractor_hpp
#define extractor_hpp

#include <filesystem>
#include <map>
#include <png.h>
#include <stdio.h>
#include <string>
#include <vector>

enum alis_platform {

    atari,
    falcon,
    amiga,
    aga,
    mac,
    dos
};

using std::filesystem::path;
using std::vector;

enum data_type {
    
    none        = 0,
    image2      = 1,
    image4ST   = 2,
    image4      = 3,
    image8      = 4,
    video       = 5,
    palette4    = 6,
    palette8    = 7,
    composite   = 8,
    rectangle   = 9,
    sample      = 10,
    pattern     = 11,
    unknown     = 12
};

enum extract_type {
    
    ex_image       = 1 << 0,
    ex_video       = 1 << 1,
    ex_palette     = 1 << 2,
    ex_draw        = 1 << 3,
    ex_rectangle   = 1 << 4,
    ex_sound       = 1 << 5,
    ex_ranges      = 1 << 6,
    ex_everything  = 0xFFFFFFFF
};

struct Buffer {
  
    Buffer() { data = NULL; size = 0; }
    Buffer(uint8_t *d, uint32_t s) { data = d; size = s; }

    uint8_t& operator[] (int x) { return data[x]; }
    
    uint8_t *data;
    uint32_t size;
};

struct Entry {
  
    Entry() { type = none; buffer = Buffer(); };
    Entry(data_type t, uint32_t p, const Buffer& b) { type = t; position = p; buffer = b; };

    data_type type;
    uint32_t position;
    Buffer buffer;
};


class extractor {
    
public:
    
    extractor(alis_platform platform);
    extractor(const path& output, char *palette = NULL, bool force_tc = false, bool list_only = false);
    ~extractor();

    void set_palette(uint8_t *palette);
    void set_out_dir(const path& output);
    
    bool is_script(const path& file);
    
    void extract_dir(const path& path, uint32_t etype = ex_everything);
    void extract_file(const path& file, uint32_t etype = ex_everything, vector<uint8_t *> *pal_overrides = NULL);
    void extract_buffer(const std::string& name, uint8_t *buffer, int length, uint32_t etype, vector<uint8_t *> *pal_overrides = NULL);

private:

    bool find_assets(const uint8_t *buffer, int length, uint32_t& address, uint32_t& entries, uint32_t& mod);

    void set_palette(Buffer& script, uint32_t address, uint32_t entries);

    Entry *get_entry_data(Buffer& script, uint32_t mod, uint32_t address, uint32_t entries, uint32_t index);

    void write_buffer(const std::filesystem::path& path, const Buffer& buffer);
    void write_png_file(const char *filename, int width, int height, png_byte color_type, png_byte bit_depth, uint8_t *data, uint8_t *palette = NULL);

    alis_platform _platform;
    
    std::filesystem::path _out_dir;
    
    uint8_t _default_pal[256 * 3]; // default grayscale pallete
    
    uint8_t *_active_pal;
    uint8_t *_override_pal;
    
    bool _force_tc;
    bool _list_only;
    
    std::map<int, Entry *> _entry_map;
};

#endif /* extractor_hpp */
