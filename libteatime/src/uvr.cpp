#include <vector>
#include <string>
#include <filesystem>
#include <stdio.h>
namespace fs = std::filesystem;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "logging.hpp"

struct COLOR {
	uint8_t R, G, B, A;
};
static_assert(sizeof(COLOR) == 4);

bool uvr_extract(fs::path fileIn, fs::path fileOut) {

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
		LOGWAR("the UVR resolution is incorrect for PSP, exceeding max of 512x512 (%dx%d)", (int)width, (int)height);
		return false;
	}

	std::vector<COLOR> image(width * height);

	if (colorMode == 2) {
		LOGINF("ABGR4444 color mode");
	}
	else if (colorMode == 3) {
		LOGINF("ABGR8888 color mode");
	}
	else {
		LOGWAR("no known color mode (%d/0x%02X)", (int)colorMode, (int)colorMode);
	}

	auto readColor = [&]() {
		COLOR pix = { 0, 0, 0, 0 };
        if (colorMode == 1) {
            uint16_t tmp;
			fread(&tmp, 2, 1, fi);

			pix.R = (255 / 31) * ((tmp >> 0) & 0x3f);
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
		return pix;
	};

	if (imageMode == 0x86) {
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
	else if (imageMode == 0x8a || imageMode == 0x8c) {
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
	else {
		LOGWAR("unknown image mode (%d/0x%02X)", imageMode, imageMode);
	}

	stbi_write_png(fileOut.u8string().c_str(), width, height, 4, image.data(), width * 4);

    return true;
}
