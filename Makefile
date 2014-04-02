#
# Unified Makefile for Apple 2 SDL
#
# by James Hammons
# (C) 2005 Underground Software
# This software is licensed under the GPL v3
#

FIND = find
FINDSDL2 := $(shell which $(CROSS)sdl2-config 2> /dev/null)

# Figure out which system we're compiling for, and set the appropriate variables

ifeq "$(CROSS)" ""
OSTYPE   := $(shell uname -a)

# Win32
ifeq "$(findstring Msys,$(OSTYPE))" "Msys"

SYSTYPE    = __GCCWIN32__
EXESUFFIX  = .exe
ICON       = obj/icon.o
SDLLIBTYPE = --libs
MSG        = Win32 on MinGW

# Should catch both 'darwin' and 'darwin7.0'
else ifeq "$(findstring Darwin,$(OSTYPE))" "Darwin"

SYSTYPE    = __GCCUNIX__ -D_OSX_
EXESUFFIX  =
ICON       =
SDLLIBTYPE = --static-libs
MSG        = Mac OS X

# *nix
else ifeq "$(findstring Linux,$(OSTYPE))" "Linux"

SYSTYPE    = __GCCUNIX__
EXESUFFIX  =
ICON       =
SDLLIBTYPE = --libs
MSG        = generic Unix/Linux

# Throw error, unknown OS
else

$(error OS TYPE UNDETECTED)

endif
# Cross compile using MXE under Linux host
else

SYSTYPE    = __GCCWIN32__
EXESUFFIX  = .exe
ICON       = obj/icon.o
SDLLIBTYPE = --libs
MSG        = Win32 under MXE (cross compile)

endif

CC         = $(CROSS)gcc
LD         = $(CROSS)gcc
TARGET     = apple2

SDL_CFLAGS = `$(CROSS)sdl2-config --cflags`
SDL_LIBS   = `$(CROSS)sdl2-config $(SDLLIBTYPE)`
DEFINES    = -D$(SYSTYPE)
GCC_DEPS   = -MMD

# Note that we use optimization level 2 instead of 3--3 doesn't seem to gain much over 2
#CFLAGS   = -MMD -Wall -Wno-switch -O2 -D$(SYSTYPE) -ffast-math -fomit-frame-pointer `sdl2-config --cflags`
#CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -O2 -D$(SYSTYPE) \
# No optimization and w/gcov flags, so that we get an accurate picture from gcov
#CFLAGS   = -MMD -Wall -Wno-switch -D$(SYSTYPE) \
#		-ffast-math -fomit-frame-pointer `sdl2-config --cflags` -fprofile-arcs -ftest-coverage
#CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -D$(SYSTYPE) \
#		-ffast-math -fomit-frame-pointer `sdl2-config --cflags` -fprofile-arcs -ftest-coverage
# No optimization for profiling with gprof...
#CFLAGS   = -MMD -Wall -Wno-switch -D$(SYSTYPE) \
#		-ffast-math `sdl2-config --cflags` -pg -g
#CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -D$(SYSTYPE) \
#		-ffast-math `sdl2-config --cflags` -pg -g
#		-fomit-frame-pointer `sdl2-config --cflags` -g
#		-fomit-frame-pointer `sdl2-config --cflags` -DLOG_UNMAPPED_MEMORY_ACCESSES
CFLAGS   = $(GCC_DEPS) -Wall -Wno-switch $(DEFINES) -ffast-math $(SDL_CFLAGS) -pg -g
CPPFLAGS = $(GCC_DEPS) -Wall -Wno-switch -Wno-non-virtual-dtor $(DEFINES) \
		-ffast-math $(SDL_CFLAGS) -pg -g

LDFLAGS =

#LIBS = -L/usr/local/lib -L/usr/lib `sdl2-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB)
# Link in the gcov library (for profiling purposes)
#LIBS = -L/usr/local/lib -L/usr/lib `sdl2-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB) -lgcov
# Link in the gprof lib
#LIBS = -L/usr/local/lib -L/usr/lib `sdl2-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB) -pg
#LIBS = -L/usr/local/lib -L/usr/lib $(SDL_LIBS) -lstdc++ -lz $(GLLIB) -pg
LIBS = $(SDL_LIBS) -lstdc++ -lz $(GLLIB) -pg

