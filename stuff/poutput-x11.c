/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * X11 graphic driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -ss050424   Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#define _CONSOLE_DRIVER
#include "config.h"
#include <curses.h> /* for key-codes */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "boot/psetting.h"
#include "types.h"
#include "cpiface/cpiface.h"
#include "poutput-x11.h"
#include "boot/console.h"
#include "poutput.h"
#include "pfonts.h"
#include "x11-common.h"
#include "stuff/framelock.h"
#include "desktop/opencubicplayer.xpm"

/* TEXT-MODE DRIVER */
static uint8_t *vgatextram = 0;
typedef enum {
	_4x4 = 0,
	_8x8 = 1,
	_8x16 = 2,
	_FONT_MAX = 2
} FontSizeEnum;

static FontSizeEnum plUseFont = _8x8;
static FontSizeEnum plCurrentFont = _8x8;

static unsigned short plScrRowBytes;

static int ___valid_key(uint16_t key);
static void create_image(void);
static void destroy_image(void);

static XSizeHints Textmode_SizeHints;
static unsigned long Textmode_Window_Width = 0;
static unsigned long Textmode_Window_Height = 0;


	

/* "stolen" from Mplayer START */
static void vo_hidecursor(Display * disp, Window win)
{
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black, dummy;
	Colormap colormap;
	static char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	colormap = DefaultColormap(disp, DefaultScreen(disp));
	XAllocNamedColor(disp, colormap, "black", &black, &dummy);
	bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8, 8);
	no_ptr = XCreatePixmapCursor(disp, bm_no, bm_no, &black, &black, 0, 0);
	XDefineCursor(disp, win, no_ptr);
	XFreeCursor(disp, no_ptr);
	if (bm_no != None)
		XFreePixmap(disp, bm_no);
	XFreeColors(disp,colormap,&black.pixel,1,0);
}
static void vo_showcursor(Display * disp, Window win)
{
	XDefineCursor(disp, win, 0);
}
/* "stolen" from Mplayer STOP */

static int xvidmode_event_base=-1, xvidmode_error_base;
static XF86VidModeModeInfo **xvidmode_modes=NULL;

static XF86VidModeModeInfo *modelines[6], *Graphmode_modeline, default_modeline;
enum MODE_LINES 
{
	MODE_320x200,
/*	MODE_320x240,*/
/*	MODE_640x400,*/
	MODE_640x480,
	MODE_1024x768,
/*	MODE_1280x1024*/
/*	MODE_1600x1200*/
};

static Atom XA_NET_SUPPORTED;
static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_WM_NAME;
static Atom XA_STRING;
static Atom XA_UTF8_STRING;
static Atom XA_WM_NAME;

static int we_have_fullscreen;
static int do_fullscreen=0;

static XShmSegmentInfo shminfo[1];
static int shm_completiontype = -1;

static Window window=0;
static XImage *image=0;
static Pixmap icon=0;
static Pixmap icon_mask=0;

static GC copyGC=0;
static char *virtual_framebuffer;

static int cachemode=-1;

static void xvidmode_init(void)
{
	int modes_n=1024;
	int i;
	XF86VidModeModeLine tmp;
	XWindowAttributes xwa;

	memset(modelines, 0, sizeof(modelines));
	memset(&default_modeline, 0, sizeof(default_modeline));

	XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &xwa);
	fprintf(stderr, "[x11] rootwindow: width:%d height:%d\n", xwa.width, xwa.height); 
	default_modeline.hdisplay = xwa.width;
	default_modeline.vdisplay = xwa.height;
	/* TODO */

	if ((!cfGetProfileBool("x11", "xvidmode", 1, 0)) )
	{
        	if (!XF86VidModeQueryExtension(mDisplay, &xvidmode_event_base, &xvidmode_error_base))
		{
			fprintf(stderr, "[x11] XF86VidModeQueryExtension() failed\n");
			xvidmode_event_base=-1;
			return;
		} else {
			fprintf(stderr, "[x11] xvidmode enabled\n");
		}
	} else {
		fprintf(stderr, "[x11] xvidmode disabled in ocp.ini\n");
		return;
	}
	if (!(XF86VidModeGetModeLine(mDisplay, mScreen, (int *)&default_modeline.dotclock, &tmp)))
	{
		fprintf(stderr, "[x11] XF86VidModeGetModeLine() failed\n");
		xvidmode_event_base=-1;
		return;
	}
	/* X is somewhat braindead sometimes */
	default_modeline.hdisplay=tmp.hdisplay;
	default_modeline.hsyncstart=tmp.hsyncstart;
	default_modeline.hsyncend=tmp.hsyncend;
	default_modeline.htotal=tmp.htotal;
	default_modeline.hskew=tmp.hskew;
	default_modeline.vdisplay=tmp.vdisplay;
	default_modeline.vsyncstart=tmp.vsyncstart;
	default_modeline.vsyncend=tmp.vsyncend;
	default_modeline.vtotal=tmp.vtotal;
	default_modeline.flags=tmp.flags;
	default_modeline.privsize=tmp.privsize;
	default_modeline.private=tmp.private;

	if (!XF86VidModeGetAllModeLines(mDisplay, mScreen, &modes_n, &xvidmode_modes))
	{
		fprintf(stderr, "[x11] XF86VidModeGetAllModeLines() failed\n");
		xvidmode_event_base=-1;
		return;
	}

	for (i=modes_n-1;i>=0;i--)
	{
		if ((xvidmode_modes[i]->hdisplay>=320) && (xvidmode_modes[i]->vdisplay>=200))
		{
			if (!modelines[MODE_320x200])
				modelines[MODE_320x200] = xvidmode_modes[i];
			if ((modelines[MODE_320x200]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_320x200]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_320x200] = xvidmode_modes[i];
		}
		/*if ((xvidmode_modes[i]->hdisplay>=320) && (xvidmode_modes[i]->vdisplay>=240))
		{
			if (!modelines[MODE_320x240])
				modelines[MODE_320x240] = xvidmode_modes[i];
			if ((modelines[MODE_320x240]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_320x240]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_320x240] = xvidmode_modes[i];
		}
		if ((xvidmode_modes[i]->hdisplay>=640) && (xvidmode_modes[i]->vdisplay>=400))
		{
			if (!modelines[MODE_640x400])
				modelines[MODE_640x400] = xvidmode_modes[i];
			if ((modelines[MODE_640x400]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_640x400]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_640x400] = xvidmode_modes[i];
		}*/
		if ((xvidmode_modes[i]->hdisplay>=640) && (xvidmode_modes[i]->vdisplay>=480))
		{
			if (!modelines[MODE_640x480])
				modelines[MODE_640x480] = xvidmode_modes[i];
			if ((modelines[MODE_640x480]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_640x480]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_640x480] = xvidmode_modes[i];
		}
		if ((xvidmode_modes[i]->hdisplay>=1024) && (xvidmode_modes[i]->vdisplay>=768))
		{
			if (!modelines[MODE_1024x768])
				modelines[MODE_1024x768] = xvidmode_modes[i];
			if ((modelines[MODE_1024x768]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_1024x768]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_1024x768] = xvidmode_modes[i];
		}
		/*if ((xvidmode_modes[i]->hdisplay>=1280) && (xvidmode_modes[i]->vdisplay>=1024))
		{
			if (!modelines[MODE_1280x1024])
				modelines[MODE_1280x1024] = xvidmode_modes[i];
			if ((modelines[MODE_1280x1024]->hdisplay > xvidmode_modes[i]->hdisplay) || (modelines[MODE_1280x1024]->vdisplay > xvidmode_modes[i]->vdisplay))
				modelines[MODE_1280x1024] = xvidmode_modes[i];
		}*/
	}
}
static void xvidmode_done(void)
{
	if (xvidmode_event_base>=0)
	{
		XF86VidModeSwitchToMode(mDisplay, mScreen, &default_modeline);
		xvidmode_event_base=-1;
	}
	if (default_modeline.privsize)
	{
		XFree(default_modeline.private);
		default_modeline.privsize=0;
	}
	if (xvidmode_modes)
	{
		XFree(xvidmode_modes);
		xvidmode_modes=NULL;
	}
}

