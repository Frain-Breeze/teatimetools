#include "logging.hpp"
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static const int MAX_MSG_LENGTH = 2048;

static const int INDENT_STEP_SIZE = 4;

namespace logging{

    static uint32_t indent_level = 0;

    static uint32_t prev_count = 0;
    static char* prev_msg = (char*)malloc(MAX_MSG_LENGTH);
    static char* this_msg = (char*)malloc(MAX_MSG_LENGTH);

    void indent(){
        indent_level += INDENT_STEP_SIZE;
    }

    void undent(){
        if(INDENT_STEP_SIZE == 0){ return; }
        indent_level -= INDENT_STEP_SIZE;
    }

    void log_basic(const char* str, ...){
        va_list all_varg;
        va_start(all_varg, str);
        vsprintf(this_msg, str, all_varg);
        va_end(all_varg);

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
}
