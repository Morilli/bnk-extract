#ifndef _WPK_H
#define _WPK_H

#include "defs.h"

void extract_wpk_file(char* wpk_path, StringHashes* string_hashes, char* output_path, bool delete_wems);

#endif
