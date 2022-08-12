//
//  main.cpp
//  silm-extract
//
//  Created on 08.08.2022.
//

extern "C"
{
    #include "depack.h"
}

#include <iostream>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "extractor.hpp"

#define kSEPAppName           "silm-extract"
#define kSEPAppVersion        "0.1"

static void usage() {
    printf("%s v%s\n",
           kSEPAppName, kSEPAppVersion);
    printf("An experimental Silmarils ALIS assets extractor.\n");
    printf("\n");
    printf("Usage:\n");
    printf("  %s <file> | <dir> [options]\n", kSEPAppName);
    printf("\n");
    printf("Options:\n");
    printf("  -h        This help info.\n");
    printf("  -l        List all extractable assets.\n");
    printf("  -o <dir>  Output directory.\n");
    printf("  -p <file> Palette override.\n");
    printf("  -f        Force 32 bit depth for all sprites.\n");
    printf("\n");
}

int main(int argc, const char * argv[])
{
    if (argc > 1)
    {
        path exe = argv[0];
        path current_dir = exe.parent_path();
        
        std::string c0 = argv[1];
        if (c0 == "-h")
        {
            usage();
        }
        else
        {
            path input = c0;
            
            if (std::filesystem::is_regular_file(c0) == false && std::filesystem::is_directory(c0) == false)
            {
                path cc0 = current_dir;
                cc0.append(c0);

                if (std::filesystem::is_regular_file(cc0) || std::filesystem::is_directory(cc0))
                {
                    input = cc0;
                }
                else
                {
                    std::cout << "Invalid input path!" << std::endl;
                    return errno;
                }
            }
            
            path output = input;
            if (std::filesystem::is_directory(output) == false)
            {
                output = output.parent_path();
            }
            
            path palette = "";
            bool force_tc = false;
            bool list_only = false;

            for (int c = 1; c < argc; c++)
            {
                std::string cmd = argv[c];
                if (cmd == "-o" && c + 1 < argc)
                {
                    output = argv[c + 1];
                    if (output.is_absolute() == false)
                    {
                        output = std::filesystem::is_directory(input) ? input : input.parent_path();;
                        output.append(argv[c + 1]);
                    }

                    if (std::filesystem::exists(output) == false)
                    {
                        if (std::filesystem::is_directory(output.parent_path()))
                        {
                            std::filesystem::create_directory(output);
                        }

                        if (std::filesystem::is_directory(output) == false)
                        {
                            std::cout << "Wrong parameter for output directory!" << std::endl;
                            return errno;
                        }
                    }
                        
                    c++;
                }
                
                if (cmd == "-p" && c + 1 < argc)
                {
                    palette = argv[c + 1];
                    if (std::filesystem::is_regular_file(palette) == false)
                    {
                        palette = current_dir;
                        palette.append(argv[c + 1]);

                        if (std::filesystem::is_regular_file(palette) == false)
                        {
                            palette = std::filesystem::is_directory(input) ? input : input.parent_path();
                            palette.append(argv[c + 1]);

                            if (std::filesystem::is_regular_file(palette) == false)
                            {
                                std::cout << "Wrong palette path!" << std::endl;
                                return errno;
                            }
                        }
                    }
                    
                    c++;
                }
                
                if (cmd == "-f")
                {
                    force_tc = true;
                }
                
                if (cmd == "-l")
                {
                    list_only = true;
                }
            }

            char *paldata = NULL;
            if (std::filesystem::is_regular_file(palette))
            {
                std::ifstream is(palette, std::ifstream::binary);
                if (is)
                {
                    is.seekg (0, is.end);
                    long length = is.tellg();
                    is.seekg (0, is.beg);

                    if (length != 768)
                    {
                        std::cout << "Wrong palette format!" << std::endl;
                        return errno;
                    }
                    
                    paldata = new char [length];
                    is.read(paldata, length);
                    is.close();
                }
            }
            
            extractor ex = extractor(output, paldata, force_tc, list_only);
            if (std::filesystem::is_directory(input))
            {
                ex.extract_dir(input);
            }
            else
            {
                ex.extract_file(input);
            }
        }
        
//        depack((char *)argv[1]);
    }
    else
    {
        usage();
    }
    
    return 0;
}