static void ewmh_init(void)
{
	int format;
	unsigned int i;
	unsigned long nitems;
	unsigned long bytes_after_return;
	Atom *args;
	unsigned char *tmp;
	
	XA_NET_SUPPORTED = XInternAtom(mDisplay, "_NET_SUPPORTED", False);
	XA_NET_WM_STATE = XInternAtom(mDisplay, "_NET_WM_STATE", False);
	XA_NET_WM_STATE_FULLSCREEN = XInternAtom(mDisplay, "_NET_WM_STATE_FULLSCREEN", False);
	XA_NET_WM_NAME = XInternAtom(mDisplay, "_NET_WM_NAME", False);
	XA_STRING = XInternAtom(mDisplay, "STRING", False);
	XA_UTF8_STRING = XInternAtom(mDisplay, "UTF8_STRING", False);
	XA_WM_NAME = XInternAtom(mDisplay, "WM_NAME", False);

	we_have_fullscreen=0;

	if ((XGetWindowProperty(mDisplay, DefaultRootWindow(mDisplay), XA_NET_SUPPORTED, 0, 16384, False, AnyPropertyType, &XA_NET_SUPPORTED, &format, &nitems, &bytes_after_return, (unsigned char **)&tmp) == Success ))
		if (tmp)
		{
			args=(Atom *)tmp; /* removes compiler warning above */
			for (i = 0; i < nitems; i++)
			{
				if ((args[i]==XA_NET_WM_STATE_FULLSCREEN))
					we_have_fullscreen=1;
			}
			XFree(tmp);
		}
}

static int ewmh_fullscreen(Window window, int action)
{
	XEvent xev;

	xev.xclient.type = ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = True;
	xev.xclient.message_type = XInternAtom(mDisplay, "_NET_WM_STATE", False);
	xev.xclient.window = window;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = action;
	xev.xclient.data.l[1] = XInternAtom(mDisplay, "_NET_WM_STATE_FULLSCREEN", False);
	xev.xclient.data.l[2] = 0;
	xev.xclient.data.l[3] = 0;
	xev.xclient.data.l[4] = 0;
	
	if (!XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev))
	{
		fprintf(stderr, "[x11] (EWMH) Failed to set NET_WM_STATE_FULLSCREEN\n");
		return -1;
	}
	return 0;
}

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW      (1L<<0)
static void motif_decoration(Window window, int action)
{

	typedef struct
	{
    		long flags;
		long functions;
		long decorations;
		long input_mode;
		long state;
	} MotifWmHints;
	
	static Atom vo_MotifHints = None;
	
     	vo_MotifHints = XInternAtom(mDisplay, "_MOTIF_WM_HINTS", 0);
	
	if (vo_MotifHints != None)
	{
		MotifWmHints vo_MotifWmHints;

		memset(&vo_MotifWmHints, 0, sizeof(MotifWmHints));
		vo_MotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
		if (action)
		{
			vo_MotifWmHints.functions=MWM_FUNC_ALL; //MWM_FUNC_MOVE;
			vo_MotifWmHints.decorations=MWM_DECOR_ALL; //MWM_DECOR_BORDER|MWM_DECOR_TITLE;
		} else {
			vo_MotifWmHints.functions=0;
			vo_MotifWmHints.decorations=0;
		}
		XChangeProperty(mDisplay, window, vo_MotifHints, vo_MotifHints, 32,
				PropModeReplace,
				(unsigned char *) &vo_MotifWmHints,
				5);
	}
}

static void (*WindowResized)(unsigned int width, unsigned int height);

static void WindowResized_Graphmode(unsigned int width, unsigned int height)
{
}

static void WindowResized_Textmode(unsigned int width, unsigned int height)
{
	plScrLineBytes=width;
	plScrLines=height;

	if (plCurrentFont >= _8x16)
	{
		if ( ( plScrLineBytes < ( 8 * 80 ) ) || ( plScrLines < ( 16 * 25 ) ) )
			plCurrentFont = _8x8;
	}
	if (plCurrentFont >= _8x8)
	{
		if ( ( plScrLineBytes < ( 8 * 80 ) ) || ( plScrLines < ( 8 * 25 ) ) )
			plCurrentFont = _4x4;
	}

	switch (plCurrentFont)
	{
		case _4x4:
			plScrWidth = plScrLineBytes / 4;
			plScrHeight = plScrLines / 4;
			break;
		case _8x8:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 8;
			break;
		case _8x16:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 16;
			break;
	}
	plScrRowBytes=plScrWidth*2;
	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}

	destroy_image();
	create_image();

	if (!do_fullscreen)
	{
		Textmode_Window_Height = height;
		Textmode_Window_Width = width;
	}

	___push_key(VIRT_KEY_RESIZE);
	/*RefreshScreenText();*/
}

static void (*set_state)(int target) = 0;

static void TextModeSetState(FontSizeEnum FontSize, int FullScreen);

static void set_state_textmode(int target)
{
	TextModeSetState (plCurrentFont, target);
	plUseFont = plCurrentFont;
}

