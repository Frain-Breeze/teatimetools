#include "uvr.hpp"
#include <stdint.h>
#include <stddef.h>


//adapted from https://github.com/Benjamin-Dobell/s3tc-dxt-decompression

void dxt1_decompress_block(size_t x, size_t y, size_t width, const uint8_t* in_data, COLOR* out_data) {
	uint32_t code = *(uint32_t*)in_data;
	uint16_t color0 = *(uint16_t*)(in_data + 4);
	uint16_t color1 = *(uint16_t*)(in_data + 6);
	
	uint32_t temp;
	temp = (color0 >> 11) * 255 + 16;
	uint8_t r0 = (uint8_t)((temp/32 + temp)/32);
	temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g0 = (uint8_t)((temp/64 + temp)/64);
	temp = (color0 & 0x001F) * 255 + 16;
	uint8_t b0 = (uint8_t)((temp/32 + temp)/32);
	
	temp = (color1 >> 11) * 255 + 16;
	uint8_t r1 = (uint8_t)((temp/32 + temp)/32);
	temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
	uint8_t g1 = (uint8_t)((temp/64 + temp)/64);
	temp = (color1 & 0x001F) * 255 + 16;
	uint8_t b1 = (uint8_t)((temp/32 + temp)/32);
	
	for(size_t j = 0; j < 4; j++) {
		for(size_t i = 0; i < 4; i++) {
			COLOR final_color;
			uint8_t position_code = (code >> 2 * ((4 * j) + i)) & 0x03;
			
			if(color0 > color1) {
				switch(position_code) {
					case 0: final_color = {r0, g0, b0, 0xFF}; break;
					case 1: final_color = {r1, g1, b1, 0xFF}; break;
					case 2: final_color = {(uint8_t)((2*r0+r1)/3), (uint8_t)((2*g0+g1)/3), (uint8_t)((2*b0+b1)/3), 0xFF}; break;
					case 3: final_color = {(uint8_t)((r0+2*r1)/3), (uint8_t)((g0+2*g1)/3), (uint8_t)((b0+2*b1)/3), 0xFF}; break;
				}
			}
			else {
				switch(position_code) {
					case 0: final_color = {r0, g0, b0, 0xFF}; break;
					case 1: final_color = {r1, g1, b1, 0xFF}; break;
					case 2: final_color = {(uint8_t)((r0+r1)/2), (uint8_t)((g0+g1)/2), (uint8_t)((b0+b1)/2), 0xFF}; break;
					case 3: final_color = {0, 0, 0, 0xFF}; break;
				}
			}
			
			if(x + i < width)
				out_data[(y + j) * width + (x + i)] = final_color;
		}
	}
}

void dxt1_decompress_image(size_t width, size_t height, const uint8_t* in_data, COLOR* out_data) {
	size_t blocks_x = (width + 3) / 4;
	size_t blocks_y = (height + 3) / 4;
	
	for(size_t j = 0; j < blocks_y; j++) {
		for(size_t i = 0; i < blocks_x; i++) {
			dxt1_decompress_block(i * 4, j * 4, width, in_data + (i * 8), out_data);
		}
		in_data += blocks_x * 8;
	}
}