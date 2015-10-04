
VPATH=../../../

CC=gcc
STRIP=strip
AR=ar

CFLAGS=-O2 -g -Wall -Werror-implicit-function-declaration -fno-strict-aliasing -DCLIENTONLY -DNETQW -I../thirdparty/include -L../thirdparty/lib $(OSCFLAGS) $(CPUCFLAGS) $(RENDERERCFLAGS)
STRIPFLAGS=--strip-unneeded --remove-section=.comment

TARGETSYSTEM:=$(shell $(CC) -dumpmachine | sed "s/^mingw32$$/i586-mingw32msvc/")

OS=$(shell echo $(TARGETSYSTEM) | sed "s/-linux-.*/-linux/" | sed "s/-gnu//" | sed "s/.*-//" | tr [A-Z] [a-z] | sed s/^mingw.*/win32/ | sed s/^openbsd.*/openbsd/ | sed s/^freebsd.*/freebsd/ | sed s/^darwin.*/macosx/)
CPU=$(shell echo $(TARGETSYSTEM) | cut -d '-' -f 1 | tr [A-Z] [a-z] | sed "s/powerpc/ppc/")

TARGETS=sw gl

# OS specific settings

ifeq ($(OS), morphos)
	OSCFLAGS=-noixemul -D__MORPHOS_SHAREDLIBS
	OSOBJS= \
		sys_morphos.o \
		net_amitcp.o \
		thread_morphos.o \
		cd_null.o \
		snd_morphos.o \
		in_morphos.o \
		sys_io_morphos.o

	OSSWOBJS=vid_mode_morphos.o vid_morphos.o

	OSGLOBJS=vid_mode_morphos.o vid_tinygl.o
endif

ifeq ($(OS), aros)
	OSCFLAGS=
	OSOBJS= \
		sys_morphos.o \
		net_amitcp.o \
		thread_aros.o \
		cd_null.o \
		in_morphos.o \
		sys_io_morphos.o \
		sys_lib_null.o

#		snd_morphos.o \

	OSSWOBJS=vid_mode_morphos.o vid_morphos.o

	OSGLOBJS=vid_mode_morphos.o vid_arosmesa.o
	OSGLLDFLAGS=-lGL

	THIRDPARTYLIBS=libz libpng libjpeg
endif

ifeq ($(OS), linux)
	OSOBJS= \
		sys_linux.o \
		sys_error_gtk.o \
		net_posix.o \
		thread_posix.o \
		cd_linux.o \
		snd_oss.o \
		snd_alsa2.o \
		snd_pulseaudio.o \
		sys_io_posix.o \
		sys_lib_posix.o

	OSCFLAGS=-DBUILD_STRL
	OSLDFLAGS=-lpthread -lrt -ldl

	OSSWOBJS=vid_x11.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSSWLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga

	OSGLOBJS=vid_glx.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSGLLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lGL
endif

ifeq ($(OS), freebsd)
	OSOBJS= \
		sys_linux.o \
		sys_error_gtk.o \
		net_posix.o \
		thread_posix.o \
		cd_null.o \
		snd_oss.o \
		sys_io_posix.o \
		sys_lib_posix.o

	OSCFLAGS=-I/usr/local/include
	OSLDFLAGS=-lpthread

	OSSWOBJS=vid_x11.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSSWLDFLAGS=-L/usr/local/lib -lX11 -lXext -lXxf86dga

	OSGLOBJS=vid_glx.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSGLLDFLAGS=-L/usr/local/lib -lX11 -lXext -lXxf86dga -lGL
endif

ifeq ($(OS), netbsd)
	OSOBJS= \
		sys_linux.o \
		cd_null.o \
		snd_oss.o

	OSCFLAGS=-I/usr/X11R6/include
	OSLDFLAGS=-lossaudio

	OSSWOBJS=vid_x11.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSSWLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga

	OSGLOBJS=vid_glx.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSGLLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lGL
endif

