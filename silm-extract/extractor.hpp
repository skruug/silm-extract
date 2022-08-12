//
//  extractor.hpp
//  silm-extract
//
//  Created on 08.08.2022.
//

#ifndef extractor_hpp
#define extractor_hpp

#include <stdio.h>
#include <string>
#include <filesystem>

enum alis_platform {

    atari,
    falcon,
    amiga,
    aga,
    mac,
    dos
};

using std::filesystem::path;

class extractor {
    
public:
    
    extractor(alis_platform platform);
    extractor(const path& output, char *palette = NULL, bool force_tc = false, bool list_only = false);
    ~extractor();
    
    bool is_script(const path& file);
    
    void extract_dir(const path& path);
    void extract_file(const path& file);
    void extract_buffer(const std::string& name, const uint8_t *buffer, int length);

private:
    
    alis_platform _platform;
    
    std::filesystem::path _out_dir;
    
    uint8_t _pallette16[256 * 3]; // default 16 colors grayscale pallete
    uint8_t _pallette256[256 * 3]; // default 256 colors grayscale pallete
    
    uint8_t *_paletteOverride;
    
    bool _force_tc;
    bool _list_only;
};

#endif /* extractor_hpp */
