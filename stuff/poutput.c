/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Routines for screen output
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
 *  -kb980717   Tammo Hinrichs <kb@nwn.de>
 *    -did a LOT to avoid redundant mode switching (really needed with
 *     today's monitors)
 *    -added color look-up table for redefining the palette
 *    -added MDA output mode for all Hercules enthusiasts out there
 *     (really rocks in Windows 95 background :)
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    -faked in a LFB-mode (required some other changes)
 *  -doj20020418 Dirk Jagdmann <doj@cubic.org>
 *    -added screenshot routines
 *  -ss040613   Stian Skjelstad <stian@nixi.no>
 *    -more or less killed the entire file due to the unix implementation, but
 *     the generic functions is still kept in this file.
 */

#include "config.h"
#include <string.h>
#include "types.h"
#include "poutput.h"
#include "pfonts.h"
#include "imsrtns.h"

unsigned char plpalette[256];
int plScrLineBytes;
int plScrLines;

void make_title(char *part)
{
	uint16_t sbuf[CONSOLE_MAX_X];
	char *verstr="opencp v" VERSION;

	fillstr(sbuf, 0, 0x30, 0, CONSOLE_MAX_X);
	writestring(sbuf, 2, 0x30, verstr, strlen(verstr));
	if (plScrWidth<100)
		writestring(sbuf, plScrWidth-58, 0x30, part, strlen(part));
	else
		writestring(sbuf, (plScrWidth-strlen(part))/2, 0x30, part, strlen(part));
	writestring(sbuf, plScrWidth-28, 0x30, "(c) '94-'10 Stian Skjelstad", 27);
	displaystrattr(0, 0, sbuf, plScrWidth);
}

char *convnum(unsigned long num, char *buf, unsigned char radix, unsigned short len, char clip0)
{
	int i;
	for (i=0; i<len; i++)
	{
		buf[len-1-i]="0123456789ABCDEF"[num%radix];
		num/=radix;
	}
	buf[len]=0;
	if (clip0)
		for (i=0; i<(len-1); i++)
		{
			if (buf[i]!='0')
				break;
			buf[i]=' ';
		}
	return buf;
}

void writenum(uint16_t *buf, unsigned short ofs, unsigned char attr, unsigned long num, unsigned char radix, unsigned short len, char clip0)
{
	char convbuf[20];
	uint16_t *p=buf+ofs;
	char *cp=convbuf+len;
	int i;
	for (i=0; i<len; i++)
	{
		*--cp="0123456789ABCDEF"[num%radix];
		num/=radix;
	}
	for (i=0; i<len; i++)
	{
 		if (clip0&&(convbuf[i]=='0')&&(i!=(len-1)))
 		{
			*p++=' '|(attr<<8);
			cp++;
		} else {
			*p++=(*cp++)|(attr<<8);
			clip0=0;
		}
	}
}

void writestring(uint16_t *buf, unsigned short ofs, unsigned char attr, const char *str, unsigned short len)
{
	uint16_t *p=buf+ofs;
	int i;
	for (i=0; i<len; i++)
	{
		*p++=(*((unsigned char *)(str)))|(attr<<8);
		if (*str)
			str++;
	}
}

void writestringattr(uint16_t *buf, unsigned short ofs, const uint16_t *str, unsigned short len)
{
	memcpyb(buf+ofs, (void *)str, len*2);
}


void markstring(uint16_t *buf, unsigned short ofs, unsigned short len)
{
	int i;
	buf+=ofs;
	for (i=0; i<len; i++)
		*buf++^=0x8000;
}

void fillstr(uint16_t *buf, const unsigned short ofs, const unsigned char chr, const unsigned char attr, unsigned short len)
{
	buf+=ofs;
	while(len)
	{
		*buf=(chr<<8)+attr;
		buf++;
		len--;
	}

}

void generic_gdrawchar8(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	unsigned char *scr;
	short i,j;

	scr=(unsigned char*)(plVidMem+p);

	f=plpalette[f]&0x0f;
	b=plpalette[b]&0x0f;

	for (i=0; i<8; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			*scr++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
	}
}

void generic_gdrawchar8t(unsigned short x, unsigned short y, unsigned char c, unsigned char f)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	char *scr;
	short i,j;

	scr=(char*)(plVidMem+p);

	f=plpalette[f]&0x0f;

	for (i=0; i<8; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			if (bitmap&128)
				*scr=f;
			scr++;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
	}
}

