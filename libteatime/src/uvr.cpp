#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
#include <memory.h>
#include <unordered_set>
namespace fs = std::filesystem;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

namespace WuQuant {
extern "C" {
#include "WuQuant.h"
}}

#include "teaio_file.hpp"
#include "logging.hpp"
#include "kmeans.hpp"
#include "uvr.hpp"

extern void dxt1_decompress_image(size_t width, size_t height, const uint8_t* in_data, COLOR* out_data); //see psp_dxt1.cpp


bool uvr_repack(const fs::path& fileIn, const fs::path& fileOut, int palette_colors) {
    int width = 0, height = 0, channels = 4;
	
    uint8_t* imgdata = stbi_load(fileIn.u8string().c_str(), &width, &height, &channels, 4);

    {
        const auto is_pow_of_two = [](int num) -> bool {
            if(num == 0) { return false; }
            while(num != 1) {
                num = num / 2;
                if(num % 2 != 0 && num != 1) { return false; }
            }
            return true;
        };
        
        if(!is_pow_of_two(width)) {
            //TODO: determine if padding up width is a good choice
            LOGWAR("input image is %d pixels wide, which is not a power of two. the PSP requires pow2-wide images");
        }
        if(!is_pow_of_two(height)) {
            //TODO: pad up automatically
            LOGWAR("input image is %d pixels high, which is not a power of two. the PSP requires pow2-tall images");
        }
    }

    std::vector<uint8_t> indices;
    std::vector<KCOL> palette;

    //check if the amount of colors in the image is below 256 (or 16). if so, don't do kmeans at all
    indices.resize(width * height);
    palette.resize(0);
    bool should_do_kmeans = false;
	bool should_do_wuquant = false;
    for(int i = 0; i < width * height; i++) {
        bool already_exists = false;
        for(int a = 0; a < palette.size(); a++) {
            if(*(uint32_t*)&palette[a] == *(uint32_t*)&imgdata[i * 4]) {
                indices[i] = a;
                already_exists = true;
                break;
            }
        }

        if(!already_exists) {
            if(palette.size() == palette_colors) {
                LOGWAR("applying lossy WuQuant algorithm to reduce palette to %d colors", palette_colors);
                //should_do_kmeans = true;
				should_do_wuquant = true;
                goto past_color_indexing;
            }

            KCOL ncol;
            ncol.r = imgdata[(i * 4) + 0];
            ncol.g = imgdata[(i * 4) + 1];
            ncol.b = imgdata[(i * 4) + 2];
            ncol.a = imgdata[(i * 4) + 3];
            palette.push_back(ncol);

            indices[i] = palette.size() - 1;
        }
    }
past_color_indexing:

	//should_do_kmeans = true;
	//should_do_wuquant = true;
    if(should_do_kmeans) {
        std::vector<KCOL> img_in_vec(width * height);
        memcpy(img_in_vec.data(), imgdata, width * height * 4);
        kmeans(img_in_vec, width, height, indices, palette, palette_colors, 0, 1);
    }
    if(should_do_wuquant) {
		WuQuant::Quantizer* quantizer = WuQuant::Create();
		//
		palette.resize(256);
		int colors = palette_colors;
		WuQuant::Quantize(quantizer, (unsigned int*)imgdata, (unsigned int*)palette.data(), &colors, width, height, (char*)indices.data(), 0);
		WuQuant::Destroy(quantizer);
		palette.resize(palette_colors);
	}

    stbi_image_free(imgdata);

    uint8_t* out_data = (uint8_t*)malloc(width * height);

    //TODO: implement 16-color palette stuff too
    palette.resize(palette_colors);

	if(palette_colors == 256) {
		int segWidth = 16;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		int data_offs = 0;

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						out_data[data_offs] = indices[pixelI];
						data_offs++;
					}
				}
			}
		}
	}
	else if(palette_colors == 16) {
		int segWidth = 32;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);
		int data_offs = 0;
		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j+=2) {
						
						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						
						out_data[data_offs] = (indices[pixelI] & 0xf);
						out_data[data_offs] |= ((indices[pixelI+1] << 4) & 0xf0);
						data_offs++;
					}
				}
			}
		}
	}
    //FILE* out_debug = fopen("out.bin", "wb");
    //fwrite(out_data.data(), out_data.size(), 1, out_debug);
    //fwrite(palette.data(), 256*4, 1, out_debug);
    //fclose(out_debug);

    FILE* fo = fopen(fileOut.u8string().c_str(), "wb");
    if(!fo) { LOGERR("couldn't open %s for writing!", fileOut.u8string().c_str()); return false; }

    //TODO: errors etc
    fwrite("GBIX\x8\0\0\0\0\0\0\0\0\0\0\0UVRT", 20, 1, fo);
    uint32_t dataSize = (width * height) + (palette.size() * sizeof(KCOL));
    fwrite(&dataSize, 4, 1, fo);
    uint8_t colorMode = 3;
	uint8_t imageMode = 0;
	switch(palette_colors){
		case 16: imageMode = 0x88; break;
		case 256: imageMode = 0x8C; break;
		default: { LOGERR("unimplemented color amount set"); fclose(fo); free(out_data); return false; }
	}
    fwrite(&colorMode, 1, 1, fo);
    fwrite(&imageMode, 1, 1, fo);
    fwrite("\0\0", 2, 1, fo);
    fwrite(&width, 2, 1, fo);
    fwrite(&height, 2, 1, fo);
    fwrite(palette.data(), palette.size() * sizeof(KCOL), 1, fo);
	switch(palette_colors){
		case 16: fwrite(out_data, (width * height) / 2, 1, fo); break;
		case 256: fwrite(out_data, width * height, 1, fo); break;
		default: { LOGERR("unimplemented color amount set"); fclose(fo); free(out_data); return false; }
	}
    
    fclose(fo);

    free(out_data);

    return true;
}

