//
//  utils.hpp
//  silm-extract
//
//  Created on 12.08.2022.
//

#ifndef utils_hpp
#define utils_hpp

#include <string>

namespace utils {

    std::string get_file_name(std::string filePath, bool withExtension = true, char seperator = '/');
    std::string get_file_ext(std::string filePath);
}

#endif /* utils_hpp */
