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

#include "logging.hpp"
#include "kmeans.hpp"

struct COLOR {
	uint8_t R, G, B, A;
};
static_assert(sizeof(COLOR) == 4, "COLOR struct is not the correct size");

bool uvr_repack(const fs::path& fileIn, const fs::path& fileOut) {
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

    std::vector<int> indices;
    std::vector<KCOL> palette;

    //check if the amount of colors in the image is below 256 (or 16). if so, don't do kmeans at all
    indices.resize(width * height);
    palette.resize(0);
    bool should_do_kmeans = false;
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
            if(palette.size() == 256) {
                LOGWAR("image doesn't fit within 256 colors, so we will do a LOSSY k-means operation to reduce the amount of colors");
                should_do_kmeans = true;
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

    if(should_do_kmeans) {
        std::vector<KCOL> img_in_vec(width * height);
        memcpy(img_in_vec.data(), imgdata, width * height * 4);
        kmeans(img_in_vec, width, height, indices, palette, 256, 0, 40);
    }

    stbi_image_free(imgdata);

    uint8_t* out_data = (uint8_t*)malloc(width * height);

    //TODO: implement 16-color palette stuff too
    palette.resize(256);

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
                    out_data[data_offs] = (int)indices[pixelI];
                    data_offs++;
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
    uint8_t imageMode = 0x8C;
    fwrite(&colorMode, 1, 1, fo);
    fwrite(&imageMode, 1, 1, fo);
    fwrite("\0\0", 2, 1, fo);
    fwrite(&width, 2, 1, fo);
    fwrite(&height, 2, 1, fo);
    fwrite(palette.data(), palette.size() * sizeof(KCOL), 1, fo);
    fwrite(out_data, width * height, 1, fo);
    fclose(fo);

    free(out_data);

    return true;
}

bool uvr_extract(const fs::path& fileIn, const fs::path& fileOut) {

	FILE* fi = fopen(fileIn.u8string().c_str(), "rb");
	
	fseek(fi, 20, SEEK_SET);

	uint32_t dataSize;
	uint8_t colorMode;
	uint8_t imageMode;

	fread(&dataSize, 4, 1, fi);
	fread(&colorMode, 1, 1, fi);
	fread(&imageMode, 1, 1, fi);
	fseek(fi, 2, SEEK_CUR);

	uint16_t width, height;
	fread(&width, 2, 1, fi);
	fread(&height, 2, 1, fi);
	if (width > 512 || height > 512) {
		LOGWAR("the UVR resolution (%dx%d) is incorrect for PSP, exceeding max of 512x512 , but we'll still continue", (int)width, (int)height);
	}

	std::vector<COLOR> image(width * height);

	if (colorMode == 2) {
		LOGINF("ABGR4444 color mode");
	}
	else if (colorMode == 0) {
        LOGINF("ARGB1555 color mode");
    }
	else if (colorMode == 1) {
		LOGINF("RGB655 color mode");
	}
	else if (colorMode == 3) {
		LOGINF("ABGR8888 color mode");
	}
	else {
		LOGWAR("no known color mode (%d/0x%02X)", (int)colorMode, (int)colorMode);
	}

	LOGINF("color mode %d (0x%02x)", (int)imageMode, (int)imageMode);

	auto readColor = [&]() {
		COLOR pix = { 0, 0, 0, 0 };
        if (colorMode == 0) {
            uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x1f);
			pix.G = (255 / 31) * ((tmp >> 5) & 0x1f);
			pix.B = (255 / 31) * ((tmp >> 10) & 0x1f);
			pix.A = (tmp >> 15) ? 0xFF : 0;
        }
        else if (colorMode == 1) {
            uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x3f); //TODO: should this be div by 31 too?
			pix.G = (255 / 31) * ((tmp >> 6) & 0x1f);
			pix.B = (255 / 31) * ((tmp >> 11) & 0x1f);
			pix.A = 0xFF;
        }
		else if (colorMode == 2) {

			uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 15) * ((tmp >> 0) & 0xf);
			pix.G = (255 / 15) * ((tmp >> 4) & 0xf);
			pix.B = (255 / 15) * ((tmp >> 8) & 0xf);
			pix.A = (255 / 15) * ((tmp >> 12) & 0xf);
		}
		else if (colorMode == 3) {
			fread(&pix.R, 1, 1, fi);
			fread(&pix.G, 1, 1, fi);
			fread(&pix.B, 1, 1, fi);
			fread(&pix.A, 1, 1, fi);
		}
		else if (colorMode == 0x0A) { //TODO: fix this (it's wrong)
            static bool temp_filled = false;
            static uint8_t tmp;
			uint8_t cur_pix;
            if(!temp_filled) {
                fread(&tmp, 1, 1, fi);
                cur_pix = (tmp >> 4);
            }
            else {
                cur_pix = (tmp & 0xF);
            }
            pix.R = (cur_pix & 8) ? 0xFF : 0;
			pix.G = (cur_pix & 4) ? 0xFF : 0;
			pix.B = (cur_pix & 2) ? 0xFF : 0;
			pix.A = (cur_pix & 1) ? 0xFF : 0;
        }
        
        return pix;
    };

    if (imageMode == 0x80) {
		int segWidth = 8;
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
	else if (imageMode == 0x86) {
		int segWidth = 32;
		int segHeight = 8;
		int segsX = (width / segWidth);
		int segsY = (height / segHeight);

		std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		segsY /= 2;

		for (int segY = 0; segY < segsY; segY++) {
			for (int segX = 0; segX < segsX; segX++) {
				for (int l = 0; l < segHeight; l++) {
					for (int j = 0; j < segWidth; j++) {
						uint8_t data;
						fread(&data, 1, 1, fi);

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
	else if (imageMode == 0x8A || imageMode == 0x8c) {
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
						fread(&data, 1, 1, fi);

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
						fread(&data, 1, 1, fi);

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
	/*else if(imageMode == 0x88) {
        std::vector<COLOR>palette;
		for (int i = 0; i < 16; i++) {
			palette.push_back(readColor());
		}

		for(int i = 0; i < width * height / 2; i++) {\
            uint8_t data;
            fread(&data, 1, 1, fi);
            image[i * 2].R = palette[data & 0x0f].R;
            image[i * 2].G = palette[data & 0x0f].G;
            image[i * 2].B = palette[data & 0x0f].B;
            image[i * 2].A = palette[data & 0x0f].A;

            image[(i * 2) + 1].R = palette[(data & 0xf0) >> 4].R;
            image[(i * 2) + 1].G = palette[(data & 0xf0) >> 4].G;
            image[(i * 2) + 1].B = palette[(data & 0xf0) >> 4].B;
            image[(i * 2) + 1].A = palette[(data & 0xf0) >> 4].A;
        }
    }*/
	else {
		LOGWAR("unknown image mode (%d/0x%02X)", imageMode, imageMode);
	}

	stbi_write_png(fileOut.u8string().c_str(), width, height, 4, image.data(), width * 4);

    fclose(fi);

    return true;
}
