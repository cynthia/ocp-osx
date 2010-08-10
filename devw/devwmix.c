/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Wavetable Device: Software Mixer for sample stream output via devp
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
 *  -kb980525   Tammo Hinrichs <kb@nwn.de>
 *    - restructured volume calculations to avoid those nasty
 *      rounding errors
 *    - changed behaviour on loop change a bit (may cause problems with
 *      some .ULTs but fixes many .ITs instead ;)
 *    - extended volume table to 256 values, thus consuming more memory,
 *      but definitely increasing the output quality ;)
 *    - added _dllinfo record
 *  -ryg990504  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -fixed sum really stupid memory leak
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>

#include <sys/mman.h>

#include "types.h"
#include "boot/plinkman.h"
#include "dev/imsdev.h"
#include "dev/mcp.h"
#include "stuff/poll.h"
#include "dev/mix.h"
#include "dev/player.h"
#include "devwmix.h"
#include "stuff/imsrtns.h"
#include "dwmix.h"
#include "dwmixa.h"
#include "dwmixqa.h"

#ifdef I386_ASM
#include "stuff/pagesize.inc.c"
#endif

#define MIXBUFLEN 4096
#define MAXCHAN 255

extern struct sounddevice mcpMixer;

static struct mixqpostprocregstruct *postprocs;

static int quality;
static int resample;

static int _pause;
static long playsamps;
static long pausesamps;

static struct sampleinfo *samples;
static int samplenum;

static int16_t (*amptab)[256]; /* signedness is not fixed here */
static unsigned long clipmax;
static volatile int clipbusy;
#define BARRIER __asm__ __volatile__ ("":);

static unsigned long amplify;
static unsigned short transform[2][2];
static int volopt;
static unsigned long relpitch;
static int interpolation;
static int restricted;

static unsigned char stereo;
static unsigned char bit16;
static unsigned char signedout;
static unsigned long samprate;
static unsigned char reversestereo;

static int channelnum;
static struct channel *channels;
static int32_t fadedown[2];

static int32_t (*voltabsr)[256];
static int16_t (*voltabsq)[2][256];

static uint8_t (*interpoltabr)[256][2];
static int16_t (*interpoltabq)[32][256][2];
static int16_t (*interpoltabq2)[16][256][4];

static int16_t *scalebuf=0;
static int32_t *buf32;
static uint32_t bufpos;
static uint32_t buflen;
static void *plrbuf;

static void (*playerproc)(void);
static unsigned long tickwidth;
static unsigned long tickplayed;
static unsigned long orgspeed;
static unsigned short relspeed;
static unsigned long newtickwidth;
static unsigned long cmdtimerpos;

static int mastervol;
static int masterbal;
static int masterpan;
static int mastersrnd;
static int masterrvb;

static void calcinterpoltabr(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabr[i][j][1]=(i*(signed char)j)>>4;
			interpoltabr[i][j][0]=(signed char)j-interpoltabr[i][j][1];
		}
}

static void calcinterpoltabq(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<32; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq[0][i][j][1]=(i*(signed char)j)<<3;
			interpoltabq[0][i][j][0]=(((signed char)j)<<8)-interpoltabq[0][i][j][1];
			interpoltabq[1][i][j][1]=(i*j)>>5;
			interpoltabq[1][i][j][0]=j-interpoltabq[1][i][j][1];
		}
	for (i=0; i<16; i++)
		for (j=0; j<256; j++)
		{
			interpoltabq2[0][i][j][0]=((16-i)*(16-i)*(signed char)j)>>1;
			interpoltabq2[0][i][j][2]=(i*i*(signed char)j)>>1;
			interpoltabq2[0][i][j][1]=(((signed char)j)<<8)-interpoltabq2[0][i][j][0]-interpoltabq2[0][i][j][2];
			interpoltabq2[1][i][j][0]=((16-i)*(16-i)*j)>>9;
			interpoltabq2[1][i][j][2]=(i*i*j)>>9;
			interpoltabq2[1][i][j][1]=j-interpoltabq2[1][i][j][0]-interpoltabq2[1][i][j][2];
		}
}

static void calcvoltabsr(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (i=0; i<=512; i++)
		for (j=0; j<256; j++)
			voltabsr[i][j]=(i-256)*(signed char)j;
}

static void calcvoltabsq(void)
	/* used by OpenPlayer */
{
	int i,j;
	for (j=0; j<=512; j++)
	{
		long amp=j-256;
		for (i=0; i<256; i++)
		{
			int v=amp*(signed char)i;
			voltabsq[j][0][i]=(v==0x8000)?0x7FFF:v;
			voltabsq[j][1][i]=(amp*i)>>8;
		}
	}
}