ifeq ($(OS), openbsd)
	OSOBJS= \
		sys_linux.o \
		sys_error_gtk.o \
		net_posix.o \
		thread_posix.o \
		cd_null.o \
		sys_io_posix.o \
		sys_lib_posix.o \
		snd_sndio.o

	OSCFLAGS=-I/usr/X11R6/include -I/usr/local/include -I/usr/local/include/libpng -I/usr/local/include/gtk-2.0
	OSLDFLAGS=-lpthread

	OSSWOBJS=vid_x11.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSSWLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga

	OSGLOBJS=vid_glx.o vid_mode_x11.o vid_mode_xf86vm.o vid_mode_xrandr.o in_x11.o
	OSGLLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lGL
endif

ifeq ($(OS), cygwin)
	OSOBJS= \
		sys_linux.o \
		cd_null.o \
		snd_null.o

	OSSWOBJS=vid_x11.o in_x11.o
	OSSWLDFLAGS=-L/usr/X11R6/lib -lX11 -lXext
endif

ifeq ($(OS), win32)
	OSOBJS= \
		sys_win.o \
		thread_win32.o \
		snd_dx7.o \
		net_win32.o \
		cd_null.o \
		in_dinput8.o \
		sys_io_win32.o \
		sys_lib_null.o \
		clipboard_win32.o

	OSSWOBJS=vid_win.o vid_mode_win32.o fodquake-sw.windowsicon
	OSSWLDFLAGS=-lmgllt -lwsock32 -lgdi32 -ldxguid -lwinmm

	OSGLOBJS=vid_wgl.o vid_mode_win32.o fodquake-gl.windowsicon
	OSGLLDFLAGS=-lopengl32 -lwinmm -lwsock32 -lgdi32 -ldxguid -ldinput8

	OSCFLAGS = -I`cd ~/directx && pwd` -DBUILD_STRL
	ifneq ($(shell $(CC) -dumpmachine | grep cygwin),)
		OSCFLAGS+= -mno-cygwin
	endif
	OSLDFLAGS = -mwindows -lpng -ljpeg -lz

	THIRDPARTYLIBS=libz libpng libjpeg

%.ico: icons/%-16x16.png icons/%-32x32.png icons/%-48x48.png icons/%-64x64.png
	for i in $^; \
	do \
		pngtopnm $$i >tmpimg && ppmquant 256 tmpimg >`echo $$i | sed "s,.*/,,"`; rm tmpimg; \
		pngtopnm -alpha $$i >`echo $$i | sed "s,.*/,,"`_alpha; \
	done

	ppmtowinicon -andpgms -output $@ `echo $^ | sed "s,[^ ]*/\([^ ]*\),\1 \1_alpha ,g"`

%.windowsicon: %.rc %.ico
	i586-mingw32msvc-windres -O coff $< $@
endif

ifeq ($(OS), gekko)
# Hey, we're actually libogc on the Wii.

	OSOBJS = \
		sys_wii.o \
		net_null.o \
		cd_null.o \
		fatfs/libfat.a
	
	OSSWOBJS = vid_wii.o in_wii.o

	# -mrvl
	# -I/usr/local/devkitPro/devkitPPC/powerpc-gekko/include/
	OSCFLAGS = -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -I/usr/local/devkitPro/devkitPPC/include
	OSLDFLAGS = -L/usr/local/devkitPro/devkitPPC/lib/wii/ -logc
endif

ifeq ($(OS), macosx)
	STRIPFLAGS=

	OSOBJS = \
		sys_darwin.o \
		sys_io_posix.o \
		sys_lib_null.o \
		thread_posix.o \
		net_posix.o \
		snd_coreaudio.o \
		vid_mode_macosx.o \
		clipboard_macosx.o \
		cd_null.o \
		vid_cocoa.o \
		in_macosx.o

	OSGLLDFLAGS = -framework OpenGL

	OSCFLAGS = -D__MACOSX__

	OSLDFLAGS = -framework AppKit -framework ApplicationServices -framework AudioUnit -framework CoreServices -framework IOKit -lpng -ljpeg -lz

	THIRDPARTYLIBS=libpng libjpeg