bool uvr_extract(const fs::path& fileIn, const fs::path& fileOut) {

	//FILE* fi = fopen(fileIn.u8string().c_str(), "rb");
	Tea::FileDisk fi_disk;
	fi_disk.open(fileIn.u8string().c_str(), Tea::Access_read);
	fi_disk.endian(Tea::Endian::little);
	Tea::File& fi = fi_disk; //to expose templated things
	
	fi.skip(20);

	uint32_t dataSize;
	uint8_t colorMode;
	uint8_t imageMode;

	fi.read(dataSize);
	fi.read(colorMode);
	fi.read(imageMode);
	fi.skip(2);

	uint16_t width, height;
	fi.read(width);
	fi.read(height);
	
	//if (width > 512 || height > 512) {
	//	LOGWAR("the UVR resolution (%dx%d) is incorrect for PSP, exceeding max of 512x512 , but we'll still continue", (int)width, (int)height);
	//}

	std::vector<COLOR> image(width * height);

	std::string str_colormode;
	std::string str_imagemode;
	
	str_imagemode = std::to_string(imageMode); //TODO: come up with fancy names
	
	switch(colorMode) {
		case 0: str_colormode = "ARGB1555"; break;
		
		case 33:
		case 1: str_colormode = "RGB655"; break;
		
		case 34:
		case 2: str_colormode = "ARGB4444"; break;
		
		case 35: //???
		case 3: str_colormode = "ARGB8888"; break;
		
		case 42:
		case 10: str_colormode = "DXT1"; break;
		
		default: {
			LOGERR("unknown color mode %d (0x%02x)", (int)colorMode, (int)colorMode);
			return false;
		}
	}

	LOGVER("image mode %d (0x%02x)", (int)imageMode, (int)imageMode);
	LOGVER("color mode %d (0x%02x)", (int)colorMode, (int)colorMode);

	auto readColor = [&]() {
		COLOR pix = { 0, 0, 0, 0 };
        if (colorMode == 0) {
            uint16_t tmp;
			fi.read(tmp);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x1f);
			pix.G = (255 / 31) * ((tmp >> 5) & 0x1f);
			pix.B = (255 / 31) * ((tmp >> 10) & 0x1f);
			pix.A = (tmp >> 15) ? 0xFF : 0;
        }
        else if (colorMode == 1 || colorMode == 33) {
            uint16_t tmp;
			fi.read(tmp);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x3f); //TODO: should this be div by 31 too?
			pix.G = (255 / 31) * ((tmp >> 6) & 0x1f);
			pix.B = (255 / 31) * ((tmp >> 11) & 0x1f);
			pix.A = 0xFF;
        }
		else if (colorMode == 2 || colorMode == 34) {

			uint16_t tmp;
			fi.read(tmp);

			pix.R = (255 / 15) * ((tmp >> 0) & 0xf);
			pix.G = (255 / 15) * ((tmp >> 4) & 0xf);
			pix.B = (255 / 15) * ((tmp >> 8) & 0xf);
			pix.A = (255 / 15) * ((tmp >> 12) & 0xf);
		}
		else if (colorMode == 3 || colorMode == 35) {
			fi.read(&pix.R, 1);
			fi.read(&pix.G, 1);
			fi.read(&pix.B, 1);
			fi.read(&pix.A, 1);
		}
		else if (colorMode == 0x0A || colorMode == 42) { //DXT1, why not right?
			LOGERR("this should never be called. (DXT1)");
        }
        else if (colorMode == 0x0C) { //TODO: DXT3/5? see http://lukasz.dk/mirror/forums.ps2dev.org/viewtopicecdf-2.html?t=3960
			LOGERR("this should never be called. (DXT?)");
		}
        
        return pix;
    };

    if (imageMode == 0x80 || imageMode == 0xA0) {
		//linear
		if(colorMode == 0x0A || colorMode == 42) {
			std::vector<uint8_t> big_data(dataSize);
			size_t toread = dataSize;
			if(dataSize > fi.size() - fi.tell()) { //maybe dataSize includes metadata, TODO investigate
				LOGWAR("less file remaining than datasize: dataSize=%d, file remaining=%d", dataSize, fi.size() - fi.tell());
				toread = fi.size() - fi.tell();
			}
			
			if(!fi.read(big_data.data(), toread)) {
				LOGERR("read error");
			}
			
			image.resize(width * height);
			
			dxt1_decompress_image(width, height, big_data.data(), image.data());
		}
		else {
			int segWidth = 4;
			int segHeight = 8;
			int segsX = (width / segWidth);
			int segsY = (height / segHeight);
			
			for (int segY = 0; segY < segsY; segY++) {
				for (int segX = 0; segX < segsX; segX++) {
					for (int l = 0; l < segHeight; l++) {
						for (int j = 0; j < segWidth; j++) {
							COLOR color = readColor();
							
							int absX = j + segX * segWidth;
							int absY = l + segY * segHeight;
							int pixelI = absY * width + absX;
							image[pixelI].R = color.R;
							image[pixelI].G = color.G;
							image[pixelI].B = color.B;
							image[pixelI].A = color.A;
						}
					}
				}
			}
		}
	}
	else if (imageMode == 0x86 || imageMode == 0xA8 || imageMode == 0xA6) {
		int segWidth = 32;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j+=2) {
						uint8_t data;
						fi.read(data);

						COLOR color = palette[data & 0xf];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;

						color = palette[data >> 4];

						pixelI++;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else if (imageMode == 0x8A || imageMode == 0x8c || imageMode == 0xAC || imageMode == 0xAA) {
		int segWidth = 16;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 256; i++) {
			palette.push_back(readColor());
		}

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						uint8_t data;
						fi.read(data);

						COLOR color = palette[data];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else if (imageMode == 0x88) {
        int segWidth = 32;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j+=2) {
						uint8_t data;
						fi.read(data);

						COLOR color = palette[data & 0xf];

						int absX = j + segX * segWidth;
						int absY = l + segY * segHeight;
						int pixelI = absY * width + absX;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;

						color = palette[data >> 4];

						pixelI++;
						image[pixelI].R = color.R;
						image[pixelI].G = color.G;
						image[pixelI].B = color.B;
						image[pixelI].A = color.A;
					}
				}
			}
		}
	}
	else {
		LOGERR("unknown image mode (%d/0x%02X)", imageMode, imageMode);
		return false;
	}

	stbi_write_png(fileOut.u8string().c_str(), width, height, 4, image.data(), width * 4);

    fi.close(); //auto-called, but doesn't hurt
	
	LOGOK("unpacked image of size %dx%d, with color mode %s and image mode %s", width, height, str_colormode.c_str(), str_imagemode.c_str());

    return true;
}