static void set_state_graphmode(int target)
{
	XSizeHints SizeHints;

	if ( !we_have_fullscreen )
		target=0;

	do_fullscreen = target;

	if (!window)
		return;

	SizeHints.flags=PMinSize|PMaxSize|USSize|PSize;

	if (do_fullscreen)
	{
		SizeHints.min_width=SizeHints.max_width=Graphmode_modeline->hdisplay;
		SizeHints.min_height=SizeHints.max_height=Graphmode_modeline->vdisplay;
		XSetWMNormalHints(mDisplay, window, &SizeHints);
		XResizeWindow(mDisplay, window, Graphmode_modeline->hdisplay, Graphmode_modeline->vdisplay);
		XSync(mDisplay, FALSE);
		XClearWindow(mDisplay, window);		
	} else {
		SizeHints.min_width=SizeHints.max_width=plScrLineBytes;
		SizeHints.min_height=SizeHints.max_height=plScrLines;
		XSetWMNormalHints(mDisplay, window, &SizeHints);
		XResizeWindow(mDisplay, window, plScrLineBytes, plScrLines);
		XSync(mDisplay, FALSE);
	}
	___push_key(VIRT_KEY_RESIZE);

	motif_decoration(window, !do_fullscreen);
	ewmh_fullscreen(window, do_fullscreen);


	if (xvidmode_event_base>=0)
	{
		if (do_fullscreen)
		{
			XF86VidModeSwitchToMode(mDisplay, mScreen, Graphmode_modeline);
			XF86VidModeSetViewPort(mDisplay, mScreen, 0, 0);
		} else {
			XF86VidModeSwitchToMode(mDisplay, mScreen, &default_modeline);
		}
	}

	if (do_fullscreen)
	{
		XGrabKeyboard(mDisplay, DefaultRootWindow(mDisplay), True, GrabModeAsync, GrabModeAsync, CurrentTime);
		XGrabPointer(mDisplay, DefaultRootWindow(mDisplay), True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
		vo_hidecursor(mDisplay, window);
	} else {
		vo_showcursor(mDisplay, window);
		XUngrabKeyboard(mDisplay, CurrentTime);
		XUngrabPointer(mDisplay, CurrentTime);
	}

	destroy_image();
	create_image();
}

static void TextModeSetState(FontSizeEnum FontSize, int FullScreen)
{
	/* We recalc Textmode_SizeHints and friends here, as a step for future support of RootWindow resizing */
	if (!window)
		return;

	if ( (!we_have_fullscreen) )
		FullScreen=0;

	do_fullscreen = FullScreen;

	Textmode_SizeHints.flags=PMinSize|PMaxSize|USSize|PSize;
	Textmode_SizeHints.max_width=default_modeline.hdisplay;
	Textmode_SizeHints.max_height=default_modeline.vdisplay;
	
	switch (FontSize)
	{
		/* find the min value, if screen is too small, force smaller font */
		case _8x16:
			Textmode_SizeHints.min_width = 8 * 80;
			Textmode_SizeHints.min_height = 16 * 25;
			if ( (Textmode_SizeHints.min_width <= Textmode_SizeHints.max_width) && (Textmode_SizeHints.min_height <= Textmode_SizeHints.max_height) )
				break;
			FontSize = _8x8; /* drop through */
		case _8x8:
			Textmode_SizeHints.min_width = 8 * 80;
			Textmode_SizeHints.min_height = 8 * 25;
			if ( (Textmode_SizeHints.min_width <= Textmode_SizeHints.max_width) && (Textmode_SizeHints.min_height <= Textmode_SizeHints.max_height) )
				break;
			FontSize = _4x4; /* drop through */
		case _4x4:
			Textmode_SizeHints.min_width = 4 * 80;
			Textmode_SizeHints.min_height = 4 * 25;
			if ( (Textmode_SizeHints.min_width <= Textmode_SizeHints.max_width) && (Textmode_SizeHints.min_height <= Textmode_SizeHints.max_height) )
				break;
			/* unless font has became this small, force bigger window */
			Textmode_SizeHints.max_width = Textmode_SizeHints.min_width;
			Textmode_SizeHints.max_height = Textmode_SizeHints.min_height;
	}

	XSetWMNormalHints(mDisplay, window, &Textmode_SizeHints);

	if (Textmode_Window_Width < (unsigned)Textmode_SizeHints.min_width) /* the need for (unsigned here is just plain stupid) */
		Textmode_Window_Width = Textmode_SizeHints.min_width;
	if (Textmode_Window_Height < (unsigned)Textmode_SizeHints.min_height) /* the need for (unsigned here is just plain stupid) */
		Textmode_Window_Height = Textmode_SizeHints.min_height;
	if (Textmode_Window_Width > (unsigned)Textmode_SizeHints.max_width) /* the need for (unsigned here is just plain stupid) */
		Textmode_Window_Width = Textmode_SizeHints.max_width;
	if (Textmode_Window_Height > (unsigned)Textmode_SizeHints.max_height) /* the need for (unsigned here is just plain stupid) */
		Textmode_Window_Height = Textmode_SizeHints.max_height;

	motif_decoration(window, !do_fullscreen);
	ewmh_fullscreen(window, do_fullscreen);

	if (do_fullscreen)
	{
		XResizeWindow(mDisplay, window, default_modeline.hdisplay, default_modeline.vdisplay);
		XSync(mDisplay, FALSE);
		plScrLines=default_modeline.vdisplay;
		plScrLineBytes=default_modeline.hdisplay;
	} else {
		XResizeWindow(mDisplay, window, Textmode_Window_Width, Textmode_Window_Height);
		XSync(mDisplay, FALSE);
		plScrLines=Textmode_Window_Height;
		plScrLineBytes=Textmode_Window_Width;
	}
	___push_key(VIRT_KEY_RESIZE);

	if (xvidmode_event_base>=0)
	{
		XF86VidModeSwitchToMode(mDisplay, mScreen, &default_modeline);
	}
	if (do_fullscreen)
	{
		XGrabKeyboard(mDisplay, DefaultRootWindow(mDisplay), True, GrabModeAsync, GrabModeAsync, CurrentTime);
		XGrabPointer(mDisplay, DefaultRootWindow(mDisplay), True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
		vo_hidecursor(mDisplay, window);
	} else {
		vo_showcursor(mDisplay, window);
		XUngrabKeyboard(mDisplay, CurrentTime);
		XUngrabPointer(mDisplay, CurrentTime);
	}

	plCurrentFont = FontSize;
	switch (plCurrentFont)
	{
		case _4x4:
			plScrWidth = plScrLineBytes / 4;
			plScrHeight = plScrLines / 4;
			break;
		case _8x8:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 8;
			break;
		case _8x16:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 16;
			break;
	}
	plScrRowBytes=plScrWidth*2;
	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}
	destroy_image();
	create_image();
}

static void x11_common_event_loop(void)
{
	XEvent event;

	if (xvidmode_event_base>=0)
	{
		if (do_fullscreen)
		{
			int x, y;
			XF86VidModeGetViewPort(mDisplay, mScreen, &x, &y);
			if (x||y)
			{
				XWarpPointer(mDisplay, None, window, 0, 0, 0, 0, 10, 10);
				XF86VidModeSetViewPort(mDisplay, mScreen, 0, 0);
			}
		}
	}
	while (XPending(mDisplay))
	{
		int n_chars = 0;
		char buf[21];
		KeySym ks;

		uint16_t key;

		XNextEvent(mDisplay, &event);

		if (event.type==shm_completiontype)
			continue;

		switch (event.type)
		{
			default:
				fprintf(stderr, "Unknown event.type=%d\n", event.type);
				break;
			case Expose:
				/* silently ignore these */
				break;
			case ConfigureNotify:
				//fprintf(stderr, "configure notify\n");
				WindowResized(event.xconfigure.width, event.xconfigure.height);
				break;
			#if 0
			case ResizeRequest:
				#warning TODO
				if (plScrMode<8)
				{
					int x, y;
					fprintf(stderr, "Resize income pixels: %d %d\n", event.xresizerequest.width, event.xresizerequest.height);
					x=event.xresizerequest.width;
					y=event.xresizerequest.height;
	
					switch (plUseFont)
					{
						case _4x4:
							x &= ~3;
							y &= ~3;
							break;
						case _8x8:
							x &= ~7;
							y &= ~7;
							break;
						case _8x16:
							x &= ~7;
							y &= ~15;
							break;
					}
					fprintf(stderr, "TEREWRWR\n");
					//XResizeWindow(mDisplay, window, x, y);
					/*RefreshScreenText();*/
				} else {
					/*RefreshScreenGraph();*/
				}
				break;
			#endif
			case MappingNotify:
				XRefreshKeyboardMapping( &event.xmapping );
				break;
			case ButtonRelease:
				break; /* ignore for now */
			case ButtonPress:
				switch (event.xbutton.button)
				{
					case 4:
						___push_key(KEY_UP);
						break;
					case 5:
						___push_key(KEY_DOWN);
						break;
					case 1:
						___push_key(_KEY_ENTER);
						break;
					case 3:
						set_state(!do_fullscreen);
						break;
				}
				break;
			case KeyPress:
				n_chars = XLookupString(&event.xkey, buf, 20, &ks, NULL);
				key=0;
				if ((n_chars==1)/*&&(buf[0]>=32*/&&ks!=XK_Delete)
				{
					if ((event.xkey.state&(ShiftMask|ControlMask|Mod1Mask))==ControlMask)
					{
						switch (toupper(buf[0]))
						{
							case '\r':
							/*case '\n':*/ key=KEY_CTRL_ENTER; break;
							case 4:  key=KEY_CTRL_D; break;
							case 6: set_state(!do_fullscreen); break;
							case 8: key=KEY_CTRL_H; break;
							case 10: key=KEY_CTRL_J; break;
							case 11: key=KEY_CTRL_K; break;
							case 12: key=KEY_CTRL_L; break;
							case 16: key=KEY_CTRL_P; break;
							case 17: key=KEY_CTRL_Q; break;
							case 19: key=KEY_CTRL_S; break;
							case 26: key=KEY_CTRL_Z; break;
						}
					} else if ((event.xkey.state&(ShiftMask|ControlMask|Mod1Mask))==Mod1Mask)
					{
						switch (toupper(buf[0]))
						{
							case '\r':
							case '\n': set_state(!do_fullscreen); break;
							case 'A': key=KEY_ALT_A; break;
							case 'C': key=KEY_ALT_C; break;
							case 'E': key=KEY_ALT_E; break;
							case 'G': key=KEY_ALT_G; break;
							case 'I': key=KEY_ALT_I; break;
							case 'K': key=KEY_ALT_K; break;
							case 'L': key=KEY_ALT_L; break;
							case 'O': key=KEY_ALT_O; break;
							case 'P': key=KEY_ALT_P; break;
							case 'R': key=KEY_ALT_R; break;
							case 'S': key=KEY_ALT_S; break;
							case 'X': key=KEY_ALT_X; break;
							case 'Z': key=KEY_ALT_Z; break;
						}
					} else {
						if (!(event.xkey.state&(ControlMask|Mod1Mask)))
						{
							___push_key(buf[0]);
						}
					}
				} else if ((event.xkey.state&(ControlMask|Mod1Mask))==ControlMask)
				{
					switch (ks)
					{
						case XK_Up:        key=KEY_CTRL_UP;     break;
						case XK_Down:      key=KEY_CTRL_DOWN;   break;
						case XK_Right:     key=KEY_CTRL_RIGHT;  break;
						case XK_Left:      key=KEY_CTRL_LEFT;   break;
						case XK_Page_Up:   key=KEY_CTRL_PGUP;   break;
						case XK_Page_Down: key=KEY_CTRL_PGDN;   break;	
						case XK_Home:      key=KEY_CTRL_HOME;   break;
					}
				} else if ((event.xkey.state&(ControlMask|Mod1Mask))==Mod1Mask)
				{	
				} else if ((event.xkey.state&(ShiftMask|ControlMask|Mod1Mask))==ShiftMask)
				{
					switch (ks)
					{
						case XK_ISO_Left_Tab:
						case XK_Tab: ___push_key(KEY_SHIFT_TAB); return;
					}
				} else switch (ks)
				{
					case XK_Escape:    key=KEY_ESC;    break;
					case XK_F1:        key=KEY_F(1);     break;
					case XK_F2:        key=KEY_F(2);     break;
					case XK_F3:        key=KEY_F(3);     break;
					case XK_F4:        key=KEY_F(4);     break;
					case XK_F5:        key=KEY_F(5);     break;
					case XK_F6:        key=KEY_F(6);     break;
					case XK_F7:        key=KEY_F(7);     break;
					case XK_F8:        key=KEY_F(8);     break;
					case XK_F9:        key=KEY_F(9);     break;
					case XK_F10:       key=KEY_F(10);    break;
					case XK_F11:       key=KEY_F(11);    break;
					case XK_F12:       key=KEY_F(12);    break;
					case XK_Insert:    key=KEY_INSERT; break;
					case XK_Home:      key=KEY_HOME;   break;
					case XK_Page_Up:   key=KEY_PPAGE;  break;
					case XK_Delete:    key=KEY_DELETE; break;
					case XK_End:       key=KEY_END;    break;
					case XK_Page_Down: key=KEY_NPAGE;  break;
					case XK_Up:        key=KEY_UP;     break;
					case XK_Down:      key=KEY_DOWN;   break;
					case XK_Left:      key=KEY_LEFT;   break;
					case XK_Right:     key=KEY_RIGHT;  break;
					case XK_KP_Enter:
					case XK_Return:
					case XK_Linefeed:  key=_KEY_ENTER; break;
					case XK_BackSpace: key=KEY_BACKSPACE;break;
					case XK_ISO_Left_Tab:
					case XK_Tab:       key=KEY_TAB;    break;
				}
				if (key)
					___push_key(key);
				break;
			case KeyRelease:
				break;
		}
	}
}

static void create_window(void)
{
	XSetWindowAttributes xswa;
	XGCValues GCvalues;

	plDepth=XDefaultDepth(mDisplay, mScreen);

	xswa.background_pixel=BlackPixel(mDisplay, mScreen);
	xswa.border_pixel=WhitePixel(mDisplay, mScreen);
	xswa.event_mask=ExposureMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask/*|ResizeRedirectMask*/|StructureNotifyMask;
	xswa.override_redirect = False;
	if (!(window = XCreateWindow(mDisplay, DefaultRootWindow(mDisplay), 0, 0, plScrLineBytes, plScrLines, 0, plDepth, InputOutput, DefaultVisual(mDisplay, mScreen), CWEventMask|CWBackPixel|CWBorderPixel|CWOverrideRedirect, &xswa)))
	{
		fprintf(stderr, "[x11] Failed to create window\n");
		exit(-1);
	}
	XMapWindow(mDisplay, window);
	while (1)
	{
		XEvent event;
		XNextEvent(mDisplay, &event);
		if (event.type==Expose)
			break;
	}

	XChangeProperty (mDisplay, window,
			XA_NET_WM_NAME,
			XA_UTF8_STRING, 8,
     			PropModeReplace, (unsigned char *)"Open Cubic Player", 17);
	XChangeProperty (mDisplay, window,
			XA_WM_NAME,
			XA_STRING, 8,
			PropModeReplace, (unsigned char *)"Open Cubic Player", 17);

	if (XpmCreatePixmapFromData (mDisplay, window,
	                             opencubicplayer_xpm,
	                             &icon,
	                             &icon_mask,
	                             NULL) == XpmSuccess)
	{
		XWMHints hints;
		hints.flags = IconPixmapHint|IconMaskHint;
		hints.icon_pixmap = icon;
		hints.icon_mask = icon_mask;
		XSetWMHints (mDisplay, window, &hints); 	
	}

	GCvalues.function=GXcopy;
	if (!(copyGC=XCreateGC(mDisplay, window, GCFunction, &GCvalues)))
	{
		fprintf(stderr, "[x11] Failed to create GC object\n");
		exit(-1);
	}		
}

static void create_image(void)
{
	if (mLocalDisplay && XShmQueryExtension(mDisplay) )
	{
		shm_completiontype = XShmGetEventBase(mDisplay) + ShmCompletion;
		if (!(image = XShmCreateImage(mDisplay, XDefaultVisual(mDisplay, mScreen), XDefaultDepth(mDisplay, mScreen), ZPixmap, NULL, &shminfo[0], plScrLineBytes, plScrLines)))
		{
			fprintf(stderr, "[x11/shm] Failed to create XShmImage object\n");
			exit(-1);
		}
		shminfo[0].shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0777);
		if (shminfo[0].shmid < 0)
		{
			fprintf(stderr, "[x11/shm] shmget: %s\n", strerror(errno));
			exit(-1);
		}
		shminfo[0].shmaddr = (char *) shmat(shminfo[0].shmid, 0, 0);
		if (shminfo[0].shmaddr == ((char *) -1))
		{
			fprintf(stderr, "[x11/shm] shmat: %s\n", strerror(errno));
			exit(-1);
		}
		
		image->data = shminfo[0].shmaddr;
		shminfo[0].readOnly = False;
		XShmAttach(mDisplay, &shminfo[0]);
		
		XSync(mDisplay, False);
		
		shmctl(shminfo[0].shmid, IPC_RMID, 0);
	} else {
		if (!(image=XGetImage(mDisplay, window, 0, 0, plScrLineBytes, plScrLines, AllPlanes, ZPixmap)))
		{
			fprintf(stderr, "[x11] Failed to create XImage\n");
			exit(-1);
		}
	}
	plDepth = image->bits_per_pixel;
}

