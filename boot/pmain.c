/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * PMain - main module (loads and inits all startup modules)
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    - plScreenChanged variable to notify the interfaces when the
 *      screen mode has changed
 *    - added command line help
 *    - replaced INI link symbol reader with _dllinfo reader
 *    - added screen mode check for avoiding redundant mode changes
 *    - various minor changes
 *  -fd981016   Felix Domke    <tmbinc@gmx.net>
 *    - Win32-Port
 *  -doj981213  Dirk Jagdmann  <doj@cubic.org>
 *    - added the nice end ansi
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    - added stack dump and fault-in-faultproc-check
 *  -kb981224   Tammo Hinrichs <kb@ms.demo.org>
 *    - cleaned up dos shell code a bit (but did not help much)
 *  -doj990421  Dirk Jagdmann  <doj@cubic.org>
 *    - changed conSave(), conRestore, conInit()
 *  -fd990518   Felix Domke <tmbinc@gmx.net>
 *    - clearscreen now works in higher-modes too. dos shell now switches
 *      to mode 3
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    - rewritten for unix
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"

#include "pmain.h"
#include "stuff/err.h"
#include "stuff/poutput.h"
#include "plinkman.h"
#include "psetting.h"

struct mainstruct *ocpmain = 0;

static void plCloseAll(void)
{
	int i;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->PreClose)
			loadlist[i].info->PreClose();

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->Close)
			loadlist[i].info->Close();

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->LateClose)
			loadlist[i].info->LateClose();
}

static int cmdhlp(void)
{
	if (cfGetProfileString("commandline", "h", 0) || cfGetProfileString("commandline", "?", 0) || cfGetProfileString("commandline--", "help", 0))
	{
		printf("\nopencp command line help\n");
		printf("Usage: ocp [<options>]* [@<playlist>]* [<modulename>]* \n");
		printf("\nOptions:\n");
		printf("-h                : show this help\n");
		printf("-c<name>          : use specific configuration\n");
		printf("-f : fileselector settings\n");
		printf("     r[0|1]       : remove played files from module list\n");
		printf("     o[0|1]       : don't scramble module list order\n");
		printf("     l[0|1]       : loop modules\n");
		printf("-v : sound settings\n");
		printf("     a{0..800}    : set amplification\n");
		printf("     v{0..100}    : set volume\n");
		printf("     b{-100..100} : set balance\n");
		printf("     p{-100..100} : set panning\n");
		printf("     r{-100..100} : set reverb\n");
		printf("     c{-100..100} : set chorus\n");
		printf("     s{0|1}       : set surround on/off\n");
		printf("     f{0..2}      : set filter (0=off, 1=AOI, 2=FOI)\n");
		printf("-s : device settings\n");
		printf("     p<name>      : use specific player device\n");
		printf("     s<name>      : use specific sampler device\n");
		printf("     w<name>      : use specific wavetable device\n");
		printf("     r{0..64000}  : sample at specific rate\n");
		printf("     8            : play/sample/mix as 8bit\n");
		printf("     m            : play/sample/mix mono\n");
		printf("-p                : quit when playlist is empty\n");
		printf("-d : force display driver\n");
		printf("     curses       : ncurses driver\n");
		printf("     x11          : x11 driver\n");
		printf("     vcsa         : vcsa/fb linux console driver\n");
		printf("     sdl          : SDL video driverr\n");
		printf("\nExample : ocp -fl0,r1 -vp75,f2 -spdevpdisk -sr48000 ftstar.xm\n");
		printf("          (for nice HD rendering of modules)\n");
		return errHelpPrinted;
	}
	return errOk;
}


extern char compiledate[], compiletime[]/*, compiledby[]*/;

