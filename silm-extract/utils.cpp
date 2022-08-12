//
//  utils.cpp
//  silm-extract
//
//  Created on 12.08.2022.
//

#include "utils.hpp"


std::string utils::get_file_name(std::string filePath, bool withExtension, char seperator)
{
    // get last dot position
    std::size_t dotPos = filePath.rfind('.');
    std::size_t sepPos = filePath.rfind(seperator);
    if (sepPos != std::string::npos)
    {
        return filePath.substr(sepPos + 1, (withExtension == false && dotPos != std::string::npos ? dotPos - 1 : filePath.size()) - sepPos);
    }
    
    return "";
}

std::string utils::get_file_ext(std::string filePath)
{
    // get last dot position
    std::size_t dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos)
    {
        return filePath.substr(dotPos + 1, filePath.size() - 1);
    }
    
    return "";
}
