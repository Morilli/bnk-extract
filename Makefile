sound: defs.h general_utils.h bin.h wpk.h sound.c wpk.c general_utils.c bin.c
	gcc -Wall -Wextra -pedantic -g -std=gnu18 sound.c bin.c wpk.c general_utils.c  -o sound