%.o: %.m
	$(CC) $(CFLAGS) -c $< -o $@
endif

# CPU specific settings

ifeq ($(CPU), ppc)
   CPUCFLAGS=-DFOD_BIGENDIAN -DFOD_PPC -maltivec
endif

SVOBJS= \
	pr_edict.o \
	pr_exec.o \
	pr_cmds.o \
	sv_ccmds.o \
	sv_ents.o \
	sv_init.o \
	sv_main.o \
	sv_move.o \
	sv_nchan.o \
	sv_phys.o \
	sv_save.o \
	sv_send.o \
	sv_user.o \
	sv_world.o

OBJS= \
	cl_sbar.o \
	cl_screen.o \
	cl_cam.o \
	cl_capture.o \
	cl_cmd.o \
	cl_demo.o \
	cl_ents.o \
	cl_fchecks.o \
	cl_fragmsgs.o \
	cl_ignore.o \
	cl_input.o \
	cl_logging.o \
	cl_main.o \
	cl_parse.o \
	cl_pred.o \
	cl_tent.o \
	cl_view.o \
	cmd.o \
	com_msg.o \
	common.o \
	config_manager.o \
	console.o \
	context_sensitive_tab.o \
	crc.o \
	cvar.o \
	filesystem.o \
	fmod.o \
	host.o \
	huffman.o \
	image.o \
	keys.o \
	linked_list.o \
	lua.o \
	match_tools.o \
	mathlib.o \
	md5.o \
	mdfour.o \
	menu.o \
	modules.o \
	mouse.o \
	mp3_player.o \
	net_chan.o \
	net.o \
	netqw.o \
	pmove.o \
	pmovetst.o \
	qstring.o \
	r_draw.o \
	r_part.o \
	readablechars.o \
	ruleset.o \
	server_browser.o \
	server_browser_qtv.o \
	serverscanner.o \
	skin.o \
	sleep.o \
	snd_main.o \
	snd_mem.o \
	snd_mix.o \
	strlcat.o \
	strlcpy.o \
	tableprint.o \
	teamplay.o \
	text_input.o \
	tokenize_string.o \
	utils.o \
	version.o \
	vid.o \
	wad.o \
	zone.o \
	$(OSOBJS)

SWOBJS= \
	d_draw.o \
	d_edge.o \
	d_init.o \
	d_modech.o \
	d_part.o \
	d_polyse.o \
	d_scan.o \
	d_skinimp.o \
	d_sky.o \
	d_sprite.o \
	d_surf.o \
	d_vars.o \
	d_zpoint.o \
	r_aclip.o \
	r_alias.o \
	r_bsp.o \
	r_edge.o \
	r_efrag.o \
	r_light.o \
	r_main.o \
	r_model.o \
	r_misc.o \
	r_sky.o \
	r_sprite.o \
	r_surf.o \
	r_rast.o \
	r_vars.o \
	$(OSSWOBJS)

GLOBJS= \
	gl_draw.o \
	gl_mesh.o \
	gl_model.o \
	gl_ngraph.o \
	gl_part.o \
	gl_refrag.o \
	gl_rlight.o \
	gl_rmain.o \
	gl_rmisc.o \
	gl_rpart.o \
	gl_rsurf.o \
	gl_shader.o \
	gl_skinimp.o \
	gl_state.o \
	gl_texture.o \
	gl_warp.o \
	vid_common_gl.o \
	$(OSGLOBJS)

all: $(TARGETS) compilercheck

thirdparty:
	mkdir -p objects/$(TARGETSYSTEM)/thirdparty
	if [ ! -z "$(THIRDPARTYLIBS)" ]; then (cd objects/$(TARGETSYSTEM)/thirdparty; $(MAKE) -f $(VPATH)Makefile $(THIRDPARTYLIBS)); fi

gl: thirdparty
	mkdir -p objects/$(TARGETSYSTEM)/gl
	(cd objects/$(TARGETSYSTEM)/gl; $(MAKE) -f $(VPATH)Makefile fodquake-gl RENDERERCFLAGS=-DGLQUAKE)