static void destroy_image(void)
{
	if (shm_completiontype>=0)
	{
		XShmDetach(mDisplay, &shminfo[0]);
		if (image)
			XDestroyImage(image);
		shmdt(shminfo[0].shmaddr);
		shm_completiontype=-1;
	} else {
		if (image)
			XDestroyImage(image);
	}
	image=NULL;
}

static void destroy_window(void)
{
	if (copyGC)
		XFreeGC(mDisplay, copyGC);
	copyGC=(GC)0;
	if (window)
		XDestroyWindow(mDisplay, window);
	if (icon)
		XFreePixmap(mDisplay, icon);
	if (icon_mask)
		XFreePixmap(mDisplay, icon_mask);
	window=(Window)0;
	icon=(Pixmap)0;
	icon_mask=(Pixmap)0;
}


static void RefreshScreenGraph(void);

static int ekbhit(void);

static int __plSetGraphMode(int high)
{
	if (high>=0)
	{
		set_state = set_state_graphmode;
		WindowResized = WindowResized_Graphmode;
	}

	if ((high==cachemode)&&(high>=0))
		goto quick;
	cachemode=high;

	if (virtual_framebuffer)
	{
		free(virtual_framebuffer);
		virtual_framebuffer=0;
	}
	destroy_image();

	if (high<0)
	{
		if (we_have_fullscreen)
			ewmh_fullscreen(window, 0);
		/*
		if (!autodetect)
			destroy_window();
		*/
		x11_common_event_loop();
		return 0;
	}

	___setup_key(ekbhit, ekbhit);
	_validkey=___valid_key;

	if (high==13)
	{
		plScrMode=13;
		plScrLineBytes=320;
		plScrLines=200;
		if ((Graphmode_modeline=modelines[MODE_320x200]))
			if (Graphmode_modeline->vdisplay>=240)
				plScrLines=240;
		plScrWidth=plScrLineBytes/8;
		plScrHeight=plScrLines/16;
	} else if (high)
	{
		plScrMode=101;
		plScrWidth=128;
		plScrHeight=48;
		plScrLineBytes=1024;
		plScrLines=768;
		Graphmode_modeline=modelines[MODE_1024x768];
	} else {
		plScrMode=100;
		plScrWidth=80;
		plScrHeight=30;
		plScrLineBytes=640;
		plScrLines=480;
		Graphmode_modeline=modelines[MODE_640x480];
	}
	if (!Graphmode_modeline)
	{
		fprintf(stderr, "[x11] unable to find modeline, this should not happen\n");
		fprintf(stderr, "[x11] (fullscreen will not cover entire screen)\n");
		Graphmode_modeline = &default_modeline; /* should not be able to happen.... */
	}
	___push_key(VIRT_KEY_RESIZE);

	plScrRowBytes=plScrWidth*2;
	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}

	if (!window)
		create_window();

	set_state_graphmode (do_fullscreen);

	create_image();

	if ((plDepth!=8) || (plScrLineBytes!=image->bytes_per_line))
	{
		virtual_framebuffer=calloc(plScrLineBytes, plScrLines);
		plVidMem=virtual_framebuffer;
	} else {
		virtual_framebuffer=0;
		plVidMem=image->data;
	}