#INCS = -I. -I./src -I/usr/local/include -I/usr/include
INCS = -I. -I./src

OBJS = \
	obj/diskselector.o    \
	obj/font10pt.o        \
	obj/font12pt.o        \
	obj/font14pt.o        \
	obj/gui.o             \
                          \
        obj/apple2-icon-64x64.o \
	obj/applevideo.o      \
	obj/ay8910.o          \
	obj/charset.o         \
	obj/dis65c02.o        \
	obj/firmware.o        \
	obj/floppy.o          \
	obj/log.o             \
	obj/mmu.o             \
	obj/sdlemu_config.o   \
	obj/settings.o        \
	obj/sound.o           \
	obj/timing.o          \
	obj/v65c02.o          \
	obj/video.o           \
	obj/apple2.o          \
	$(ICON)

#foooked:
#	obj/button.o          \
	obj/diskwindow.o      \
	obj/draggablewindow.o \
	obj/draggablewindow2.o \
	obj/element.o         \
	obj/guimisc.o         \
	obj/menu.o            \
	obj/text.o            \
	obj/textedit.o        \
	obj/window.o          \

all: checkenv message obj $(TARGET)$(EXESUFFIX)
	@echo
	@echo -e "\033[01;33m***\033[00;32m Looks like it compiled OK... Give it a whirl!\033[00m"

# Check the compilation environment, barf if not appropriate

checkenv:
	@echo
	@echo -en "\033[01;33m***\033[00;32m Checking compilation environment... \033[00m"
ifeq "$(FINDSDL2)" ""
	@echo
	@echo
	@echo -e "\033[01;33mIt seems that you don't have the SDL 2 development libraries installed. If you"
	@echo -e "have installed them, make sure that the sdl2-config file is somewhere in your"
	@echo -e "path and is executable.\033[00m"
	@echo
#Is there a better way to break out of the makefile?
	@false;
#	@break
# YES! But ignores all the echo's above... :-/
#$(error SDL2 MISSING)

else
	@echo -e "\033[01;37mOK\033[00m"
endif

message:
	@echo
	@echo -e "\033[01;33m***\033[00;32m Building Apple2 SDL for $(MSG)...\033[00m"
	@echo

clean:
	@echo -en "\033[01;33m***\033[00;32m Cleaning out the garbage...\033[00m"
	@rm -rf obj
	@rm -f ./$(TARGET)$(EXESUFFIX)
	@echo -e "\033[01;37mdone!\033[00m"

obj:
	@mkdir obj

# This is only done for Win32 at the moment...

ifneq "" "$(ICON)"
$(ICON): res/$(TARGET).rc res/$(TARGET).ico
	@echo -e "\033[01;33m***\033[00;32m Processing icon...\033[00m"
	@$(CROSS)windres -i res/$(TARGET).rc -o $(ICON) --include-dir=./res
endif

obj/%.o: src/%.c
	@echo -e "\033[01;33m***\033[00;32m Compiling $<...\033[00m"
	@$(CC) $(CFLAGS) $(INCS) -c $< -o $@

obj/%.o: src/%.cpp
	@echo -e "\033[01;33m***\033[00;32m Compiling $<...\033[00m"
	@$(CC) $(CPPFLAGS) $(INCS) -c $< -o $@

#GUI compilation...
obj/%.o: src/gui/%.cpp
	@echo -e "\033[01;33m***\033[00;32m Compiling $<...\033[00m"
	@$(CC) $(CPPFLAGS) $(INCS) -c $< -o $@

$(TARGET)$(EXESUFFIX): $(OBJS)
	@echo -e "\033[01;33m***\033[00;32m Linking it all together...\033[00m"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
#	strip --strip-all vj$(EXESUFFIX)
#	upx -9 vj$(EXESUFFIX)

statistics:
	@echo -n "Lines in source files: "
	@-$(FIND) ./src -name "*.cpp" | xargs cat | wc -l
	@echo -n "Lines in header files: "
	@-$(FIND) ./src -name "*.h" | xargs cat | wc -l

# Pull in dependencies autogenerated by gcc's -MMD switch
# The "-" in front in there just in case they haven't been created yet

-include obj/*.d

