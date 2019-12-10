sound: defs.h general_utils.h sound.c wpk.c general_utils.c
	gcc -Wall -Wextra -pedantic -g -std=gnu18 sound.c wpk.c general_utils.c  -o sound