quick:
	memset(image->data, 0, image->bytes_per_line * plScrLines);
	if (virtual_framebuffer)
		memset(virtual_framebuffer, 0, plScrLineBytes * plScrLines);

	x11_gflushpal();
	return 0;
}

static void __vga13(void)
{
	_plSetGraphMode(13);
}

#ifdef PFONT_IDRAWBAR
static uint8_t bartops[18]="\xB5\xB6\xB6\xB7\xB7\xB8\xBD\xBD\xBE\xC6\xC6\xC7\xC7\xCF\xCF\xD7\xD7";
static uint8_t ibartops[18]="\xB5\xD0\xD0\xD1\xD1\xD2\xD2\xD3\xD3\xD4\xD4\xD5\xD5\xD6\xD6\xD7\xD7";
#else
static uint8_t bartops[18]="\xB5\xB6\xB7\xB8\xBD\xBE\xC6\xC7\xCF\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7";
#endif

static unsigned int curshape=0, curposx=0, curposy=0;

static void plSetTextMode(unsigned char x)
{
	struct modes_t
	{
		int charx, chary, windowx, windowy, bigfont/*, modeline*/;
	};
	const struct modes_t modes[8]=
	{
		{  80,  25,  640,  400, 1/*, MODE_640x400*/},
		{  80,  30,  640,  480, 1/*, MODE_640x480*/},
		{  80,  50,  640,  400, 0/*, MODE_640x400*/},
		{  80,  60,  640,  480, 0/*, MODE_640x480*/},
		{ 128,  48, 1024,  768, 1/*, MODE_1024x768*/},
		{ 160,  64, 1280, 1024, 1/*, MODE_1280x1024*/},
		{ 128,  96, 1024,  768, 0/*, MODE_1024x768*/},
		{ 160, 128, 1280, 1024, 0/*, MODE_1280x1024*/}
	};

	set_state = set_state_textmode;
	WindowResized = WindowResized_Textmode;

	___setup_key(ekbhit, ekbhit);
	_validkey=___valid_key;

	if (x==plScrMode)
	{
		memset(vgatextram, 0, plScrHeight * 2 * plScrWidth);
		return;
	}

	_plSetGraphMode(-1);

	destroy_image();

	if (x==255)
	{
		if (window)
		{
			vo_showcursor(mDisplay, window);
			if (we_have_fullscreen)
				ewmh_fullscreen(window, 0);
			XDestroyWindow(mDisplay, window);
			window=(Window)0;
		}
		if (xvidmode_event_base>=0)
		{
			XF86VidModeSwitchToMode(mDisplay, mScreen, &default_modeline);
		}

		XUngrabKeyboard(mDisplay, CurrentTime);
		XUngrabPointer(mDisplay, CurrentTime);

		XSync(mDisplay, False);
		plScrMode=255;

		return; /* gdb helper */
	}

	/* if invalid mode, set it to zero */
	if (x>7)
		x=0;
#warning We have disabled textmode being able to change resolution of Screen
#if 0

	if (do_fullscreen)
	{
		if (!modelines[modes[x].modeline])
		{
			/* scale modes upwards until we have a modeline */
			for (i=x;i<8;i++)
				if (modelines[modes[i].modeline])
				{
					x=i;
					break;
				}
		}
		if (!modelines[modes[x].modeline])
		{
		/* not found, scale down */
			for (i=x;i>=0;i--)
				if (modelines[modes[i].modeline])
				{
					x=i;
					break;
				}
		}
	}
	modeline=modelines[modes[x].modeline];
#endif


	#warning TODO
	plScrHeight=modes[x].chary;
	plScrWidth=modes[x].charx;
	plScrRowBytes=plScrWidth*2;
	plScrLineBytes=modes[x].windowx;
	plScrLines=modes[x].windowy;
	___push_key(VIRT_KEY_RESIZE);
	//plUseFont = modes[x].bigfont ? _8x16 : _8x8;

	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
	vgatextram = calloc (plScrHeight * 2, plScrWidth);
	if (!vgatextram)
	{
		fprintf(stderr, "[x11] calloc() failed\n");
		exit(-1);
	}

	plScrType=plScrMode=x;
	/*memset(vgatextram, 0, plScrHeight * 2 * plScrWidth); */

	plDepth=XDefaultDepth(mDisplay, mScreen);
	if (!window)
		create_window();
	/*set_state(do_fullscreen);*/
	TextModeSetState(plUseFont, do_fullscreen);
	plUseFont=plCurrentFont;
	
/*	XSetWMProtocols (mDisplay, window, &wm_delete_window, 1);*/

	create_image();

	x11_gflushpal();
}

static void displayvoid(uint16_t y, uint16_t x, uint16_t len)
{
	uint8_t *addr=vgatextram+y*plScrRowBytes+x*2;
	while (len--)
	{
		*addr++=0;
		*addr++=plpalette[0];
	}
}

static void displaystrattr(uint16_t y, uint16_t x, const uint16_t *buf, uint16_t len)
{
	uint8_t *p=vgatextram+(y*plScrRowBytes+x*2);
	while (len)
	{
		*(p++)=(*buf)&0x0ff;
		*(p++)=plpalette[((*buf)>>8)];
		buf++;
		len--;
	}
}

static void displaystr(uint16_t y, uint16_t x, uint8_t attr, const char *str, uint16_t len)
{
	uint8_t *p=vgatextram+(y*plScrRowBytes+x*2);
	int i;
	attr=plpalette[attr];
	for (i=0; i<len; i++)
	{
		*p++=*str;
		if (*str)
			str++;
		*p++=attr;
	}
}

