/* OpenCP Module Player
 * copyright (c) '05-'10 Stian Skjelstad <stian@nixia.no>
 *
 * OPLPlay interface routines
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
 */

#include "config.h"
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
extern "C"
{
#include "filesel/pfilesel.h"
#include "filesel/mdb.h"
#include "dev/player.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "stuff/compat.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/timer.h"
#include "dev/deviplay.h"
#include "cpiface/cpiface.h"
}
#include "stsoundlib/StSoundLibrary.h"
#include "ymplay.h"

static time_t starttime, pausetime;
static uint32_t pausefadestart;
static uint8_t pausefaderelspeed;
static int8_t pausefadedirect;

static void startpausefade(void)
{
	if (plPause)
		starttime=starttime+dos_clock()-pausetime;

	if (pausefadedirect)
	{
		if (pausefadedirect<0)
			plPause=1;
		pausefadestart=2*dos_clock()-DOS_CLK_TCK-pausefadestart;
	} else
		pausefadestart=dos_clock();

	if (plPause)
	{
		plChanChanged=1;
		ymPause(plPause=0);
		pausefadedirect=1;
	} else
		pausefadedirect=-1;
}

static void dopausefade(void)
{
	int16_t i;
	if (pausefadedirect>0)
	{
		i=((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i<0)
			i=0;
		if (i>=64)
		{
			i=64;
			pausefadedirect=0;
		}
	} else {
		i=64-((int32_t)dos_clock()-pausefadestart)*64/DOS_CLK_TCK;
		if (i>=64)
			i=64;
		if (i<=0)
		{
			i=0;
			pausefadedirect=0;
			pausetime=dos_clock();
			ymPause(plPause=1);
			plChanChanged=1;
			ymSetSpeed(globalmcpspeed);
			return;
		}
	}
	pausefaderelspeed=i;
	ymSetSpeed(globalmcpspeed*i/64);
}

static char convnote(long freq)
{
	if (!freq) return 0xff;

	float frfac=(float)freq/220.0;

	float nte=12*(log(frfac)/log(2))+48;

	if (nte<0 || nte>127) nte=0xff;
	return (char)nte;
}
/*
static void logvolbar(int &l, int &r)
{
	if (l>32)
		l=32+((l-32)>>1);
	if (l>48)
		l=48+((l-48)>>1);
	if (l>56)
		l=56+((l-56)>>1);
	if (l>64)
		l=64;
	if (r>32)
		r=32+((r-32)>>1);
	if (r>48)
		r=48+((r-48)>>1);
	if (r>56)
		r=56+((r-56)>>1);
	if (r>64)
		r=64;
}
*/
static void drawvolbar(uint16_t *buf, int l, int r, unsigned char st)
{
	/*logvolbar(l, r);
	l=(l+4)>>3;
	r=(r+4)>>3;*/
	l=l>>1;
	r=r>>1;

	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 8-l, 0x08, "\376\376\376\376\376\376\376\376", l);
		writestring(buf, 9, 0x08, "\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0ffe};
		writestringattr(buf, 8-l, left+8-l, l);
		writestringattr(buf, 9, right, r);
	}
}