static int init_modules(int argc, char *argv[])
{
	int ret;
	int i;

	if ((ret=cmdhlp()))
		return ret;

	if (!geteuid())
		if (getuid())
		{
			fprintf(stderr, "Changing user to non-root\n");
			if (seteuid(getuid()))
			{
				perror("seteuid()");
				return errGen;
			}
		}
	if (!getegid())
		if (getgid())
		{
			fprintf(stderr, "Changing group to non-root\n");
			if (setegid(getgid()))
			{
				perror("setegid()");
				return errGen;
			}
		}

	lnkInit();

	fprintf(stderr, "linking default objects...\n");

	cfConfigSec="defaultconfig";

	{
		int epoch = cfGetProfileInt("version", "epoch", 0, 10);
		if (epoch <= 20060815)
		{
			char temp[1024];

			fprintf(stderr, "ocp.ini update (0.1.10) adds devpALSA to [sound] playerdevices=....\n");
			snprintf(temp, sizeof(temp), "devpALSA %s", cfGetProfileString("sound", "playerdevices", ""));
			cfSetProfileString("sound", "playerdevices", temp);

			fprintf(stderr, "ocp.ini update (0.1.10) adds [sound] digitalcd=on\n");
			cfSetProfileBool("sound", "digitalcd", 1);

			fprintf(stderr, "ocp.ini update (0.1.10) adds AY to [fileselector] modextensions=....\n");
			snprintf(temp, sizeof(temp), "%s AY", cfGetProfileString("fileselector", "modextensions", ""));
			cfSetProfileString("fileselector", "modextensions", temp);

			fprintf(stderr, "ocp.ini update (0.1.10) adds [devpALSA]\n");
			cfSetProfileString("devpALSA", "link", "devpalsa");
			cfSetProfileInt("devpALSA", "keep", 1, 10);

			fprintf(stderr, "ocp.ini update (0.1.10) adds [filetype 37]\n");
			cfSetProfileInt("filetype 37", "color", 6, 10);
			cfSetProfileString("filetype 37", "name", "AY");
			cfSetProfileString("filetype 37", "interface", "plOpenCP");
			cfSetProfileString("filetype 37", "pllink", "playay");
			cfSetProfileString("filetype 37", "player", "ayPlayer");
		}

		if (epoch <= 20070712)
		{
			char temp[1024];
			fprintf(stderr, "ocp.ini update (0.1.13/0.1.14) adds devpCA to [sound] playerdevices=....\n");
			snprintf(temp, sizeof(temp), "devpCA %s", cfGetProfileString("sound", "playerdevices", ""));
			cfSetProfileString("sound", "playerdevices", temp);

			fprintf(stderr, "ocp.ini update (0.1.13/0.1.14) adds [devpCA]\n");
			cfSetProfileString("devpCA", "link", "devpcoreaudio");

			fprintf(stderr, "ocp.ini update (0.1.14) changed [devsOSS] revstereo to off\nn");
			cfSetProfileBool("devsOSS", "revstereo", 0);	

			fprintf(stderr, "ocp.ini update (0.1.14) adds [filetype 38]\n");
			cfSetProfileInt("filetype 38", "color", 6, 10);
			cfSetProfileString("filetype 38", "name", "FLA");
			cfSetProfileString("filetype 38", "interface", "plOpenCP");
			cfSetProfileString("filetype 38", "pllink", "playflac");
			cfSetProfileString("filetype 38", "player", "flacPlayer");
		}

		if (epoch <= 20081117)
		{
			fprintf(stderr, "ocp.ini update (0.1.17) removes [general] autoload=....\n");
			cfRemoveEntry("general", "autoload");

			fprintf(stderr, "ocp.ini update (0.1.16/0.1.17) removes [general] link=....\n");
			cfRemoveEntry("general", "link");
			fprintf(stderr, "ocp.ini update (0.1.16/0.1.17) removes [defaultconfig] link=....\n");
			cfRemoveEntry("defaultconfig", "link");

			fprintf(stderr, "ocp.ini update (0.1.16) renames [x11] framebuffer to autodetect\n");
			cfSetProfileBool("x11", "autodetect", cfGetProfileBool("x11", "framebuffer", 1, 1));
			cfRemoveEntry("x11", "framebuffer");

			fprintf(stderr, "ocp.ini update (0.1.16) adds [x11] font=1\n");
			cfSetProfileInt("x11", "font", cfGetProfileInt("x11", "font", 1, 10), 10);
	
			fprintf(stderr, "ocp.ini update (0.1.16) adds [x11] xvidmode=on\n");
			cfSetProfileBool("x11", "xvidmode", cfGetProfileBool("x11", "xvidmode", 1, 1));
		}

		if (epoch <= 20090208)
		{
			fprintf(stderr, "ocp.ini update (0.1.18) removes [driver] keep=1\n");
			cfRemoveEntry("devpALSA", "keep");
		}

		if (epoch <= 20100515)
		{
			fprintf(stderr, "ocp.ini update (0.1.19) adds [filetype 39]\n");
			cfSetProfileInt("filetype 39", "color", 6, 10);
			cfSetProfileString("filetype 39", "name", "YM");
			cfSetProfileString("filetype 39", "interface", "plOpenCP");
			cfSetProfileString("filetype 39", "pllink", "playym");
			cfSetProfileString("filetype 39", "player", "ymPlayer");
		}

		if (epoch <= 20100515)
		{
			const char *temp;
			char temp1[1024];
			char temp2[1024];

			temp = cfGetProfileString("fileselector", "modextensions", "");

			if (strstr(temp, " YM"))
			{
				snprintf(temp1, sizeof(temp1), "%s", temp);
			} else {
				fprintf(stderr, "ocp.ini update (0.1.19) adds YM to [fileselector] modextensions=....\n");
				snprintf(temp1, sizeof(temp1), "%s YM", temp);
			}

			if (strstr(temp, " OGA"))
			{
				snprintf(temp2, sizeof(temp2), "%s", temp1);
			} else {
				fprintf(stderr, "ocp.ini update (0.1.19) adds OGA to [fileselector] modextensions=....\n");
				snprintf(temp2, sizeof(temp2), "%s OGA", temp1);
			}

			cfSetProfileString("fileselector", "modextensions", temp2);
		}

		if (epoch < 20100517)
		{
			cfSetProfileInt("version", "epoch", 20100517, 10);
			cfStoreConfig();
			if (isatty(2))
			{
				fprintf(stderr,"\n\033[1m\033[31mWARNING, ocp.ini has changed, have tried my best to update it. If OCP failes to start, please try to remove by doing this:\033[0m\nrm -f ~/.ocp/ocp.ini\n\n");
			} else {
				fprintf(stderr,"\nWARNING, ocp.ini has changed, have tried my best to update it. If OCP failes to start, please try to remove by doing this:\nrm -f ~/.ocp/ocp.ini\n\n");
			}
			sleep(5);
		}
	}
	if (cfGetProfileInt("version", "epoch", 0, 10)!=20100517)
	{
		if (isatty(2))
		{
			fprintf(stderr,"\n\033[1m\033[31mWARNING, ocp.ini [version] epoch != 20100515\033[0m\n\n");
		} else {
			fprintf(stderr,"\nWARNING, ocp.ini [version] epoch != 20100515\033[0m\n\n");
		}
		sleep(5);
	}

	cfScreenSec=cfGetProfileString(cfConfigSec, "screensec", "screen");
	cfSoundSec=cfGetProfileString(cfConfigSec, "soundsec", "sound");

	lnkLink(cfGetProfileString2(cfConfigSec, "defaultconfig", "prelink", ""));
	lnkLink(cfGetProfileString("general", "prelink", ""));

	{
		char buffer[PATH_MAX];
		snprintf(buffer, sizeof(buffer), "%sautoload/", cfProgramDir);
		if (lnkLinkDir(buffer)<0)
		{
			fprintf(stderr, "could not autoload directory: %s\n", buffer);
			return -1;
		}
	}

	if (lnkLink(cfGetProfileString("general", "link", ""))<0)
	{
		fprintf(stderr, "could not link default objects!\n");
		return -1;
	}

	if ((lnkLink(cfGetProfileString2(cfConfigSec, "defaultconfig", "link", ""))<0))
	{
		fprintf(stderr, "could not link default objects!\n");
		return -1;
	}

	fprintf(stderr, "running initializers...\n");

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->PreInit)
			if (loadlist[i].info->PreInit()<0)
				return errGen;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->Init)
			if (loadlist[i].info->Init()<0)
				return errGen;

	for (i=0;i<loadlist_n;i++)
		if (loadlist[i].info->LateInit)
			if (loadlist[i].info->LateInit()<0)
				return errGen;

	if (!ocpmain)
	{
		fprintf(stderr, "ERROR - No main specified in libraries\n");
		return errGen;
	}
	if (ocpmain->main(argc, argv)<0)
		return errGen;

	plSetTextMode(255);

	return 0;
}