static void drawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	char buf[60];
	unsigned int i;
	uint8_t *scrptr;
	unsigned int yh1, yh2;

	if (hgt>((yh*(unsigned)16)-4))
		hgt=(yh*16)-4;
	for (i=0; i<yh; i++)
	{
		if (hgt>=16)
		{
			buf[i]=bartops[16];
			hgt-=16;
		} else {
			buf[i]=bartops[hgt];
			hgt=0;
		}
	}
	scrptr=vgatextram+(2*x+yb*plScrRowBytes);
	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2;
	for (i=0; i<yh1; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<yh; i++, scrptr-=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
}
#ifdef PFONT_IDRAWBAR
static void idrawbar(uint16_t x, uint16_t yb, uint16_t yh, uint32_t hgt, uint32_t c)
{
	unsigned char buf[60];
	unsigned int i;
	uint8_t *scrptr;
	unsigned int yh1=(yh+2)/3;
	unsigned int yh2=(yh+yh1+1)/2;

	if (hgt>((yh*(unsigned)16)-4))
	  hgt=(yh*16)-4;

	scrptr=vgatextram+(2*x+(yb-yh+1)*plScrRowBytes);

	for (i=0; i<yh; i++)
	{
		if (hgt>=16)
		{
			buf[i]=ibartops[16];
			hgt-=16;
		} else {
			buf[i]=ibartops[hgt];
			hgt=0;
		}
	}
	yh1=(yh+2)/3;
	yh2=(yh+yh1+1)/2;
	for (i=0; i<yh1; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh1; i<yh2; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
	c>>=8;
	for (i=yh2; i<yh; i++, scrptr+=plScrRowBytes)
	{
		scrptr[0]=buf[i];
		scrptr[1]=plpalette[c&0xFF];
	}
}
#endif

static void RefreshScreenGraph(void)
{
	if (virtual_framebuffer)
	{
		uint8_t *src=(uint8_t *)virtual_framebuffer;
		uint8_t *dst_line = (uint8_t *)image->data;
		int Y=0, j;

		if (plDepth==32)
		{ 
			uint32_t *dst;
			while (1)
			{
				dst = (uint32_t *)dst_line;
				for (j=0;j<plScrLineBytes;j++)
					*(dst++)=x11_palette32[*(src++)];
				if ((++Y)>=plScrLines)
					break;
				dst_line += image->bytes_per_line;
			}
		} else if (plDepth==24)
		{ 
			uint8_t *dst;
			while (1)
			{
				dst = (uint8_t *)dst_line;
				for (j=0;j<plScrLineBytes;j++)
				{
					*(dst++)=x11_palette32[*src]&255;
					*(dst++)=(x11_palette32[*src]>>8) & 255;
					*(dst++)=(x11_palette32[*src++]>>16) & 255;
				}
				if ((++Y)>=plScrLines)
					break;
				dst_line += image->bytes_per_line;
			}
		} else if (plDepth==16)
		{ 
			uint16_t *dst;
			while (1)
			{
				dst = (uint16_t *)dst_line;
				for (j=0;j<plScrLineBytes;j++)
					*(dst++)=x11_palette16[*(src++)];
				if ((++Y)>=plScrLines)
					break;
				dst_line += image->bytes_per_line;
			}
		} else if (plDepth==15)
		{ 
			uint16_t *dst;
			while (1)
			{
				dst = (uint16_t *)dst_line;
				for (j=0;j<plScrLineBytes;j++)
					*(dst++)=x11_palette15[*(src++)];
				if ((++Y)>=plScrLines)
					break;
				dst_line += image->bytes_per_line;
			}
		} else if (plDepth==8)
		{ 
			uint8_t *dst;
			while (1)
			{
				dst = dst_line;
				memcpy(dst, src, plScrLineBytes);
				src+=plScrLineBytes;
				if ((++Y)>=plScrLines)
					break;
				dst_line += image->bytes_per_line;
			}
		}
	}
	if (shm_completiontype>=0)
		XShmPutImage(mDisplay, window, copyGC, image, 0, 0, 0, (plScrLines==240?20:0), plScrLineBytes, plScrLines, True);
	else
		XPutImage(mDisplay, window, copyGC, image, 0, 0, 0, (plScrLines==240?20:0), plScrLineBytes, plScrLines);
}

static void RefreshScreenText(void)
{
	unsigned int x, y;
	uint8_t *mem=vgatextram;
	int doshape=0;
	uint8_t save=save;
	int precalc_linelength;

	if (!window)
		return;
	if (!image)
		return;
	
	if (curshape)
	if (time(NULL)&1)
		doshape=curshape;

	if (doshape==2)
	{
		save=vgatextram[curposy*plScrRowBytes+curposx*2];
		vgatextram[curposy*plScrRowBytes+curposx*2]=219;
	}

	switch (plDepth)
	{
		case 8:
			switch (plCurrentFont)
			{
			case _8x16:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*16*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f, b, a, *cp;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 8;
						cp=plFont816[*(mem++)];
						a=*(mem++);
						f=a&15;
						b=a>>4;
						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr+=image->bytes_per_line-8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=8;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=image->bytes_per_line+8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _8x8:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*8*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f, b, a, *cp;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 8;
						cp=plFont88[*(mem++)];
						a=*(mem++);
						f=a&15;
						b=a>>4;
						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr+=image->bytes_per_line-8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=8;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=image->bytes_per_line+8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _4x4:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*4*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f, b, a, *cp;
						uint8_t *scr=scr_precalc;
						int i, j;
						scr_precalc += 4;
						cp=plFont44[*(mem++)];
						a=*(mem++);
						f=a&15;
						b=a>>4;
						for (i=0; i<2; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr+=image->bytes_per_line-4;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr+=image->bytes_per_line-4;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=4;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr-=image->bytes_per_line+4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
								scr-=image->bytes_per_line+4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			}
			break;
		case 15:
			precalc_linelength = image->bytes_per_line/sizeof(uint16_t);
			switch (plCurrentFont)
			{
			case _8x16:
				for (y=0;y<plScrHeight;y++)
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr=((uint16_t *)image->data)+y*8*image->bytes_per_line+x*8;
						int i, j;
						cp=plFont816[*(mem++)];
						a=*(mem++);
						f=x11_palette15[a&15];
						b=x11_palette15[a>>4];
						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=8;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				break;
			case _8x8:
				for (y=0;y<plScrHeight;y++)
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr=((uint16_t *)image->data)+y*4*image->bytes_per_line+x*8;
						int i, j;
						cp=plFont88[*(mem++)];
						a=*(mem++);
						f=x11_palette15[a&15];
						b=x11_palette15[a>>4];
						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=8;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				break;
			case _4x4:
				for (y=0;y<plScrHeight;y++)
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr=((uint16_t *)image->data)+y*2*image->bytes_per_line+x*4;
						int i, j;
						cp=plFont44[*(mem++)];
						a=*(mem++);
						f=x11_palette15[a&15];
						b=x11_palette15[a>>4];
						for (i=0; i<2; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=4;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				break;
			}
			break;
		case 16:
			precalc_linelength = image->bytes_per_line/sizeof(uint16_t);
			switch (plCurrentFont)
			{
			case _8x16:
				for (y=0;y<plScrHeight;y++)
				{
					uint16_t *scr_precalc = ((uint16_t *)image->data)+y*16*image->bytes_per_line/sizeof(uint16_t);
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr = scr_precalc;
						int i, j;

						scr_precalc += 8;

						cp=plFont816[*(mem++)];
						a=*(mem++);
						f=x11_palette16[a&15];
						b=x11_palette16[a>>4];
						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=8;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _8x8:
				for (y=0;y<plScrHeight;y++)
				{
					uint16_t *scr_precalc = ((uint16_t *)image->data)+y*8*image->bytes_per_line/sizeof(uint16_t);
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr = scr_precalc;
						int i, j;

						scr_precalc +=8 ;

						cp=plFont88[*(mem++)];
						a=*(mem++);
						f=x11_palette16[a&15];
						b=x11_palette16[a>>4];
						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=8;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _4x4:
				for (y=0;y<plScrHeight;y++)
				{
					uint16_t *scr_precalc = ((uint16_t *)image->data)+y*2*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t a, *cp;
						uint16_t f, b;
						uint16_t *scr = scr_precalc;
						int i, j;

						scr_precalc +=4 ;

						cp=plFont44[*(mem++)];
						a=*(mem++);
						f=x11_palette16[a&15];
						b=x11_palette16[a>>4];
						for (i=0; i<2; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;
							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;

						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=8;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;

			}
			break;
		case 24:
#if 0
			switch (plCurrentFont)
			{
			case _8x16:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*16*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f1, f2, f3, b1, b2, b3;
						int i, j;
						uint8_t a, *cp;
						uint8_t *scr = scr_precalc;
						scr_precalc += 24;

						cp=plFont816[*(mem++)];
						a=*(mem++);
						f1=x11_palette32[a&15] & 255;
						f2=(x11_palette32[a&15]>>8) & 255;
						f3=(x11_palette32[a&15]>>16) & 255;
						b1=x11_palette32[a>>4] & 255;
						b2=(x11_palette32[a>>4]>>8) & 255;
						b3=(x11_palette32[a>>4]>>16) & 255;

						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								if (bitmap&128)
								{
									*scr++=f1;
									*scr++=f2;
									*scr++=f3;
								} else {
									*scr++=b1;
									*scr++=b2;
									*scr++=b3;
								}
								bitmap<<=1;
							}
							scr += image->bytes_per_line - 24;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=24;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= image->bytes_per_line + 24;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
									{
										scr[0]=f1;
										scr[1]=f2;
										scr[2]=f3;
									}
									bitmap>>=1;
									scr+=3;
								}
							}
						}
					}
				}
				break;
			case _8x8:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*8*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f1, f2, f3, b1, b2, b3;
						int i, j;
						uint8_t a, *cp;
						uint8_t *scr=scr_precalc;
						scr_precalc+=24;
						cp=plFont88[*(mem++)];
						a=*(mem++);
						f1=x11_palette32[a&15] & 255;
						f2=(x11_palette32[a&15]>>8) & 255;
						f3=(x11_palette32[a&15]>>16) & 255;
						b1=x11_palette32[a>>4] & 255;
						b2=(x11_palette32[a>>4]>>8) & 255;
						b3=(x11_palette32[a>>4]>>16) & 255;

						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								if (bitmap&128)
								{
									*scr++=f1;
									*scr++=f2;
									*scr++=f3;
								} else {
									*scr++=b1;
									*scr++=b2;
									*scr++=b3;
								}
								bitmap<<=1;
							}
							scr += image->bytes_per_line - 24;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=24;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= image->bytes_per_line + 24;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
									{
										scr[0]=f1;
										scr[1]=f2;
										scr[2]=f3;
									}
									bitmap>>=1;
									scr+=3;
								}
							}
						}
					}
				}
				break;
			case _4x4:
				for (y=0;y<plScrHeight;y++)
				{
					uint8_t *scr_precalc = ((uint8_t *)image->data)+y*4*image->bytes_per_line;
					for (x=0;x<plScrWidth;x++)
					{
						uint8_t f1, f2, f3, b1, b2, b3;
						int i, j;
						uint8_t a, *cp;
						uint8_t *scr=scr_precalc;
						scr_precalc+=12;
						cp=plFont44[*(mem++)];
						a=*(mem++);
						f1=x11_palette32[a&15] & 255;
						f2=(x11_palette32[a&15]>>8) & 255;
						f3=(x11_palette32[a&15]>>16) & 255;
						b1=x11_palette32[a>>4] & 255;
						b2=(x11_palette32[a>>4]>>8) & 255;
						b3=(x11_palette32[a>>4]>>16) & 255;

						for (i=0; i<2; i++)
						{
							uint8_t bitmap = *cp++;

							for (j=0; j<4; j++)
							{
								if (bitmap&128)
								{
									*scr++=f1;
									*scr++=f2;
									*scr++=f3;
								} else {
									*scr++=b1;
									*scr++=b2;
									*scr++=b3;
								}
								bitmap<<=1;
							}
							scr += image->bytes_per_line - 12;

							for (j=0; j<4; j++)
							{
								if (bitmap&128)
								{
									*scr++=f1;
									*scr++=f2;
									*scr++=f3;
								} else {
									*scr++=b1;
									*scr++=b2;
									*scr++=b3;
								}
								bitmap<<=1;
							}
							scr += image->bytes_per_line - 12;

						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=12;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= image->bytes_per_line + 12;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
									{
										scr[0]=f1;
										scr[1]=f2;
										scr[2]=f3;
									}
									bitmap>>=1;
									scr+=3;
								}
								scr -= image->bytes_per_line + 12;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
									{
										scr[0]=f1;
										scr[1]=f2;
										scr[2]=f3;
									}
									bitmap>>=1;
									scr+=3;
								}
							}
						}
					}
				}
				break;
			}
			break;