static void drawlongvolbar(uint16_t *buf, int l, int r, unsigned char st)
{
/*	logvolbar(l, r);
	l=(l+2)>>2;
	r=(r+2)>>2;*/
	if (plPause)
		l=r=0;
	if (st)
	{
		writestring(buf, 16-l, 0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", l);
		writestring(buf, 17,   0x08, "\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376\376", r);
	} else {
		uint16_t left[] =  {0x0ffe, 0x0ffe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe};
		uint16_t right[] = {0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x01fe, 0x09fe, 0x09fe, 0x09fe, 0x09fe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0bfe, 0x0ffe, 0x0ffe};
		writestringattr(buf, 16-l, left+16-l, l);
		writestringattr(buf, 17,   right, r);
	}
}

static void ymDrawGStrings(uint16_t (*buf)[CONSOLE_MAX_X])
{
      	long tim;
	ymMusicInfo_t globinfo;

	ymMusicGetInfo(pMusic, &globinfo);

/*       	mcpDrawGStrings(buf);*/

	if (plPause)
		tim=(pausetime-starttime)/DOS_CLK_TCK;
	else
		tim=(dos_clock()-starttime)/DOS_CLK_TCK;

      	if (plScrWidth<128)
	{
		memset(buf[0]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[1]+80, 0, (plScrWidth-80)*sizeof(uint16_t));
		memset(buf[2]+80, 0, (plScrWidth-80)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, " vol: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa ", 15);
		writestring(buf[0], 15, 0x09, " srnd: \xfa  pan: l\xfa\xfa\xfam\xfa\xfa\xfar  bal: l\xfa\xfa\xfam\xfa\xfa\xfar ", 41);
		writestring(buf[0], 56, 0x09, "            pitch: ---% ", 24);
		writestring(buf[0], 6, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+4)>>3);
		writestring(buf[0], 22, 0x0F, srnd?"x":"o", 1);
		if (((pan+70)>>4)==4)
			writestring(buf[0], 34, 0x0F, "m", 1);
		else {
			writestring(buf[0], 30+((pan+70)>>4), 0x0F, "r", 1);
			writestring(buf[0], 38-((pan+70)>>4), 0x0F, "l", 1);
		}
		writestring(buf[0], 46+((bal+70)>>4), 0x0F, "I", 1);
/*		_writenum(buf[0], 62, 0x0F, speed*100/256, 10, 3);*/
		_writenum(buf[0], 75, 0x0F, ymbufrate*100/65536, 10, 3);

		writestring(buf[1],  0, 0x09, " author: .......................... comment: ...................... type: .....",80);

		if (globinfo.pSongAuthor)
			if (globinfo.pSongAuthor[0])
				writestring(buf[1], 9, 0x0F, globinfo.pSongAuthor, 26);
		if (globinfo.pSongComment)
			if (globinfo.pSongComment[0])
				writestring(buf[1], 45, 0x0F, globinfo.pSongComment, 22);
		if (globinfo.pSongType)
			writestring(buf[1], 74, 0x0F, globinfo.pSongType, 6);

		writestring(buf[2],  0, 0x09, "  title: ......................................... pos: ...../..... time: ..:..", 80);
		if (globinfo.pSongName)
			if (globinfo.pSongName[0])
				writestring(buf[2], 9, 0x0F, globinfo.pSongName, 41);
		_writenum(buf[2], 56, 0x0F, globinfo.currentFrame, 10, 5);
		_writenum(buf[2], 62, 0x0F, globinfo.nbFrame, 10, 5);

		if (plPause)
			writestring(buf[2], 73, 0x0C, "paused", 6);
		else {
			writenum(buf[2], 74, 0x0F, (tim/60)%60, 10, 2, 1);
			writestring(buf[2], 76, 0x0F, ":", 1);
			writenum(buf[2], 77, 0x0F, tim%60, 10, 2, 0);
		}
	} else {
		memset(buf[0]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[1]+128, 0, (plScrWidth-128)*sizeof(uint16_t));
		memset(buf[2]+128, 0, (plScrWidth-128)*sizeof(uint16_t));

		writestring(buf[0], 0, 0x09, "    volume: \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa  ", 30);
		writestring(buf[0], 30, 0x09, " surround: \xfa   panning: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar   balance: l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar  ", 72);
		writestring(buf[0], 102, 0x09,  "               pitch: ---%    ", 30);
		writestring(buf[0], 12, 0x0F, "\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe\xfe", (vol+2)>>2);
		writestring(buf[0], 41, 0x0F, srnd?"x":"o", 1);
		if (((pan+68)>>3)==8)
			writestring(buf[0], 62, 0x0F, "m", 1);
		else {
			writestring(buf[0], 54+((pan+68)>>3), 0x0F, "r", 1);
			writestring(buf[0], 70-((pan+68)>>3), 0x0F, "l", 1);
		}
		writestring(buf[0], 83+((bal+68)>>3), 0x0F, "I", 1);
	/*	_writenum(buf[0], 110, 0x0F, speed*100/256, 10, 3);*/
		_writenum(buf[0], 124, 0x0F, ymbufrate*100/65536, 10, 3);

		writestring(buf[1],  0, 0x09, " author: ......................................................... comment: ........................................ type: .....",128);

		if (globinfo.pSongAuthor)
			if (globinfo.pSongAuthor[0])
				writestring(buf[1], 9, 0x0F, globinfo.pSongAuthor, 57);
		if (globinfo.pSongComment)
			if (globinfo.pSongComment[0])
				writestring(buf[1], 76, 0x0F, globinfo.pSongComment, 40);
		if (globinfo.pSongType)
			writestring(buf[1], 123, 0x0F, globinfo.pSongType, 6);

		writestring(buf[2],  0, 0x09, "  title: .......................................................................................... Pos: ...../..... time: ..:..", 128);

		if (globinfo.pSongName)
			if (globinfo.pSongName[0])
				writestring(buf[2], 9, 0x0F, globinfo.pSongName, 90);
		_writenum(buf[2], 105, 0x0F, globinfo.currentFrame, 10, 5);
		_writenum(buf[2], 111, 0x0F, globinfo.nbFrame, 10, 5);
		if (plPause)
			writestring(buf[2], 122, 0x0C, "paused", 6);
		else {
			writenum(buf[2], 123, 0x0F, (tim/60)%60, 10, 2, 1);
			writestring(buf[2], 125, 0x0F, ":", 1);
			writenum(buf[2], 126, 0x0F, tim%60, 10, 2, 0);
		}
	}
}