static void calcamptab(signed long amp)
	/* Used by SET
	 *         OpenPlayer
	 */
{
	int i;

	clipbusy++;

	BARRIER

	amp=3*amp/16;

	for (i=0; i<256; i++)
	{
		amptab[0][i]=(amp*i)>>12;
		amptab[1][i]=(amp*i)>>4;
		amptab[2][i]=(amp*(signed char)i)<<4;
	}

	if(amp)
		clipmax=0x07FFF000/amp;
	else
		clipmax=0x07FFF000;

	if (!signedout)
		for (i=0; i<256; i++)
			amptab[0][i]^=0x8000;

	BARRIER

	clipbusy--;
}

static void calcstep(struct channel *c)
	/* Used by calcsteps
	 * 	   SET
	 */
{
	if (!(c->status&MIXRQ_PLAYING))
		return;
	if (c->orgdiv)
		c->step=imuldiv(imuldiv((c->step>=0)?c->orgfrq:-c->orgfrq, c->orgrate, c->orgdiv)<<8, relpitch, samprate);
	else
		c->step=0;
	c->status&=~MIXRQ_INTERPOLATE;
	if (!quality)
	{
		if (interpolation>1)
			c->status|=MIXRQ_INTERPOLATE;
		if (interpolation==1)
			if (abs(c->step)<=(3<<15))
		c->status|=MIXRQ_INTERPOLATE;
	} else {
		if (interpolation>1)
			c->status|=MIXRQ_INTERPOLATEMAX|MIXRQ_INTERPOLATE;
		if (interpolation==1)
		{
			c->status|=MIXRQ_INTERPOLATE;
			c->status&=~MIXRQ_INTERPOLATEMAX;
		}
	}
}

static void calcsteps(void)
	/* Used by SET */
{
	int i;
	for (i=0; i<channelnum; i++)
		calcstep(&channels[i]);
}

static void calcspeed(void)
{
	/* Used by SET
	 *         OpenPlayer
	 */
	if (channelnum)
		newtickwidth=imuldiv(256*256*256, samprate, orgspeed*relspeed);
}

static void transformvol(struct channel *ch)
	/* Used by calcvol
	 *         calcvols
	 *         SET
	 */
{
	int32_t v;

	v=transform[0][0]*ch->orgvol[0]+transform[0][1]*ch->orgvol[1];
	ch->vol[0]=(v>0x10000)?256:(v<-0x10000)?-256:((v+192)>>8);

	v=transform[1][0]*ch->orgvol[0]+transform[1][1]*ch->orgvol[1];
	if (volopt^ch->volopt)
		v=-v;
	ch->vol[1]=(v>0x10000)?256:(v<-0x10000)?-256:((v+192)>>8);

	if (ch->status&MIXRQ_MUTE)
	{
		ch->dstvols[0]=ch->dstvols[1]=0;
		return;
	}
	if (!stereo)
	{
		ch->dstvols[0]=(abs(ch->vol[0])+abs(ch->vol[1])+1)>>1;
		ch->dstvols[1]=0;
	} else if (reversestereo)
	{
		ch->dstvols[0]=ch->vol[1];
		ch->dstvols[1]=ch->vol[0];
	} else {
		ch->dstvols[0]=ch->vol[0];
		ch->dstvols[1]=ch->vol[1];
	}
}

static void calcvol(struct channel *chn)
	/* Used by calcvols
	 *         SET
	 */
{
	chn->orgvol[1]=((int32_t)chn->orgvolx*(0x80L+chn->orgpan))>>8;
	chn->orgvol[0]=((int32_t)chn->orgvolx*(0x80L-chn->orgpan))>>8;
	/* werte: 0-0x100; */
	transformvol(chn);
}


static void calcvols(void)
	/* Used by SET
	 *         OpenPlayer
	 */
{
	int16_t vols[2][2];
	int i;

	vols[0][0]=vols[1][1]=(mastervol*(0x40+masterpan))>>6;
	vols[0][1]=vols[1][0]=(mastervol*(0x40-masterpan))>>6;
	/* werte: 0-0x100 */

	if (masterbal>0)
	{
		vols[0][0]=(vols[0][0]*(0x40-masterbal))>>6;
		vols[0][1]=(vols[0][1]*(0x40-masterbal))>>6;
	} else if (masterbal<0)
	{
		vols[1][0]=(vols[1][0]*(0x40+masterbal))>>6;
		vols[1][1]=(vols[1][1]*(0x40+masterbal))>>6;
	}

	volopt=mastersrnd;
	transform[0][0]=vols[0][0];
	transform[0][1]=vols[0][1];
	transform[1][0]=vols[1][0];
	transform[1][1]=vols[1][1];
	for (i=0; i<channelnum; i++)
		transformvol(&channels[i]);
}

