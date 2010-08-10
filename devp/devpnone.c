/** OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * NoSound Player device
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
 *    -added _dllinfo record
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "stuff/timer.h"
#include "stuff/imsrtns.h"

static uint32_t starttime;
static uint32_t starttime2;
static uint32_t wrap;

static void short_circuit(void)
{
}

static unsigned long buflen;
static unsigned long bufrate;

extern struct sounddevice plrNone;

static int getpos(void)
{
	/* this makes starttime always be a safe low value */
	uint32_t now = tmGetTimer();

	if ((now-starttime)>wrap)
		starttime+=wrap;
	now-=starttime;

	return imuldiv(now, bufrate, 65536)%buflen;
}

static void advance(unsigned int i)
{
}

static uint32_t gettimer(void)
{
	return tmGetTimer()-starttime2;
}

static void qpSetOptions(uint32_t rate, int opt)
{
	int stereo=!!(opt&PLR_STEREO);
	int bit16=!!(opt&PLR_16BIT);

	if (rate<5000)
		rate=5000;

	if (rate>48000)
		rate=48000;

	bufrate=rate<<(stereo+bit16); /* !!!!!!!!!! */

	plrRate=rate;
	plrOpt=opt;
/*	tmSetNewRate(plrRate);*/
}

static void *thebuf;

static int qpPlay(void **buf, unsigned int *len)
{
	if (!(thebuf=*buf=malloc(sizeof(unsigned char)*(*len))))
		return 0;
	memsetd(*buf, (plrOpt&PLR_SIGNEDOUT)?0:(plrOpt&PLR_16BIT)?0x80008000:0x80808080, *len>>2);

	buflen=*len;

	plrGetBufPos=getpos;
	plrGetPlayPos=getpos;
	plrAdvanceTo=advance;
	plrGetTimer=gettimer;

	starttime = starttime2 = tmGetTimer();

	wrap = buflen * bufrate;

	tmInit(short_circuit, plrRate);

	return 1;
}

static void qpStop(void)
{
	free(thebuf);
	tmClose();
}

static int qpInit(const struct deviceinfo *c)
{
	plrSetOptions=qpSetOptions;
	plrPlay=qpPlay;
	plrStop=qpStop;
	return 1;
}

static void qpClose(void)
{
	plrPlay=0;
}

static int qpDetect(struct deviceinfo *card)
{
	card->devtype=&plrNone;
	card->port=-1;
	card->port2=-1;
/*	card->irq=-1;
	card->irq2=-1;
	card->dma=-1;
	card->dma2=-1;*/
	card->subtype=-1;
	card->mem=0;
	card->chan=2;

	return 1;
}

struct sounddevice plrNone={SS_PLAYER, 0, "Super High Quality Quiet Player", qpDetect, qpInit, qpClose, 0};

char *dllinfo="driver plrNone";
struct linkinfostruct dllextinfo = {"devpnone", "OpenCP Player Device: None (c) 1994-09 Niklas Beisert, Tammo Hinrichs", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