void done_modules(void)
{
	plCloseAll();
	lnkFree(0);
}

#ifdef GCC_411_RUNTIMECHECK
int failcheck(signed int source, signed int filter)
{
	if ((source>128)&&(filter>0))
		return 1;
	return 0;
}
#endif

static int _bootup(int argc, char *argv[])
{
	int result;
	if (isatty(2))
	{
		fprintf(stderr, "\033[33m\033[1mOpen Cubic Player for Unix \033[32mv" VERSION "\033[33m, compiled on %s, %s\n", compiledate, compiletime);
		fprintf(stderr, "\033[31m\033[22mPorted to \033[1m\033[32mUnix \033[31m\033[22mby \033[1mStian Skjelstad\033[0m\n");
	} else {
		fprintf(stderr, "Open Cubic Player for Unix v" VERSION ", compiled on %s, %s\n", compiledate, compiletime);
		fprintf(stderr, "Ported to Unix by Stian Skjelstad\n");
	}

#ifdef GCC_411_RUNTIMECHECK
	fprintf(stderr, "Checking for gcc known 4.1.1 fault - ");
	{
		int j;
		for (j=0;j<256;j++)
		{
			signed char j2=(signed char)j;
			signed int j3=j2;
			if (failcheck(j, j3))
			{
				fprintf(stderr, "failed\nTry to remove any -O flag or to add -fwrapv to CFLAGS and CXXFLAGS and recompile\n");
				return 0;
			}
		}
	}
	fprintf(stderr, "pass\n");
#endif

	if (cfGetConfig(argc, argv))
		return -1;

	result=init_modules(argc, argv);
	if (result)
		if (result!=errHelpPrinted)
			fprintf(stderr, "%s\n", errGetLongString(result));

    	done_modules();

	cfCloseConfig();

	return 0;
}

struct mainstruct bootup = { _bootup };
