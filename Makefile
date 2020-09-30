CC := gcc
CXX := g++
ifdef DEBUG
	_DEBUG := -DDEBUG
endif
CFLAGS := -std=gnu18 -g -Wall -Wextra -pedantic -Os -flto -ffunction-sections -fdata-sections $(_DEBUG)
CXXFLAGS := -g -Wall -Wextra -Wno-unused-function -Os -flto -ffunction-sections -fdata-sections
LDFLAGS := -l:libogg.a -l:libvorbis.a -Wl,--gc-sections
target := bnk-extract

ifeq ($(OS),Windows_NT)
    LDFLAGS := $(LDFLAGS) -static
    target := $(target).exe
endif

all: $(target)
strip: LDFLAGS := $(LDFLAGS) -s
strip: all

sound_OBJECTS=general_utils.o bin.o bnk.o extract.o wpk.o sound.o

general_utils.o: general_utils.h defs.h
bin.o: bin.h defs.h list.h
bnk.o: bnk.h bin.h defs.h extract.h
extract.c: extract.h defs.h general_utils.h
wpk.o: wpk.h bin.h defs.h extract.h
sound.o: bin.h bnk.h defs.h wpk.h

BIT_STREAM_HEADERS=ww2ogg/Bit_stream.hpp ww2ogg/crc.h ww2ogg/errors.hpp
WWRIFF_HEADERS=ww2ogg/wwriff.hpp $(BIT_STREAM_HEADERS)

ww2ogg_OBJECTS=ww2ogg/ww2ogg.o ww2ogg/wwriff.o ww2ogg/codebook.o ww2ogg/crc.o

ww2ogg/ww2ogg.o: ww2ogg/ww2ogg.cpp $(WWRIFF_HEADERS) general_utils.h
ww2ogg/wwriff.o: ww2ogg/wwriff.cpp ww2ogg/codebook.hpp $(WWRIFF_HEADERS) defs.h
ww2ogg/codebook.o: ww2ogg/codebook.cpp ww2ogg/codebook.hpp $(BIT_STREAM_HEADERS) defs.h
ww2ogg/crc.o: ww2ogg/crc.c ww2ogg/crc.h

revorb_OBJECTS=revorb/revorb.o
revorb/revorb.o: revorb/revorb.c defs.h general_utils.h

bnk-extract bnk-extract.exe: $(ww2ogg_OBJECTS) $(revorb_OBJECTS) $(sound_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@


clean:
	rm -f bnk-extract bnk-extract.exe $(sound_OBJECTS) $(ww2ogg_OBJECTS) $(revorb_OBJECTS)