static void fadechanq(int *fade, struct channel *c)
	/* Used by stopchan
	 *         playchannelq
	 */
{
	int s;
	if (c->status&MIXRQ_PLAY16BIT)
		s=((short*)((unsigned long)c->samp*2))[c->pos];
	else
		s=(((signed char*)c->samp)[c->pos])<<8;
	fade[0]+=(c->curvols[0]*s)>>8;
	fade[1]+=(c->curvols[1]*s)>>8;
	c->curvols[0]=c->curvols[1]=0;
}

static void stopchan(struct channel *c)
{
	/* Used by SET
	 */
	if (!(c->status&MIXRQ_PLAYING))
		return;
	if (!quality)
		mixrFadeChannel(fadedown, c);
	else
		fadechanq(fadedown, c);
	c->status&=~MIXRQ_PLAYING;
}

static void amplifyfadeq(uint32_t pos, uint32_t cl, int32_t *curvol, int32_t dstvol)
	/* Used by playchannelq
	 */
{
	uint32_t l=abs(dstvol-*curvol);

	if (l>cl)
		l=cl;
	if (dstvol<*curvol)
	{
		mixqAmplifyChannelDown(buf32+pos, scalebuf, l, *curvol, 4<<stereo);
		*curvol-=l;
	} else if (dstvol>*curvol)
	{
		mixqAmplifyChannelUp(buf32+pos, scalebuf, l, *curvol, 4<<stereo);
		*curvol+=l;
	}
	cl-=l;
	if (*curvol&&cl)
		mixqAmplifyChannel(buf32+pos+(l<<stereo), scalebuf+l, cl, *curvol, 4<<stereo);
}

static void playchannelq(int ch, uint32_t len)
{
	/* Used by mixer
	 */
	struct channel *c=&channels[ch];
	if (c->status&MIXRQ_PLAYING)
	{
		int quiet=!c->curvols[0]&&!c->curvols[1]&&!c->dstvols[0]&&!c->dstvols[1];
		mixqPlayChannel(scalebuf, len, c, quiet);
		if (quiet)
			return;

		if (stereo)
		{
			amplifyfadeq(0, len, &c->curvols[0], c->dstvols[0]);
			amplifyfadeq(1, len, &c->curvols[1], c->dstvols[1]);
		} else
			amplifyfadeq(0, len, &c->curvols[0], c->dstvols[0]);

		if (!(c->status&MIXRQ_PLAYING))
			fadechanq(fadedown, c);
	}
}



static void mixer(void)
{
	/* mixer used by timerproc
	 *               Idle
	 */
	int i;
	int bufdeltatot;
	struct mixqpostprocregstruct *mode;

	if (!channelnum)
		return;

	BARRIER

	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	BARRIER

	bufdeltatot=((buflen+(plrGetBufPos()>>(stereo+bit16))-bufpos)%buflen);
	if (_pause)
	{
		uint32_t pass2=((bufpos+bufdeltatot)>buflen)?(bufpos+bufdeltatot-buflen):0;

		if (bit16)
		{
			memsetw((short*)plrbuf+(bufpos<<stereo), signedout?0:0x8000, (bufdeltatot-pass2)<<stereo);
			if (pass2)
				memsetw((short*)plrbuf, signedout?0:0x8000, pass2<<stereo);
		} else {
			memsetb((char*)plrbuf+(bufpos<<stereo), signedout?0:0x80, (bufdeltatot-pass2)<<stereo);
			if (pass2)
				memsetb((char*)plrbuf, signedout?0:0x80, pass2<<stereo);
		}

		bufpos+=bufdeltatot;
		if (bufpos>=buflen)
		bufpos-=buflen;

		plrAdvanceTo(bufpos<<(stereo+bit16));
		pausesamps+=bufdeltatot;
	} else while (bufdeltatot>0)
	{
		uint32_t bufdelta=(bufdeltatot>MIXBUFLEN)?MIXBUFLEN:bufdeltatot;

		if (bufdelta>(buflen-bufpos))
			bufdelta=buflen-bufpos;
		if (bufdelta>((tickwidth-tickplayed)>>8))
			bufdelta=(tickwidth-tickplayed)>>8;

		mixrFade(buf32, fadedown, bufdelta, stereo);
		if (!quality)
		{
			for (i=0; i<channelnum; i++)
				mixrPlayChannel(buf32, fadedown, bufdelta, &channels[i], stereo);
		} else {
			for (i=0; i<channelnum; i++)
				playchannelq(i, bufdelta);
		}

		for (mode=postprocs; mode; mode=mode->next)
			mode->Process(buf32, bufdelta, samprate, stereo);

		mixrClip((char*)plrbuf+(bufpos<<(stereo+bit16)), buf32, bufdelta<<stereo, amptab, clipmax, bit16);

		tickplayed+=bufdelta<<8;
		if (!((tickwidth-tickplayed)>>8))
		{
			tickplayed-=tickwidth;
			playerproc();
			cmdtimerpos+=tickwidth;
			tickwidth=newtickwidth;
		}
		bufpos+=bufdelta;
		if (bufpos>=buflen)
			bufpos-=buflen;

		plrAdvanceTo(bufpos<<(stereo+bit16));
		bufdeltatot-=bufdelta;
		playsamps+=bufdelta;
	}

	BARRIER

	clipbusy--;
}

