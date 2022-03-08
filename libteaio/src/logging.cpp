#include "logging.hpp"
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <array>

static const int MAX_MSG_LENGTH = 2048;

static const int INDENT_STEP_SIZE = 2;

namespace logging{

	static uint64_t messages_sent = 0;
	
	static uint32_t indent_level = 0;

	static uint32_t prev_count = 0;
	static char* prev_msg = (char*)memset(malloc(MAX_MSG_LENGTH), 0, MAX_MSG_LENGTH);
	static char* this_msg = (char*)memset(malloc(MAX_MSG_LENGTH), 0, MAX_MSG_LENGTH);

	static std::array<bool, (size_t)LEVEL::LEVEL_ITEM_COUNT> channels;

	uint64_t count() { return messages_sent; }
	
	void indent(){
		indent_level += INDENT_STEP_SIZE;
	}

	void undent(){
		if(INDENT_STEP_SIZE == 0){ return; }
		indent_level -= INDENT_STEP_SIZE;
	}

	void set_channel(LEVEL lvl, bool state){
		channels[(size_t)lvl] = state;
	}

	void log_basic_valist(const char* str, va_list all_varg){
		messages_sent++;
		
		vsnprintf(this_msg, MAX_MSG_LENGTH, str, all_varg);

		if (strcmp(this_msg, prev_msg)) // mismatch
		{
			strncpy(prev_msg, this_msg, MAX_MSG_LENGTH);
			fprintf(stderr, prev_count ? "\n" : "");
			fprintf(stderr, "%*s%s\r", indent_level, "", this_msg);
			prev_count = 1;
		}
		else { // match; repeated message
			fprintf(stderr, "%*s%s   [x%u]\r", indent_level, "", this_msg, ++prev_count);
		}
		fflush(stderr);
	}

	void log_basic(const char* str, ...){
		va_list all_varg;
		va_start(all_varg, str);
		log_basic_valist(str, all_varg);
		va_end(all_varg);
	}

	void log_advanced(LEVEL lvl, const char* str, ...){
		if(channels[(size_t)lvl] == true){
			va_list all_varg;
			va_start(all_varg, str);
			log_basic_valist(str, all_varg);
			va_end(all_varg);
		}
	}
}
