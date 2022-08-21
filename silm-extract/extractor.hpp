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

enum alis_platform {

    atari,
    falcon,
    amiga,
    aga,
    mac,
    dos
};

using std::filesystem::path;

enum Type {
    
    none        = 0,
    image2      = 1,
    image4old   = 2,
    image4      = 3,
    image8      = 4,
    video       = 5,
    palette4    = 6,
    palette8    = 7,
    draw        = 8,
    rectangle    = 9,
    unknown12   = 10,
    unknown     = 11
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
    Entry(Type t, uint32_t p, const Buffer& b) { type = t; position = p; buffer = b; };

    Type type;
    uint32_t position;
    Buffer buffer;
};


class extractor {
    
public:
    
    extractor(alis_platform platform);
    extractor(const path& output, char *palette = NULL, bool force_tc = false, bool list_only = false);
    ~extractor();
    
    bool is_script(const path& file);
    
    void extract_dir(const path& path);
    void extract_file(const path& file);
    void extract_buffer(const std::string& name, uint8_t *buffer, int length);

private:
    
    void set_palette(Buffer& script, uint32_t address, uint32_t entries);

    Entry *get_entry_data(Buffer& script, uint32_t address, uint32_t entries, uint32_t index);

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
