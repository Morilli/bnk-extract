CC=gcc
CXX=g++
CFLAGS=-std=gnu18 -g -Wall -Wextra -pedantic -Os -flto
CXXFLAGS=-std=c++11 -g -Wall -Wextra -Wno-unused-function -Os -flto
LDFLAGS=-logg -Wl,--gc-sections

all: unix
mingw: mingw_

unix: VORBIS_LIB=revorb/vorbis/libvorbis_unix.a
unix: bnk-extract

mingw_: VORBIS_LIB=revorb/vorbis/libvorbis_mingw.a
mingw_: bnk-extract.exe

MAIN_HEADERS=defs.h general_utils.h bin.h wpk.h

sound_OBJECTS=bin.o general_utils.o wpk.o sound.o

bin.o: bin.c defs.h
general_utils.o: general_utils.c
wpk.o: wpk.c defs.h general_utils.h ww2ogg/api.h revorb/api.h
sound.o: sound.c $(MAIN_HEADERS)


BIT_STREAM_HEADERS=ww2ogg/Bit_stream.hpp ww2ogg/crc.h ww2ogg/errors.hpp
WWRIFF_HEADERS=ww2ogg/wwriff.hpp $(BIT_STREAM_HEADERS)

ww2ogg_OBJECTS=ww2ogg/ww2ogg.o ww2ogg/wwriff.o ww2ogg/codebook.o ww2ogg/crc.o

ww2ogg/ww2ogg.o: ww2ogg/ww2ogg.cpp $(WWRIFF_HEADERS)
ww2ogg/wwriff.o: ww2ogg/wwriff.cpp ww2ogg/codebook.hpp $(WWRIFF_HEADERS)
ww2ogg/codebook.o: ww2ogg/codebook.cpp ww2ogg/codebook.hpp $(BIT_STREAM_HEADERS)
ww2ogg/crc.o: ww2ogg/crc.c ww2ogg/crc.h

revorb_OBJECTS=revorb/revorb.o
revorb/revorb.o: revorb/revorb.c
	$(CC) $(CFLAGS) -Wno-extra -c $^ -logg -o $@

bnk-extract bnk-extract.exe: $(ww2ogg_OBJECTS) $(revorb_OBJECTS) $(sound_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(VORBIS_LIB) $(LDFLAGS) -o $@


clean:
	rm -f bnk-extract bnk-extract.exe $(sound_OBJECTS) $(ww2ogg_OBJECTS) $(revorb_OBJECTS)