static void timerproc(void)
{
	/* Referred by OpenPlayer
	 */
#ifndef NO_BACKGROUND_MIXER
	mixer(/*mcpMixPoll*/);
#endif
	if (plrIdle)
		plrIdle();
}









#ifdef __STRANGE_BUG__
void *GetReturnAddress12();
#pragma aux GetReturnAddress12="mov eax, [esp+12]" value [eax];

void sbDumpRetAddr(void *ptr)
{
  linkaddressinfostruct a;
  lnkGetAddressInfo(a, ptr);
  printf("--> called from %s.%s+0x%x, %s.%d+0x%x\n", a.module, a.sym,
	 a.symoff, a.source, a.line, a.lineoff);
  fflush(stdout);
}
#endif


static void SET(int ch, int opt, int val)
{
	/* Refered by OpenPlayer
	 */
	struct channel *chn;

#ifdef __STRANGE_BUG__
  if ((opt==mcpMasterBalance)&&(val<-64 || val>64))  /* got that damn bug. */
    {
      sbDumpRetAddr(GetReturnAddress12());
      printf("mcpSet(%d, %d, %d);\n", ch, opt, val);
    }
#endif

	if (ch>=channelnum)
		ch=channelnum-1;
	if (ch<0)
		ch=0;
	chn=&channels[ch];
	switch (opt)
	{
		case mcpCReset:
			{
				int reswasmute;
				stopchan(chn);
				reswasmute=chn->status&MIXRQ_MUTE;
				memset(chn, 0, sizeof(struct channel));
				chn->status=reswasmute;
				break;
			}
		case mcpCInstrument:
			stopchan(chn);
			if ((val<0)||(val>=samplenum))
				break;
			{
				struct sampleinfo *samp=&samples[val];
				chn->samptype=samp->type;
				chn->length=samp->length;
				chn->orgrate=samp->samprate;
				chn->samp=samp->ptr;
				chn->realsamp.bit8=chn->samp;
				chn->orgloopstart=samp->loopstart;
				chn->orgloopend=samp->loopend;
				chn->orgsloopstart=samp->sloopstart;
				chn->orgsloopend=samp->sloopend;

				chn->status&=~(MIXRQ_PLAYING|MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP|MIXRQ_PLAY16BIT|MIXRQ_PLAYSTEREO);
			}
			if (chn->samptype&mcpSamp16Bit)
			{
				chn->status|=MIXRQ_PLAY16BIT;
				chn->samp=(void*)((unsigned long)chn->samp>>1);
			}
			if (chn->samptype&mcpSampStereo)
			{
				chn->status|=MIXRQ_PLAYSTEREO;
				chn->samp=(void*)((unsigned long)chn->samp>>1);
			}
			if (chn->samptype&mcpSampSLoop)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgsloopstart;
				chn->loopend=chn->orgsloopend;
				if (chn->samptype&mcpSampSBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			} else if (chn->samptype&mcpSampLoop)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgloopstart;
				chn->loopend=chn->orgloopend;
				if (chn->samptype&mcpSampBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			chn->replen=(chn->status&MIXRQ_LOOPED)?(chn->loopend-chn->loopstart):0;
			chn->step=0;
			chn->pos=0;
			chn->fpos=0;
			break;
		case mcpCStatus:
			if (!val)
				stopchan(chn);
			else {
				if (chn->pos>=chn->length)
					break;
				chn->status|=MIXRQ_PLAYING;
				calcstep(chn);
			}
			break;
		case mcpCMute:
			if (val)
				chn->status|=MIXRQ_MUTE;
			else
				chn->status&=~MIXRQ_MUTE;
			transformvol(chn);
				break;
		case mcpCVolume:
			chn->orgvolx=(val>0x100)?0x100:(val<0)?0:val;
			calcvol(chn);
			break;
		case mcpCPanning:
			chn->orgpan=(val>0x80)?0x80:(val<-0x80)?-0x80:val;
			calcvol(chn);
			break;
		case mcpCSurround:
			chn->volopt=val?1:0;
			transformvol(chn);
			break;
		case mcpCDirect:
			if (val==0)
				chn->step=abs(chn->step);
			else
				if (val==1)
					chn->step=-abs(chn->step);
				else
					chn->step=-chn->step;
			break;
		case mcpCLoop:
			chn->status&=~(MIXRQ_LOOPED|MIXRQ_PINGPONGLOOP);
			if ((val==1)&&!(chn->samptype&mcpSampSLoop))
				val=2;
			if ((val==2)&&!(chn->samptype&mcpSampLoop))
				val=0;
			if (val==1)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgsloopstart;
				chn->loopend=chn->orgsloopend;
				if (chn->samptype&mcpSampSBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			if (val==2)
			{
				chn->status|=MIXRQ_LOOPED;
				chn->loopstart=chn->orgloopstart;
				chn->loopend=chn->orgloopend;
				if (chn->samptype&mcpSampBiDi)
					chn->status|=MIXRQ_PINGPONGLOOP;
			}
			chn->replen=(chn->status&MIXRQ_LOOPED)?(chn->loopend-chn->loopstart):0;
			if (chn->replen)
			{
				if (((chn->pos<chn->loopstart)&&(chn->step<0))||((chn->pos>=chn->loopend)&&(chn->step>0)))
					chn->step=-chn->step;
			} else if (chn->step<0)
				chn->step=-chn->step;
			break;

		case mcpCPosition:
			{
				int poswasplaying;
				poswasplaying=chn->status&MIXRQ_PLAYING;
				stopchan(chn);
				if (val<0)
					val=0;
				if ((unsigned)val>=chn->length)
				val=chn->length-1;
				chn->pos=val;
				chn->fpos=0;
				chn->status|=poswasplaying;
				break;
			}
		case mcpCPitch:
			chn->orgfrq=8363;
			chn->orgdiv=mcpGetFreq8363(-val);
			calcstep(chn);
			break;
		case mcpCPitchFix:
			chn->orgfrq=val;
			chn->orgdiv=0x10000;
			calcstep(chn);
			break;
		case mcpCPitch6848:
			chn->orgfrq=6848;
			chn->orgdiv=val;
			calcstep(chn);
			break;

		case mcpGSpeed:
			orgspeed=val;
			calcspeed();
			break;
		
		case mcpMasterVolume:
			if (val>=0 && val<=64)
				mastervol=(val<0)?0:(val>63)?63:val;
			calcvols();
			break;
		
		case mcpMasterPanning:
#ifndef __STRANGE_BUG__
			if (val>=-64 && val<=64)
#endif
				masterpan=(val<-64)?-64:(val>64)?64:val;
			calcvols();
			break;
		case mcpMasterBalance:
			if (val>=-64 && val<=64)
				masterbal=(val<-64)?-64:(val>64)?64:val;
			calcvols();
			break;
		case mcpMasterSurround:
			mastersrnd=val?1:0;
			calcvols();
			break;
		case mcpMasterReverb:
			masterrvb=(val>=64)?63:(val<-64)?-64:val;
			break;
		case mcpMasterSpeed:
			relspeed=(val<16)?16:val;
			calcspeed();
			break;
		case mcpMasterPitch:
			relpitch=val;
			calcsteps();
			break;
		case mcpMasterFilter:
			interpolation=val;
			break;
		case mcpMasterAmplify:
			amplify=val;
			if (channelnum)
			{
				calcamptab(amplify);
				mixSetAmplify(amplify);
			}
			break;
		case mcpMasterPause:
			_pause=val;
			break;
		case mcpGRestrict:
			restricted=val;
		break;
	}
}

static int GET(int ch, int opt)
{
	/* Refered by OpenPlayer
	 */
	struct channel *chn;

	if (ch>=channelnum)
		ch=channelnum-1;
	if (ch<0)
		ch=0;	
	chn=&channels[ch];
	switch (opt)
	{
		case mcpCStatus:
			return !!(chn->status&MIXRQ_PLAYING);
		case mcpCMute:
			return !!(chn->status&MIXRQ_MUTE);
		case mcpGTimer:
			if (_pause)
				return imuldiv(playsamps, 65536, samprate);
			else
				return plrGetTimer()-imuldiv(pausesamps, 65536, samprate);
		case mcpGCmdTimer:
			return umuldiv(cmdtimerpos, 256, samprate);
		case mcpMasterReverb:
			return masterrvb;
	}
	return 0;
}

static void Idle(void)
{
	mixer(/*mcpMixMax*/);
	if (plrIdle)
		plrIdle();
}



static void GetMixChannel(unsigned int ch, struct mixchannel *chn, uint32_t rate)
	/* Refered to by OpenPlayer to mixInit */
{
	struct channel *c=&channels[ch];

	if (c->status&MIXRQ_PLAY16BIT)
		chn->samp=(void*)((unsigned long)c->samp<<1);
	else
		chn->samp=c->samp;
	chn->realsamp.fmt=chn->samp; /* at this point, they match */
	chn->length=c->length;
	chn->loopstart=c->loopstart;
	chn->loopend=c->loopend;
	chn->fpos=c->fpos;
	chn->pos=c->pos;
	chn->vol.vols[0]=abs(c->vol[0]);
	chn->vol.vols[1]=abs(c->vol[1]);
	chn->step=imuldiv(c->step, samprate, (signed)rate);
	chn->status=0;
	if (c->status&MIXRQ_MUTE)
		chn->status|=MIX_MUTE;
	if (c->status&MIXRQ_PLAY16BIT)
		chn->status|=MIX_PLAY16BIT;
	if (c->status&MIXRQ_LOOPED)
		chn->status|=MIX_LOOPED;
	if (c->status&MIXRQ_PINGPONGLOOP)
		chn->status|=MIX_PINGPONGLOOP;
	if (c->status&MIXRQ_PLAYING)
		chn->status|=MIX_PLAYING;
	if (c->status&MIXRQ_INTERPOLATE)
		chn->status|=MIX_INTERPOLATE;
}

static int LoadSamples(struct sampleinfo *sil, int n)
{
#if 0
	int i;
	if (!mcpReduceSamples(sil, n, 0x40000000, mcpRedToMono))
		return 0;

	samples=malloc(sizeof(*sil)*n);
	memcpy(samples, sil, sizeof(*sil)*n);
	
	for (i=0;i<n;i++)
	{
		samples[i].ptr=malloc(samples[i].length+pagesize()*10);
		samples[i].ptr= (char *)(((int) samples[i].ptr + pagesize()*4) & ~(pagesize()-1) );
		memcpy(samples[i].ptr, sil[i].ptr, samples[i].length);
		mprotect((void *)((int)(samples[i].ptr) & ~(pagesize()-1)), samples[i].length, PROT_READ);
	}
	samplenum=n;

#else
	if (!mcpReduceSamples(sil, n, 0x40000000, mcpRedToMono))
		return 0;

	samples=sil;
	samplenum=n;
#endif
	return 1;
}

static int OpenPlayer(int chan, void (*proc)())
{
	uint32_t currentrate;
	uint16_t mixrate;
	struct mixqpostprocregstruct *mode;

	fadedown[0]=fadedown[1]=0;
	playsamps=pausesamps=0;
	if (chan>MAXCHAN)
		chan=MAXCHAN;

	if (!plrPlay)
		return 0;

	currentrate=mcpMixProcRate/chan;
	mixrate=(currentrate>mcpMixMaxRate)?mcpMixMaxRate:currentrate;
	plrSetOptions(mixrate, mcpMixOpt|(restricted?PLR_RESTRICTED:0));

	playerproc=proc;

	if (!quality)
	{
		scalebuf=0;
		voltabsq=0;
		interpoltabq=0;
		interpoltabq2=0;
		if (!(voltabsr=malloc(sizeof(uint32_t)*513*256))) /*new long [513][256];*/
			return 0;
		if (!(interpoltabr=malloc(sizeof(uint8_t)*16*256*2))) /*new unsigned char [16][256][2];*/
		{
			free(voltabsr);
			return 0;
		}
	} else {
		voltabsr=0;
		interpoltabr=0;
		if (!(scalebuf=malloc(sizeof(int16_t)*MIXBUFLEN))) /* new short [MIXBUFLEN];*/
			return 0;
		
		if (!(voltabsq=malloc(sizeof(uint16_t)*513*2*256))) /*new short [513][2][256];*/
		{
			free(scalebuf);
			scalebuf=0;
			return 0;
		}
		if (!(interpoltabq=malloc(sizeof(uint16_t)*2*32*256*2))) /*new unsigned short [2][32][256][2];*/
		{
			free(scalebuf);
			free(voltabsq);
			scalebuf=0;
			return 0;
		}
		if (!(interpoltabq2=malloc(sizeof(uint16_t)*2*16*256*4))) /*new unsigned short [2][16][256][4];*/
		{
			free(scalebuf);
			free(voltabsq);
			free(interpoltabq);
			scalebuf=0;
			return 0;
		}
	}
	if (!(buf32=malloc(sizeof(uint32_t)*(MIXBUFLEN<<1)))) /*new long [MIXBUFLEN<<1];*/
	{
		if (voltabsr) free(voltabsr);
		if (interpoltabr) free(interpoltabr);
		if (scalebuf) free(scalebuf);
		if (voltabsq) free(voltabsq);
		if (interpoltabq) free(interpoltabq);
		if (interpoltabq2) free(interpoltabq2);
		scalebuf=0;
		return 0;
	}
	if (!(amptab=malloc(sizeof(int16_t)*3*256+sizeof(int32_t)))) /* PADDING since assembler indexes some bytes beyond tab and ignores upper bits */ /*new short [3][256];*/
	{
		if (voltabsr) free(voltabsr);
		if (interpoltabr) free(interpoltabr);
		if (scalebuf) free(scalebuf);
		if (voltabsq) free(voltabsq);
		if (interpoltabq) free(interpoltabq);
		if (interpoltabq2) free(interpoltabq2);
		free(buf32);
		scalebuf=0;
		return 0;
	}

	if (!(channels=malloc(sizeof(struct channel)*chan))) /*new channel[chan];*/
	{
		if (voltabsr) free(voltabsr);
		if (interpoltabr) free(interpoltabr);
		if (scalebuf) free(scalebuf);
		if (voltabsq) free(voltabsq);
		if (interpoltabq) free(interpoltabq);
		if (interpoltabq2) free(interpoltabq2);
		free(buf32);
		free(channels);
		scalebuf=0;
		return 0;
	}

	mcpGetMasterSample=plrGetMasterSample;
	mcpGetRealMasterVolume=plrGetRealMasterVolume;
	if (!mixInit(GetMixChannel, resample, chan, amplify))
		return 0;

	memset(channels, 0, sizeof(struct channel)*chan);
	calcvols();

	if (!quality)
	{
		mixrSetupAddresses(&voltabsr[256], interpoltabr);
		calcinterpoltabr();
		calcvoltabsr();
	} else {
		mixqSetupAddresses(&voltabsq[256], interpoltabq, interpoltabq2);
		calcinterpoltabq();
		calcvoltabsq();
	}

	if (!plrOpenPlayer(&plrbuf, &buflen, mcpMixBufSize))
	{
		mixClose();
		return 0;
	}

	stereo=(plrOpt&PLR_STEREO)?1:0;
	bit16=(plrOpt&PLR_16BIT)?1:0;
	signedout=!!(plrOpt&PLR_SIGNEDOUT);
	reversestereo=!!(plrOpt&PLR_REVERSESTEREO);

	samprate=plrRate;
	bufpos=0;
	_pause=0;
	orgspeed=12800;

	channelnum=chan;
	mcpNChan=chan;
	mcpIdle=Idle;

	calcamptab(amplify);
	calcspeed();
	/* playerproc();*/  /* some timing is wrong here!*/
	tickwidth=newtickwidth;
	tickplayed=0;
	cmdtimerpos=0;
	if (!pollInit(timerproc))
	{
		mcpNChan=0;
		mcpIdle=0;
		plrClosePlayer();
		mixClose();
		return 0;
	}

	for (mode=postprocs; mode; mode=mode->next)
		if (mode->Init)
			mode->Init(samprate, stereo);

	return 1;
}

static void ClosePlayer()
{
	struct mixqpostprocregstruct *mode;

	mcpNChan=0;
	mcpIdle=0;

	pollClose();

	plrClosePlayer();

	channelnum=0;
	restricted=0;

	mixClose();

	for (mode=postprocs; mode; mode=mode->next)
		if (mode->Close)
			mode->Close();

    	if (voltabsr) free(voltabsr);
	if (interpoltabr) free(interpoltabr);
	if (scalebuf) free(scalebuf);
	if (voltabsq) free(voltabsq);
	if (interpoltabq) free(interpoltabq);
	if (interpoltabq2) free(interpoltabq2);

	free(channels);
	free(amptab);
	free(buf32);
	scalebuf=0;

	voltabsr=NULL;
	interpoltabr=NULL;
	scalebuf=NULL;
	voltabsq=NULL;
	interpoltabq=NULL;
	interpoltabq2=NULL;
}

static int wmixInit(const struct deviceinfo *dev)
{
	resample=!!(dev->opt&MIXRQ_RESAMPLE);
	quality=!!dev->subtype;

	restricted=0;
	amplify=65535;
	relspeed=256;
	relpitch=256;
	interpolation=0;
	mastervol=64;
	masterbal=0;
	masterpan=0;
	mastersrnd=0;
	channelnum=0;

	mcpLoadSamples=LoadSamples;
	mcpOpenPlayer=OpenPlayer;
	mcpClosePlayer=ClosePlayer;
	mcpGet=GET;
	mcpSet=SET;

#ifdef I386_ASM

	/* Self-modifying code needs access to modify it self */
	{
		int fd;
		char *file=strdup("/tmp/ocpXXXXXX");
		char *start1, *stop1, *start2, *stop2;
		int len1, len2;
		fd=mkstemp(file);

		start1=(void *)remap_range1_start;
		stop1=(void *)remap_range1_stop;
		start2=(void *)remap_range2_start;
		stop2=(void *)remap_range2_stop;
#ifdef MIXER_DEBUG
		fprintf(stderr, "range1: %p - %p\n", start1, stop1);
		fprintf(stderr, "range2: %p - %p\n", start2, stop2);
#endif

		start1=(char *)(((int)start1)&~(pagesize()-1));
		start2=(char *)(((int)start2)&~(pagesize()-1));
		len1=((stop1-start1)+pagesize()-1)& ~(pagesize()-1);
		len2=((stop2-start2)+pagesize()-1)& ~(pagesize()-1);
#ifdef MIXER_DEBUG
		fprintf(stderr, "mprot: %p + %08x\n", start1, len1);
		fprintf(stderr, "mprot: %p + %08x\n", start2, len2);
#endif
		if (write(fd, start1, len1)!=len1)
		{
#ifdef MIXER_DEBUG
			fprintf(stderr, "write 1 failed\n");
#endif
			return 0;
		}
		if (write(fd, start2, len2)!=len2)
		{
#ifdef MIXER_DEBUG
	 		fprintf(stderr, "write 2 failed\n");
#endif
			return 0;
		}

		if (mmap(start1, len1, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED, fd, 0)==MAP_FAILED)
		{
			perror("mmap()");
			return 0;
		}

		if (mmap(start2, len2, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED, fd, len1)==MAP_FAILED)
		{
			perror("mmap()");
			return 0;
		}
		
/*	if (mprotect((char *)(((int)remap_range1_start)&~(pagesize()-1)), (((char *)remap_range1_stop-(char *)remap_range1_start)+pagesize()-1)& ~(pagesize()-1), PROT_READ|PROT_WRITE|PROT_EXEC) ||
	    mprotect((char *)(((int)remap_range2_start)&~(pagesize()-1)), (((char *)remap_range2_stop-(char *)remap_range2_start)+pagesize()-1)& ~(pagesize()-1), PROT_READ|PROT_WRITE|PROT_EXEC) )
	    {
			perror("Couldn't mprotect"); 
 			return 0;
		}*/
#ifdef MIXER_DEBUG
		fprintf(stderr, "Done ?\n");
#endif
		close(fd);
		unlink(file);
		free(file);
	}
#endif

	return 1;
}

static void wmixClose(void)
{
	mcpOpenPlayer=0;
}


static int wmixDetect(struct deviceinfo *c)
{
	c->devtype=&mcpMixer;
	c->port=-1;
	c->port2=-1;
/*	c->irq=-1;
	c->irq2=-1;
	c->dma=-1;
	c->dma2=-1;*/
	if (c->subtype==-1)
		c->subtype=0;
	c->chan=/*(MAXCHAN>99)?99:MAXCHAN;*/MAXCHAN;
	c->mem=0;
	return 1;
}

void mixrRegisterPostProc(struct mixqpostprocregstruct *mode)
{
	mode->next=postprocs;
	postprocs=mode;
}

#include "dev/devigen.h"
#include "boot/psetting.h"
#include "dev/deviplay.h"
#include "boot/plinkman.h"

static struct mixqpostprocaddregstruct *postprocadds;

static uint32_t mixGetOpt(const char *sec)
{
	uint32_t opt=0;
	if (cfGetProfileBool(sec, "mixresample", 0, 0))
		opt|=MIXRQ_RESAMPLE;
	return opt;
}

static void mixrInit(const char *sec)
{
	char regname[50];
	const char *regs;


	fprintf(stderr, "[devwmix] INIT, ");
	if (!quality)
	{
#ifdef I386_ASM 
		fprintf(stderr, "using dwmixa.c x86-asm version\n");
#else
#ifdef I386_ASM_EMU
		fprintf(stderr, "using dwmixa.c x86-emu-asm version\n");
#else
		fprintf(stderr, "using dwmixa.c C version\n");
#endif
#endif
	} else {
#ifdef I386_ASM 
		fprintf(stderr, "using dwmixaq.c x86-asm version\n");
#else
		fprintf(stderr, "using dwmixaq.c C version\n");
#endif
	}

	postprocs=0;
	regs=cfGetProfileString(sec, "postprocs", "");

	while (cfGetSpaceListEntry(regname, &regs, 49))
	{
		void *reg=_lnkGetSymbol(regname);
		fprintf(stderr, "[%s] registering %s: %p\n", sec, regname, reg);
		if (reg)
			mixrRegisterPostProc((struct mixqpostprocregstruct*)reg);
	}

	postprocadds=0;
	regs=cfGetProfileString(sec, "postprocadds", "");
	while (cfGetSpaceListEntry(regname, &regs, 49))
	{
		void *reg=_lnkGetSymbol(regname);
		if (reg)
		{
			((struct mixqpostprocaddregstruct*)reg)->next=postprocadds;
			postprocadds=(struct mixqpostprocaddregstruct*)reg;
		}
	}
}

static int mixProcKey(unsigned short key)
{
	struct mixqpostprocaddregstruct *mode;
	for (mode=postprocadds; mode; mode=mode->next)
	{
		int r=mode->ProcessKey(key);
		if (r)
			return r;
	}

	if (plrProcessKey)
		return plrProcessKey(key);
	return 0;
}

struct devaddstruct mcpMixAdd = {mixGetOpt, mixrInit, 0, mixProcKey};
struct sounddevice mcpMixer={SS_WAVETABLE|SS_NEEDPLAYER, 0, "Mixer", wmixDetect, wmixInit, wmixClose, &mcpMixAdd};
char *dllinfo="driver mcpMixer";

struct linkinfostruct dllextinfo = {"devwmix", "OpenCP Wavetable Device: Mixer (c) 1994-10 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