static void drawchannel(uint16_t *buf, int len, int i)
{
	int voll, volr;
	int env = 0;
#warning TODO
        unsigned char st=plMuteCh[i]; /* TODO */

        unsigned char tcol=st?0x08:0x0F;
        unsigned char tcold=st?0x08:0x07;

	unsigned char channel_mode;
	int freq;
	
	const char *waves4 []= {"t+s ", "nois", "tone", "    ", "gene",
	"\\___",  /* 0 0 0 0 */
	"\\___",  /* 0 0 0 1 */
	"\\___",  /* 0 0 1 0 */
	"\\___",  /* 0 0 1 1 */
	"/|__",   /* 0 1 0 0 */
	"/|__",   /* 0 1 0 1 */
	"/|__",   /* 0 1 1 0 */
	"/|__",   /* 0 1 1 1 */
	"\\|\\|", /* 1 0 0 0 */
	"\\___",  /* 1 0 0 1 */
	"\\/\\/", /* 1 0 1 0 */
	"\\|--",  /* 1 0 1 1 */
	"/|/|",   /* 1 1 0 0 */
	"/---",   /* 1 1 0 1 */
	"\\/\\/", /* 1 1 1 0 */
	"/|__"    /* 1 1 1 1 */
	};
	const char *waves16[]= {"tone+noise      ", "noise           ", "tone            ", "                ", "noise generator ",
	"env:falling     ", /* 0 0 0 0 */
	"env:falling     ", /* 0 0 0 1 */
	"env:falling     ", /* 0 0 1 0 */
	"env:falling     ", /* 0 0 1 1 */
	"env:pos spike   ", /* 0 1 0 0 */
	"env:pos spike   ", /* 0 1 0 1 */
	"env:pos spike   ", /* 0 1 1 0 */
	"env:pos spike   ", /* 0 1 1 1 */
	"env:falling saw ", /* 1 0 0 0 */
	"env:falling     ", /* 1 0 0 1 */
	"env:saw         ", /* 1 0 1 0 */
	"env:neg spike   ", /* 1 0 1 1 */
	"env:rising saw  ", /* 1 1 0 0 */
	"env:rising      ", /* 1 1 0 1 */
	"env:saw         ", /* 1 1 1 0 */
	"env:spike       "  /* 1 1 1 1 */
	};
/*	const char *waves4 []= {"sine", "half", "2x  ", "saw "};
	const char *waves16[]= {"sine curves     ", "half sine curves", "positiv sines   ", "sawtooth        "};*/
	struct channel_info_t *info = ymRegisters();

	switch (i)
	{
		case 0:
			freq = info->frequency_a;
			voll = volr = info->level_a & 15;
			if (info->level_a & 16)
				env = 1;
			channel_mode = (info->mixer_control & 1) | ((info->mixer_control >> 2)&2);
			if (channel_mode==3)
				voll = volr = 0;
			break;
		case 1:
			freq = info->frequency_b;
			voll = volr = info->level_b & 15;
			if (info->level_b & 16)
				env = 1;
			channel_mode = ((info->mixer_control>>1) & 1) | ((info->mixer_control >> 3)&2);
			if (channel_mode==3)
				voll = volr = 0;
			break;
		case 2:
			freq = info->frequency_c;
			voll = volr = info->level_c & 15;
			if (info->level_c & 16)
				env = 1;
			channel_mode = ((info->mixer_control>>2) & 1) | ((info->mixer_control >> 4)&2);
			if (channel_mode==3)
				vol = volr = 0;
			break;
		case 3:
			freq = info->frequency_noise;
			voll = volr = 0;
			channel_mode = 4;
			break;
		case 4:
			freq = info->frequency_envelope;
			voll = volr = 0;
			channel_mode = 5+info->envelope_shape;
			break;
	}

	switch (len)
	{
		case 36:
			writestring(buf, 0, tcold, " ---- --- -- - -- \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 36);
			break;
		case 62:
			writestring(buf, 0, tcold, " ---------------- ---- --- --- --- -------  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 62);
			break;
		case 128:
			writestring(buf, 0, tcold, "                   \263        \263       \263       \263                \263               \263   \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372\372\372\372\372\372\372\372\372", 128);
			break;                                                                      
		case 76:
			writestring(buf, 0, tcold, "                  \263      \263     \263     \263     \263             \263 \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372", 76);
			break;
		case 44:
			writestring(buf, 0, tcold, " ---- ---- --- -- --- --  \372\372\372\372\372\372\372\372 \372\372\372\372\372\372\372\372 ", 44);

		break;
	}

/*	oplpGetChanInfo(i,ci);*/

/*	if (!ci.vol)
		return;*/
	uint8_t nte=convnote(freq);
	char nchar[4];

	if (nte<0xFF)
	{
		nchar[0]="CCDDEFFGGAAB"[nte%12];
		nchar[1]="-#-#--#-#-#-"[nte%12];
		nchar[2]="0123456789ABCDEFGHIJKLMN"[nte/12];
		nchar[3]=0;
	} else
		strcpy(nchar,"   ");

	switch(len)
	{
		case 36:
			writestring(buf+1, 0, tcol, waves4[channel_mode], 4);
			writestring(buf+6, 0, tcol, nchar, 3);
			if (env)
				writestring(buf+10, 0, tcol, "En", 2);
/*#			writenum(buf+10, 0, tcol, ci.pulse>>4, 16, 2, 0);
  #			if (ci.filtenabled && ftype)
  #				writenum(buf+13, 0, tcol, ftype, 16, 1, 0);
  #			if (efx)
  #				writestring(buf+15, 0, tcol, fx2[efx], 2);*/
			drawvolbar(buf+18, voll, volr, st);
			break;
		case 44:
			writestring(buf+1, 0, tcol, waves4[channel_mode], 4);
/*#			writenum(buf+6, 0, tcol, ci.ad, 16, 2, 0);
  #			writenum(buf+8, 0, tcol, ci.sr, 16, 2, 0);*/
			writestring(buf+11, 0, tcol, nchar, 3);
			if (env)
				writestring(buf+15, 0, tcol, "En", 2);
/*#			writenum(buf+15, 0, tcol, ci.pulse>>4, 16, 2, 0);
  #			if (ci.filtenabled && ftype)
  #				writestring(buf+18, 0, tcol, filters3[ftype], 3);
  #			if (efx)
  #				writestring(buf+22, 0, tcol, fx2[efx], 2);*/
			drawvolbar(buf+26, voll, volr, st);
			break;
		case 62:
			writestring(buf+1, 0, tcol, waves16[channel_mode], 16);
/*#			writenum(buf+18, 0, tcol, ci.ad, 16, 2, 0);
  #			writenum(buf+20, 0, tcol, ci.sr, 16, 2, 0);*/
			writestring(buf+23, 0, tcol, nchar, 3);

			if (env)
				writestring(buf+27, 0, tcol, "Env", 3);
/*#			writenum(buf+27, 0, tcol, ci.pulse, 16, 3, 0);
  #			if (ci.filtenabled && ftype)
  #				writestring(buf+31, 0, tcol, filters3[ftype], 3);
  #			if (efx)
  #				writestring(buf+35, 0, tcol, fx7[efx], 7);*/
			drawvolbar(buf+44, voll, volr, st);
			break;
		case 76:
			writestring(buf+1, 0, tcol, waves16[channel_mode], 16);
			writenum(buf+19, 0, tcol, freq, 10, 6, 0);
/*#			writenum(buf+20, 0, tcol, ci.ad, 16, 2, 0);
  #			writenum(buf+22, 0, tcol, ci.sr, 16, 2, 0);*/
			writestring(buf+27, 0, tcol, nchar, 3);
			if (env)
				writestring(buf+33, 0, tcol, "Env", 3);
/*#			writenum(buf+33, 0, tcol, ci.pulse, 16, 3, 0);
  #			if (ci.filtenabled && ftype)
  #				writestring(buf+39, 0, tcol, filters3[ftype], 3);
  #			writestring(buf+45, 0, tcol, fx11[efx], 11);*/
			drawvolbar(buf+59, voll, volr, st);
			break;
		case 128:
			writestring(buf+1, 0, tcol, waves16[channel_mode], 16);
			writenum(buf+21, 0, tcol, freq, 10, 6, 0);
/*#			writenum(buf+22, 0, tcol, ci.ad, 16, 2, 0);
  #			writenum(buf+24, 0, tcol, ci.sr, 16, 2, 0);*/
			writestring(buf+31, 0, tcol, nchar, 3);
			if (env)
				writestring(buf+39, 0, tcol, "Env", 3);
/*#			writenum(buf+39, 0, tcol, ci.pulse, 16, 3, 0);
  #			if (ci.filtenabled && ftype)
  #				writestring(buf+47, 0, tcol, filters12[ftype], 12);
  #			writestring(buf+64, 0, tcol, fx11[efx], 11);*/
			drawlongvolbar(buf+81, voll, volr, st);
			break;
	}			
}

static int ymProcessKey(uint16_t key)
{
        if (mcpSetProcessKey(key))
                return 1;

	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('p', "Start/stop pause with fade");
			cpiKeyHelp('P', "Start/stop pause with fade");
			cpiKeyHelp(KEY_CTRL_P, "Start/stop pause");
			cpiKeyHelp(KEY_CTRL_UP, "Rewind 1 second");
			cpiKeyHelp(KEY_CTRL_LEFT, "Rewind 10 second");
			cpiKeyHelp('<', "Rewind 10 second");
			cpiKeyHelp(KEY_CTRL_DOWN, "Forward 1 second");
			cpiKeyHelp(KEY_CTRL_RIGHT, "Forward 10 second");
			cpiKeyHelp('>', "Forward 10 second");
			cpiKeyHelp(KEY_CTRL_HOME, "Rewind to start");


			if (plrProcessKey)
				plrProcessKey(key);
			return 0;
		case 'p': case 'P':
			startpausefade();
			break;
		case KEY_CTRL_P:
			if (plPause)
				starttime=starttime+dos_clock()-pausetime;
			else
				pausetime=dos_clock();
			plPause=!plPause;
			ymPause(plPause);
			break;
		case KEY_CTRL_UP:
		/* case 0x8D00: //ctrl-up */
			ymSetPos(ymGetPos()-50);
			break;
		case KEY_CTRL_DOWN:
		/* case 0x9100: //ctrl-down */
			ymSetPos(ymGetPos()+50);
			break;
		case '<':
		case KEY_CTRL_LEFT:
		/* case 0x7300: //ctrl-left */
			ymSetPos(ymGetPos()-500);
			break;
		case '>':
		case KEY_CTRL_RIGHT:
		/* case 0x7400: //ctrl-right */
			ymSetPos(ymGetPos()+500);
			break;
		case 0x7700: //ctrl-home TODO keys
			ymSetPos(0);
			break;
		default:
			if (plrProcessKey)
			{
				int ret=plrProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			return 0;
	}
	return 1;
}


static int ymLooped(void)
{
	if (pausefadedirect)
		dopausefade();
	ymSetLoop(fsLoopMods);
	ymIdle();
	if (plrIdle)
		plrIdle();
	return !fsLoopMods&&ymIsLooped();
}

static void ymCloseFile(void)
{
	ymClosePlayer();
}

static void normalize(void)
{
	mcpNormalize(0);
}
static int ymOpenFile(const char *path, struct moduleinfostruct *info, FILE *file)
{
	plIsEnd=ymLooped;
	plProcessKey=ymProcessKey;
	plDrawGStrings=ymDrawGStrings;
	plGetMasterSample=plrGetMasterSample;
	plGetRealMasterVolume=plrGetRealMasterVolume;
	
	if (!ymOpenPlayer(file))
		return -1;
	
	starttime=dos_clock();
	normalize();

	plNLChan=plNPChan=5;
	plUseChannels(drawchannel);
	plSetMute=ymMute;

	return 0;
}

extern "C"
{
  cpifaceplayerstruct ymPlayer = {ymOpenFile, ymCloseFile};
  struct linkinfostruct dllextinfo = {"playym", "OpenCP STYMulator Player (c) 2010 Stian Skjelstad", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
}