void generic_gdrawchar8p(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{
	unsigned char *cp=plFont88[c];
	unsigned long p=y*plScrLineBytes+x;
	char *scr;
	char *pic;
	short i,j;

      	if (!picp)
	{
		_gdrawchar8(x,y,c,f,0);
		return;
	}

	f=plpalette[f]&0x0f;
	scr=(char*)(plVidMem+p);
	pic=(char*)picp+p;

	for (i=0; i<8; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			if (bitmap&128)
				*scr=f;
			else
				*scr=*pic;
			scr++;
			pic++;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
		pic+=plScrLineBytes-8;
	}
}

void generic_gdrawstr(unsigned short y, unsigned short x, const char *str, unsigned short len, unsigned char f, unsigned char b)
{
	unsigned long p=16*y*plScrLineBytes+x*8;
	unsigned char *sp;
	short i,j,k;

	sp=(unsigned char*)plVidMem+p;

	f=plpalette[f]&0x0f;
	b=plpalette[b]&0x0f;
	for (i=0; i<16; i++)
	{
		const unsigned char *s=(unsigned char *)str;
		for (k=0; k<len; k++)
		{
			unsigned char bitmap=plFont816[*s][i];
			for (j=0; j<8; j++)
			{
				*sp++=(bitmap&128)?f:b;
				bitmap<<=1;
			}
			if (*s)
				s++;
		}
		sp+=plScrLineBytes-8*len;
	}
}

void generic_gdrawcharp(unsigned short x, unsigned short y, unsigned char c, unsigned char f, void *picp)
{

	unsigned char *cp=plFont816[c];
	unsigned long p=y*plScrLineBytes+x;
	char *pic=(char*)picp+p;
	char *scr;
	short i,j;

      	if (!picp)
	{
		_gdrawchar(x,y,c,f,0);
		return;
	}

	scr=(char*)(plVidMem+p);

	f=plpalette[f]&0x0f;

	for (i=0; i<16; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			if (bitmap&128)
				*scr=f;
			else
				*scr=*pic;
			scr++;
			pic++;
			bitmap<<=1;
		}
		pic+=plScrLineBytes-8;
		scr+=plScrLineBytes-8;
	}
}

void generic_gdrawchar(unsigned short x, unsigned short y, unsigned char c, unsigned char f, unsigned char b)
{
	unsigned char *cp=plFont816[c];
	unsigned long p=y*plScrLineBytes+x;
	char *scr;
	short i,j;

	f=plpalette[f]&0x0f;
	b=plpalette[b]&0x0f;
	scr=(char*)(plVidMem+p);

	for (i=0; i<16; i++)
	{
		unsigned char bitmap=*cp++;
		for (j=0; j<8; j++)
		{
			*scr++=(bitmap&128)?f:b;
			bitmap<<=1;
		}
		scr+=plScrLineBytes-8;
	}
}

void generic_gupdatestr(unsigned short y, unsigned short x, const uint16_t *str, unsigned short len, uint16_t *old)
{
	unsigned long p=16*y*plScrLineBytes+x*8;
	unsigned char *sp;
	short i,j,k;

	sp=(unsigned char*)plVidMem+p;

	for (k=0; k<len; k++, str++, old++)
		if (*str!=*old)
		{
			uint8_t a = (*str)>>8;
			unsigned char *bitmap0=plFont816[(*str)&0xff];
			unsigned char f=plpalette[a]&0x0F;
			unsigned char b=plpalette[a]>>4;
			*old=*str;

			for (i=0; i<16; i++)
			{
				unsigned char bitmap=bitmap0[i];
				for (j=0; j<8; j++)
				{
					*sp++=(bitmap&128)?f:b;
					bitmap<<=1;
				}
				sp+=plScrLineBytes-8;
			}
			sp-=16*plScrLineBytes-8;
		} else
			sp+=8;
}
