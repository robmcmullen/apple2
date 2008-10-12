#as;kjf;ass
# Unified Makefile for Apple 2 SDL
#
# by James L. Hammons
# (C) 2005 Underground Software
# This software is licensed under the GPL v2
#

# Figure out which system we're compiling for, and set the appropriate variables

ifeq "$(OSTYPE)" "msys"							# Win32

SYSTYPE    = __GCCWIN32__
EXESUFFIX  = .exe
GLLIB      = -lopengl32
ICON       = obj/icon.o
SDLLIBTYPE = --libs
MSG        = Win32 on MinGW

else
#ifeq "$(OSTYPE)" "darwin"
ifeq "darwin" "$(findstring darwin,$(OSTYPE))"	# Should catch both 'darwin' and 'darwin7.0'

SYSTYPE    = __GCCUNIX__ -D_OSX_
EXESUFFIX  =
GLLIB      =
ICON       =
SDLLIBTYPE = --static-libs
MSG        = Mac OS X

else											# *nix

SYSTYPE    = __GCCUNIX__
EXESUFFIX  =
GLLIB      = -lGL
ICON       =
SDLLIBTYPE = --libs
MSG        = generic Unix/Linux

endif
endif

CC         = gcc
LD         = gcc
TARGET     = apple2

# Note that we use optimization level 2 instead of 3--3 doesn't seem to gain much over 2
#CFLAGS   = -MMD -Wall -Wno-switch -O2 -D$(SYSTYPE) -ffast-math -fomit-frame-pointer `sdl-config --cflags`
#CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -O2 -D$(SYSTYPE) \
# No optimization and w/gcov flags, so that we get an accurate picture from gcov
#CFLAGS   = -MMD -Wall -Wno-switch -D$(SYSTYPE) \
#		-ffast-math -fomit-frame-pointer `sdl-config --cflags` -fprofile-arcs -ftest-coverage
#CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -D$(SYSTYPE) \
#		-ffast-math -fomit-frame-pointer `sdl-config --cflags` -fprofile-arcs -ftest-coverage
# No optimization for profiling with gprof...
CFLAGS   = -MMD -Wall -Wno-switch -D$(SYSTYPE) \
		-ffast-math `sdl-config --cflags` -pg -g
CPPFLAGS = -MMD -Wall -Wno-switch -Wno-non-virtual-dtor -D$(SYSTYPE) \
		-ffast-math `sdl-config --cflags` -pg -g
#		-fomit-frame-pointer `sdl-config --cflags` -g
#		-fomit-frame-pointer `sdl-config --cflags` -DLOG_UNMAPPED_MEMORY_ACCESSES

LDFLAGS =

#LIBS = -L/usr/local/lib -L/usr/lib `sdl-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB)
# Link in the gcov library (for profiling purposes)
#LIBS = -L/usr/local/lib -L/usr/lib `sdl-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB) -lgcov
# Link in the gprof lib
LIBS = -L/usr/local/lib -L/usr/lib `sdl-config $(SDLLIBTYPE)` -lstdc++ -lz $(GLLIB) -pg

INCS = -I. -I./src -I/usr/local/include -I/usr/include

OBJS = \
	obj/button.o          \
	obj/draggablewindow.o \
	obj/draggablewindow2.o \
	obj/element.o         \
	obj/gui.o             \
	obj/guimisc.o         \
	obj/menu.o            \
	obj/textedit.o        \
	obj/window.o          \
                          \
	obj/applevideo.o      \
	obj/ay8910.o          \
	obj/dis65c02.o        \
	obj/floppy.o          \
	obj/log.o             \
	obj/sdlemu_config.o   \
	obj/sdlemu_opengl.o   \
	obj/settings.o        \
	obj/sound.o           \
	obj/timing.o          \
	obj/v65c02.o          \
	obj/video.o           \
	obj/apple2.o          \
	$(ICON)

all: checkenv message obj $(TARGET)$(EXESUFFIX)
	@echo
	@echo "*** Looks like it compiled OK... Give it a whirl!"

# Check the compilation environment, barf if not appropriate

checkenv:
	@echo
	@echo -n "*** Checking compilation environment... "
ifeq "" "$(shell which sdl-config)"
	@echo
	@echo
	@echo "It seems that you don't have the SDL development libraries installed. If you"
	@echo "have installed them, make sure that the sdl-config file is somewhere in your"
	@echo "path and is executable."
	@echo
#Is there a better way to break out of the makefile?
	@break
else
	@echo "OK"
endif

message:
	@echo
	@echo "*** Building Apple 2 SDL for $(MSG)..."
	@echo

clean:
	@echo -n "*** Cleaning out the garbage..."
	@rm -rf obj
	@rm -f ./$(TARGET)$(EXESUFFIX)
	@echo "done!"

obj:
	@mkdir obj

# This is only done for Win32 at the moment...

ifneq "" "$(ICON)"
$(ICON): res/$(TARGET).rc res/$(TARGET).ico
	@echo "*** Processing icon..."
	@windres -i res/$(TARGET).rc -o $(ICON) --include-dir=./res
endif

obj/%.o: src/%.c
	@echo "*** Compiling $<..."
	@$(CC) $(CFLAGS) $(INCS) -c $< -o $@

obj/%.o: src/%.cpp
	@echo "*** Compiling $<..."
	@$(CC) $(CPPFLAGS) $(INCS) -c $< -o $@

#GUI compilation...
obj/%.o: src/gui/%.cpp
	@echo "*** Compiling $<..."
	@$(CC) $(CPPFLAGS) $(INCS) -c $< -o $@

$(TARGET)$(EXESUFFIX): $(OBJS)
	@echo "*** Linking it all together..."
	@$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
#	strip --strip-all vj$(EXESUFFIX)
#	upx -9 vj$(EXESUFFIX)

# Pull in dependencies autogenerated by gcc's -MMD switch
# The "-" in front in there just in case they haven't been created yet

-include obj/*.d