sw: thirdparty
	mkdir -p objects/$(TARGETSYSTEM)/sw
	(cd objects/$(TARGETSYSTEM)/sw; $(MAKE) -f $(VPATH)Makefile fodquake-sw)


clean:
	rm -rf objects

libz: libz/zlib-1.2.5/.buildstamp

libz/zlib-1.2.5/.buildstamp:
	rm -rf libz
	mkdir libz
	(cd libz && tar -xf ../$(VPATH)/thirdparty/zlib-1.2.5.tar.gz)
	cp $(VPATH)/thirdparty/zlib-Makefile libz/zlib-1.2.5/
	(cd libz/zlib-1.2.5 && $(MAKE) -f zlib-Makefile libz.a CC="$(CC)")
	mkdir -p include lib
	cp libz/zlib-1.2.5/zconf.h libz/zlib-1.2.5/zlib.h include
	cp libz/zlib-1.2.5/libz.a lib
	touch $@

libpng: libpng/libpng-1.2.50/.buildstamp

libpng/libpng-1.2.50/.buildstamp:
	rm -rf libpng
	mkdir libpng
	(cd libpng && tar -xf ../$(VPATH)/thirdparty/libpng-1.2.50-no-config.tar.gz)
	(cd libpng/libpng-1.2.50 && cp scripts/makefile.gcc Makefile)
	(cd libpng/libpng-1.2.50 && $(MAKE) CC="$(CC)" AR_RC="$(AR) rcs" CFLAGS="-W -Wall -I../../include $(CRELEASE)" RANLIB=touch libpng.a)
	mkdir -p include lib
	cp libpng/libpng-1.2.50/*.h include
	cp libpng/libpng-1.2.50/libpng.a lib
	touch $@

libjpeg: libjpeg/jpeg-8c/.buildstamp

libjpeg/jpeg-8c/.buildstamp:
	rm -rf libjpeg
	mkdir libjpeg
	(cd libjpeg && tar -xf ../$(VPATH)/thirdparty/jpegsrc.v8c.tar.gz)
	cp libjpeg/jpeg-8c/jconfig.txt libjpeg/jpeg-8c/jconfig.h
	(cd libjpeg/jpeg-8c && $(MAKE) -f makefile.ansi CC="$(CC)" CFLAGS="-O2" AR="$(AR) rcs" AR2="touch" libjpeg.a)
	mkdir -p include
	cp libjpeg/jpeg-8c/jpeglib.h libjpeg/jpeg-8c/jconfig.h libjpeg/jpeg-8c/jmorecfg.h include
	cp libjpeg/jpeg-8c/libjpeg.a lib
	touch $@

fodquake-sw: $(OBJS) $(SWOBJS)
	$(CC) $(CFLAGS) $^ -lm $(OSLDFLAGS) $(OSSWLDFLAGS) -o $@.db
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

fodquake-gl: $(OBJS) $(GLOBJS)
	$(CC) $(CFLAGS) $^ -lm $(OSLDFLAGS) $(OSGLLDFLAGS) -o $@.db
	$(STRIP) $(STRIPFLAGS) $@.db -o $@

sys_error_gtk.o: CFLAGS+=`pkg-config --cflags gtk+-2.0`

compilercheck:
# Check for GCC 4

	@if [ ! -z "`$(CC) -v 2>&1 | grep \"gcc version 4\"`" ]; \
	then \
		echo ""; \
		echo ""; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		echo "GCC version 4 was detected."; \
		echo ""; \
		echo "Please note that builds done with GCC 4 or newer are not supported. If you"; \
		echo "experience any problems with Fodquake built with this compiler, please either"; \
		echo "disable optimisations or build Fodquake with GCC 2.95.3, GCC 3.3.6 or GCC"; \
		echo "3.4.6."; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		echo ""; \
		echo ""; \
	fi

.PHONY: compilercheck thirdparty

