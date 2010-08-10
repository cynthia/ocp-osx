/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * CPIface link info screen
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "stuff/poutput.h"
#include "filesel/pfilesel.h"
#include "cpiface.h"
#include "boot/plinkman.h"

#include <curses.h>

static int plWinFirstLine;
static int plWinHeight;

static int plHelpHeight;
static int plHelpScroll;
static int mode;


static void plDisplayHelp(void)
{
	int y;
	int x;
      	struct linkinfostruct l;

	plHelpHeight=lnkCountLinks()*(mode?2:1);
	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;
	if (plHelpScroll<0)
		plHelpScroll=0;
	displaystr(plWinFirstLine, 0, 0x09, "  Link View", 15);
	displaystr(plWinFirstLine, 15, 0x08, "press tab to toggle copyright                               ", 65);

	x=0;
	for (y=0; y<plWinHeight; y++)
	{
		uint16_t buf[/*80*/132];
		writestring(buf, 0, 0, "", /*80*/132);
		if (lnkGetLinkInfo(&l, (y+plHelpScroll)/(mode?2:1)))
		{
			int dl=strlen(l.desc);
			int i;
			const char *d2;

			for (i=0; i<dl; i++)
				if (!strncasecmp(l.desc+i, "(c)", 3))
					break;
			d2=l.desc+i;
			if (i>/*58*/110)
				i=/*58*/110;
			if (!((y+plHelpScroll)&1)||!mode)
			{
				writestring(buf, 2, 0x0A, l.name, 8);
				if (l.size)
				{
					writenum(buf, 12, 0x07, (l.size+1023)>>10, 10, 6, 1);
					writestring(buf, 18, 0x07, "k", 1);
				} else
					writestring(buf, 12, 0x07, "builtin", 7);
				writestring(buf, 22, 0x0F, l.desc, i);
			} else {
				char vbuf[30];
				strcpy(vbuf, "version ");
				convnum(l.ver>>16, vbuf+strlen(vbuf), 10, 3, 1);
				strcat(vbuf, ".");
				if ((signed char)(l.ver>>8)>=0)
					convnum((signed char)(l.ver>>8), vbuf+strlen(vbuf), 10, 2, 0);
				else
				{
					strcat(vbuf, "-");
					convnum(-(signed char)(l.ver>>8)/10, vbuf+strlen(vbuf), 10, 1, 0);
				}
				strcat(vbuf, ".");
				convnum((unsigned char)l.ver, vbuf+strlen(vbuf), 10, 2, 0);
				writestring(buf, 2, 0x08, vbuf, 17);
				writestring(buf, 24, 0x08, d2, 108 /* 56 */);
			}
		}
		displaystrattr(y+plWinFirstLine+1, 0, buf, 132 /* 80 */);
	}
}

static int plHelpKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(KEY_PPAGE, "Scroll up");
			cpiKeyHelp(KEY_NPAGE, "Scroll down");
			cpiKeyHelp(KEY_HOME, "Scroll to to the first line");
			cpiKeyHelp(KEY_END, "Scroll to to the last line");
			cpiKeyHelp(KEY_TAB, "Toggle copyright on/off");
			cpiKeyHelp(KEY_CTRL_PGUP, "Scroll a page up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Scroll a page down");
			return 0;
		case KEY_TAB:
			if (mode)
				plHelpScroll/=2;
			else
				plHelpScroll*=2;
			mode=!mode;
			break;
		/*case 0x4900: //pgup*/
		case KEY_PPAGE:
			plHelpScroll--;
			break;
		/*case 0x5100: //pgdn*/
		case KEY_NPAGE:
			plHelpScroll++;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x8400: //ctrl-pgup */
			plHelpScroll-=plWinHeight;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x7600: //ctrl-pgdn */
			plHelpScroll+=plWinHeight;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plHelpScroll=0;
			break;
		/*case 0x4F00: //end*/
		case KEY_END:
			plHelpScroll=plHelpHeight;
			break;
		default:
			return 0;
	}
	if ((plHelpScroll+plWinHeight)>plHelpHeight)
		plHelpScroll=plHelpHeight-plWinHeight;
	if (plHelpScroll<0)
		plHelpScroll=0;
	return 1;
}

static void hlpDraw(void)
{
	cpiDrawGStrings();
	plDisplayHelp();
}

static void hlpSetMode()
{
	cpiSetTextMode(fsScrType);
	plWinFirstLine=5;
	plWinHeight=plScrHeight-6;
}

static int hlpIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('\'', "View loaded dll/plugins");
			return 0;
		case '\'':
			cpiSetMode("links");
			break;
		default:
			return 0;
	}
	return 1;
}

static int hlpEvent(int ignore)
{
	return 1;
}

static struct cpimoderegstruct cpiModeLinks = {"links", hlpSetMode, hlpDraw, hlpIProcessKey, plHelpKey, hlpEvent CPIMODEREGSTRUCT_TAIL};

static void __attribute__((constructor))init(void)
{
	cpiRegisterDefMode(&cpiModeLinks);
}

static void __attribute__((destructor))done(void)
{
	cpiUnregisterDefMode(&cpiModeLinks);
}
