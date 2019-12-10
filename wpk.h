#ifndef _WPK_H
#define _WPK_H

#include "defs.h"

void extract_wpk_file(char* wpk_path, struct string_file_hash* string_hashes, uint32_t amount_of_strings, char* output_path);

#endif