#endif
		case 32:
			precalc_linelength = image->bytes_per_line/sizeof(uint32_t);
			switch (plCurrentFont)
			{
			case _8x16:
				for (y=0;y<plScrHeight;y++)
				{
					uint32_t *scr_precalc = ((uint32_t *)image->data)+y*16*image->bytes_per_line/sizeof(uint32_t);
					for (x=0;x<plScrWidth;x++)
					{
						uint32_t f, b;
						int i, j;
						uint8_t a, *cp;
						uint32_t *scr = scr_precalc;
						scr_precalc +=8 ;

						cp=plFont816[*(mem++)];
						a=*(mem++);
						f=x11_palette32[a&15];
						b=x11_palette32[a>>4];
						for (i=0; i<16; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont816['_']+15;
							scr+=8;
							for (i=0; i<16; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _8x8:
				for (y=0;y<plScrHeight;y++)
				{
					uint32_t *scr_precalc = ((uint32_t *)image->data)+y*8*image->bytes_per_line/sizeof(uint32_t);	
					for (x=0;x<plScrWidth;x++)
					{
						uint32_t f, b;
						int i, j;
						uint8_t a, *cp;
						uint32_t *scr=scr_precalc;
						scr_precalc+=8;
						cp=plFont88[*(mem++)];
						a=*(mem++);
						f=x11_palette32[a&15];
						b=x11_palette32[a>>4];
						for (i=0; i<8; i++)
						{
							uint8_t bitmap=*cp++;
							for (j=0; j<8; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 8;
						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont88['_']+7;
							scr+=8;
							for (i=0; i<8; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 8;
								for (j=0; j<8; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
							}
						}
					}
				}
				break;
			case _4x4:
				for (y=0;y<plScrHeight;y++)
				{
					uint32_t *scr_precalc = ((uint32_t *)image->data)+y*4*image->bytes_per_line/sizeof(uint32_t);	
					for (x=0;x<plScrWidth;x++)
					{
						uint32_t f, b;
						int i, j;
						uint8_t a, *cp;
						uint32_t *scr=scr_precalc;
						scr_precalc+=4;
						cp=plFont44[*(mem++)];
						a=*(mem++);
						f=x11_palette32[a&15];
						b=x11_palette32[a>>4];
						for (i=0; i<2; i++)
						{
							uint8_t bitmap = *cp++;

							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;

							for (j=0; j<4; j++)
							{
								*scr++=(bitmap&128)?f:b;
								bitmap<<=1;
							}
							scr += precalc_linelength - 4;

						}
						if ((doshape==1)&&(curposy==y)&&(curposx==x))
						{
							cp=plFont44['_']+1;
							scr+=4;
							for (i=0; i<2; i++)
							{
								uint8_t bitmap=*cp--;
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}
								scr -= precalc_linelength + 4;
								for (j=0; j<4; j++)
								{
									if (bitmap&1)
										*scr=f;
									bitmap>>=1;
									scr++;
								}

							}
						}
					}
				}
				break;
			}
			break;
	}

	if (doshape==2)
		vgatextram[curposy*plScrRowBytes+curposx*2]=save;

	if (shm_completiontype>=0)
		XShmPutImage(mDisplay, window, copyGC, image, 0, 0, 0, 0, plScrLineBytes, plScrLines, True);
	else
		XPutImage(mDisplay, window, copyGC, image, 0, 0, 0, 0, plScrLineBytes, plScrLines);
}
static int ekbhit(void)
{
	if (plScrMode<8)
	{
		RefreshScreenText();
	} else {
		RefreshScreenGraph();
	}

	x11_common_event_loop();

	return 0;
}
static int conRestore(void)
{
	return 0;
}
static void conSave(void)
{
}
static void plDosShell(void)
{
	pid_t child;
	XEvent event;
	
	if (xvidmode_event_base>=0)
	{
		XF86VidModeSwitchToMode(mDisplay, mScreen, &default_modeline);
	}
	if (we_have_fullscreen)
		ewmh_fullscreen(window, 0);
	XUngrabKeyboard(mDisplay, CurrentTime);
	XUngrabPointer(mDisplay, CurrentTime);
	XUnmapWindow(mDisplay, window);

	XSync(mDisplay, False);
	while (XPending(mDisplay))
		XNextEvent(mDisplay, &event);

	if (!(child=fork()))
	{
		char *shell=getenv("SHELL");
		if (!shell)
			shell="/bin/sh";
		if (!isatty(2))
		{
			close(2);
			if (dup(1)!=2)
				fprintf(stderr, __FILE__ ": dup(1) != 2\n");
		}
		execl(shell, shell, NULL);
		perror("execl()");
		exit(-1);
	} else if (child>0)
	{
		while(1)
		{
			int status, retval;
			if ((retval=waitpid(child, &status, 0))<0)
			{
				if (errno==EINTR)
					continue;
			}
			break;
		}
	}

	XMapWindow(mDisplay, window);
	set_state(do_fullscreen);
}
static void setcur(uint8_t y, uint8_t x)
{
	curposx=x;
	curposy=y;
}
static void setcurshape(uint16_t shape)
{
	curshape=shape;
}

static int ___valid_key(uint16_t key)
{
	switch (key)
	{
		case KEY_ESC:
		case KEY_NPAGE:
		case KEY_PPAGE:
		case KEY_HOME:
		case KEY_END:
		case KEY_TAB:
		case _KEY_ENTER:
		case KEY_DOWN:
		case KEY_UP:
		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_ALT_A:
		case KEY_ALT_B:
		case KEY_ALT_C:
		case KEY_ALT_E:
		case KEY_ALT_G:
		case KEY_ALT_I:
		case KEY_ALT_K:
		case KEY_ALT_L:
		case KEY_ALT_M:
		case KEY_ALT_O:
		case KEY_ALT_P:
		case KEY_ALT_R:
		case KEY_ALT_S:
		case KEY_ALT_X:
		case KEY_ALT_Z:
		case KEY_BACKSPACE:
		case KEY_F(1):
		case KEY_F(2):
		case KEY_F(3):
		case KEY_F(4):
		case KEY_F(5):
		case KEY_F(6):
		case KEY_F(7):
		case KEY_F(8):
		case KEY_F(9):
		case KEY_F(10):
		case KEY_F(11):
		case KEY_F(12):
		case KEY_DELETE:
		case KEY_INSERT:
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '0':
		case '/':
		case '*':
		case '-':
		case '+':
		case '\\':
		case '\'':
		case ',':
		case '.':
		case '?':
		case '!':
		case '>':
		case '<':
		case KEY_SHIFT_TAB:
		case KEY_CTRL_P:
		case KEY_CTRL_D:
		case KEY_CTRL_H:
		case KEY_CTRL_J:
		case KEY_CTRL_K:
		case KEY_CTRL_L:
		case KEY_CTRL_Q:
		case KEY_CTRL_S:
		case KEY_CTRL_Z:
		case KEY_CTRL_BS:
		case KEY_CTRL_UP:
		case KEY_CTRL_DOWN:
		case KEY_CTRL_LEFT:
		case KEY_CTRL_RIGHT:
		case KEY_CTRL_PGUP:
		case KEY_CTRL_PGDN:
		case KEY_CTRL_ENTER:
		case KEY_CTRL_HOME:
			return 1;

		default:
			fprintf(stderr, __FILE__ ": unknown key 0x%04x\n", (int)key);
		case KEY_ALT_ENTER:
			return 0;
	}
}

static const char *plGetDisplayTextModeName(void)
{
	static char mode[32];
	snprintf(mode, sizeof(mode), "res(%dx%d), font(%s)%s", plScrWidth, plScrHeight,
		plUseFont == _4x4 ? "4x4"
		: plUseFont == _8x8 ? "8x8" : "8x16", do_fullscreen?" fullscreen":"");
	return mode;
}

/*
static void Recalc_Scene(void)
{
	switch (plUseFont)
	{
		case _4x4:
			plScrWidth = plScrLineBytes / 4;
			plScrHeight = plScrLines / 4;
			break;
		case _8x8:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 8;
			break;
		case _8x16:
			plScrWidth = plScrLineBytes / 8;
			plScrHeight = plScrLines / 16;
			break;
	}
	plScrRowBytes=plScrWidth*2;
}
*/


static void plDisplaySetupTextMode(void)
{
	while (1)
	{
		uint16_t c;

		memset(vgatextram, 0, plScrHeight * 2 * plScrWidth);

		make_title("x11-driver setup");
		displaystr(1, 0, 0x07, "1:  font-size:", 14);
		displaystr(1, 15, plCurrentFont == _4x4 ? 0x0f : 0x07 , "4x4", 3);
		displaystr(1, 19, plCurrentFont == _8x8 ? 0x0f : 0x07, "8x8", 3);
		displaystr(1, 23, plCurrentFont == _8x16 ? 0x0f : 0x07, "8x16", 4);
		displaystr(2, 0, 0x07, "2:  fullscreen: ", 16);
		displaystr(3, 0, 0x07, "3:  resolution in fullscreen:", 29);

		displaystr(plScrHeight-1, 0, 0x17, "  press the number of the item you wish to change and ESC when done", plScrWidth);

		while (!_ekbhit())
				framelock();
		c=_egetch();

		switch (c)
		{
			case '1':
				TextModeSetState((plUseFont+1)%3, do_fullscreen);
				plUseFont = plCurrentFont;
				break;
			case 27: return;
		}
	}
}

int x11_init(int use_explicit)
{
	if ( (!use_explicit) && (!cfGetProfileBool("x11", "autodetect", 1, 0)) )
		return -1;

	plUseFont = cfGetProfileInt("x11", "font", _8x8, 10);
	if ( (plUseFont > _FONT_MAX) /*|| (plUseFont < 0 )*/)
		plUseFont = _8x8;

	if (x11_connect())
		return -1;

	plScrMode=255;
	
	xvidmode_init();

	ewmh_init();	

	_plSetGraphMode=__plSetGraphMode;
	_gdrawstr=generic_gdrawstr;
	_gdrawchar8=generic_gdrawchar8;
	_gdrawchar8p=generic_gdrawchar8p;
	_gdrawchar8t=generic_gdrawchar8t;
	_gdrawcharp=generic_gdrawcharp;
	_gdrawchar=generic_gdrawchar;
	_gupdatestr=generic_gupdatestr;
	_gupdatepal=x11_gupdatepal;
	_gflushpal=x11_gflushpal;
	_vga13=__vga13;

	_plGetDisplayTextModeName = plGetDisplayTextModeName;
	_plDisplaySetupTextMode = plDisplaySetupTextMode;

	plVidType=vidVESA;

	_displayvoid=displayvoid;
	_displaystrattr=displaystrattr;
	_displaystr=displaystr;
	___setup_key(ekbhit, ekbhit); /* filters in more keys */
	_validkey=___valid_key;

	_plSetTextMode=plSetTextMode;
	_drawbar=drawbar;
#ifdef PFONT_IDRAWBAR
	_idrawbar=idrawbar;
#else
	_idrawbar=drawbar;
#endif
		
	_conRestore=conRestore;
	_conSave=conSave;
	_plDosShell=plDosShell;

	_setcur=setcur;
	_setcurshape=setcurshape;

	plSetTextMode(0);
	return 0;
}

void x11_done(void)
{
	if (!mDisplay)
		return;
	destroy_image();
	destroy_window();
	xvidmode_done();
	x11_disconnect();

	if (vgatextram)
	{
		free(vgatextram);
		vgatextram=0;
	}
}
