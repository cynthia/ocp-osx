/* OpenCP Module Player
 * copyright (c) '07-'10 Stian Skjelstad <stian@nixia.no>
 *
 * FLACPlay file type detection routines for the fileselector
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
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"

static struct mdbreadinforegstruct flacReadInfoReg;

static int flacReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	const uint8_t *mybuf;
	size_t mylen;

	if (len<4)
		return 0;

	if (memcmp(buf, "fLaC", 4))
		return 0;
	m->modtype=mtFLAC;

	mybuf = (const uint8_t *)buf + 4;
	mylen = len - 4;

	while (mylen>=4)
	{
		uint8_t BLOCK_TYPE;
		uint_least32_t length;
	
		BLOCK_TYPE = *mybuf++;
		length = (mybuf[0]<<16) | (mybuf[1]<<8) | (mybuf[2]);
		mybuf+=3;
		mylen-=4;

		if (length>mylen)
			break; /* chunk goes outside the range we got */

		switch (BLOCK_TYPE&0x7f)
		{
			case (0x00): /* STREAMINFO */
				if (length>=18)
				{
					uint_least64_t l;
					uint_least32_t rate;
//					fprintf(stderr, "min block size = %d\n", (mybuf[0]<<8) | mybuf[1]);
//					fprintf(stderr, "max block size = %d\n", (mybuf[2]<<8) | mybuf[3]);
//					fprintf(stderr, "min frame size = %d\n", (mybuf[4]<<16) | (mybuf[5]<<8) | mybuf[6]);
//					fprintf(stderr, "max frame size = %d\n", (mybuf[7]<<16) | (mybuf[8]<<8) | mybuf[9]);
//					fprintf(stderr, "sample rate = %d\n", (mybuf[10]<<12) | (mybuf[11]<<4) | mybuf[12]>>4);
//					fprintf(stderr, "channels = %d\n", ((mybuf[12]>>1)&0x07) + 1);
//					fprintf(stderr, "bits per sample = %d\n", (((mybuf[12]<<5)&0x10)|mybuf[13]>>4)+1);
					l = ((((uint_least64_t)mybuf[13])<<32)&0xf00000000ll) | (mybuf[14]<<24) | (mybuf[15]<<16) | (mybuf[16]<<8) | mybuf[17];
					rate = (mybuf[10]<<12) | (mybuf[11]<<4) | mybuf[12]>>4;
//					fprintf(stderr, "length = %lld\n", l);
					m->channels = (((mybuf[12]>>1)&0x07)+1);
					m->playtime = l/rate;
				}
				break;
			case (0x01): /* PADDING */
				break;
			case (0x02): /* APPLICATION */
				break;
			case (0x03): /* SEEKTABLE */
				break;
			case (0x04): /* VORBIS_COMMENT */
				{
					const uint8_t *mymybuf = mybuf;
					uint32_t mymylen = length;

					uint32_t l;
					uint32_t num, n;
					/*int i;*/

					int title = 0;
					int artist = 0;
					int genre = 0;
					int album = 0;

					if (mymylen<4)
						break;
					l = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
					mymylen-=4;
					mymybuf+=4;

					if (mymylen<l)
						break;

//					fprintf(stderr, "VENDORSTRING=");
//					for (i=0;i<l;i++)
//						fprintf(stderr, "%c", mymybuf[i]);
//					fprintf(stderr, "\n");

					mymylen-=l;
					mymybuf+=l;

					if (mymylen<4)
						break;
					num = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
					mymylen-=4;
					mymybuf+=4;

					for (n=0;n<num;n++)
					{
						if (mymylen<4)
							break;
						l = (mymybuf[0]) | (mymybuf[1]<<8) | (mymybuf[2]<<16) | (mymybuf[3]<<24);
						mymylen-=4;
						mymybuf+=4;
	
						if (mymylen<l)
							break;

						if (l>=7)
						{
							if ((!artist)&&(!strncasecmp((char *)mymybuf, "artist=", 7)))
							{
								unsigned int myl=l-7;
								if (myl>sizeof(m->composer))
									myl=sizeof(m->composer);
								strncpy(m->composer, (char *)mymybuf+7, myl);
								if (myl!=sizeof(m->composer))
									m->composer[myl]=0;
								artist=1;
							}
						}
						if (l>=6)
						{
							if ((!title)&&(!strncasecmp((char *)mymybuf, "title=", 6)))
							{
								unsigned int myl=l-6;
								if (myl>sizeof(m->modname))
									myl=sizeof(m->modname);
								strncpy(m->modname, (char *)mymybuf+6, myl);
								if (myl!=sizeof(m->modname))
									m->modname[myl]=0;
								title=1;
							}
							if ((!album)&&(!strncasecmp((char *)mymybuf, "album=", 6)))
							{
								unsigned int myl=l-6;
								if (myl>sizeof(m->comment))
									myl=sizeof(m->comment);
								strncpy(m->comment, (char *)mymybuf+6, myl);
								if (myl!=sizeof(m->comment))
									m->comment[myl]=0;
								album=1;
							}
							if ((!genre)&&(!strncasecmp((char *)mymybuf, "genre=", 6)))
							{
								unsigned int myl=l-6;
								if (myl>sizeof(m->style))
									myl=sizeof(m->style);
								strncpy(m->style, (char *)mymybuf+6, myl);
								if (myl!=sizeof(m->style))
									m->style[myl]=0;
								genre=1;
							}
						}

/*						fprintf(stderr, "COMMENT(%d/%d)=", n+1, num);
						for (i=0;i<l;i++)
							fprintf(stderr, "%c(%d)", mymybuf[i], mymybuf[i]);
						fprintf(stderr, "\n");*/

						mymylen-=l;
						mymybuf+=l;
					}
				}
				break;
			case (0x05): /* CUESHEET */
				break;
			case (0x06): /* PICTURE */
				break;
		}

		if (BLOCK_TYPE&0x80)
			break; /* This terminates the BLOCK list */

		mylen-=length;
		mybuf+=length;
	}
	return 1;
}


static int flacReadInfo(struct moduleinfostruct *m, FILE *fp, const char *mem, size_t len)
{
	flacReadMemInfo(m, mem, len);
	return 0;
}

static void flacEvent(int event)
{
	switch (event)
	{
		case mdbEvInit:
		{
			fsRegisterExt("FLA");
			fsRegisterExt("FLAC");
			fsRegisterExt("FLC");
		}
	}
}

static void __attribute__((constructor))init(void)
{
	mdbRegisterReadInfo(&flacReadInfoReg);
}

static void __attribute__((destructor))done(void)
{
	mdbUnregisterReadInfo(&flacReadInfoReg);
}


static struct mdbreadinforegstruct flacReadInfoReg = {flacReadMemInfo, flacReadInfo, flacEvent MDBREADINFOREGSTRUCT_TAIL};
char *dllinfo = "";
struct linkinfostruct dllextinfo = {"flacptype", "OpenCP FLAC Detection (c) 2007-09 Stian Skjelstad", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
