/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * ITPlay .IT module loader
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
 *   -many many many many small memory allocation bugs fixed. the heap looked
 *    more like a swiss cheese than like anything else after the player had
 *    run. but now, everything's initialised, alloced and freed correctly
 *    (i hope).
 *   -added IT2.14 decompression. Thanks to daniel for handing me that
 *    not-fully-working source which i needed to completely find out the
 *    compression (see ITSEX.CPP).
 *   -added MIDI command and filter setting reading. This will become use-
 *    ful as soon i have a software mixing routine with filters, at the
 *    moment its only use is to prevent confusion of filter and pitch
 *    envelopes
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"
#include "stuff/compat.h"
#include "itplay.h"
#include "dev/mcp.h"
#include "stuff/err.h"

int __attribute__ ((visibility ("internal"))) it_load(struct it_module *this, FILE *file)
{
	int i,j,k,n;

	struct __attribute__((packed))
	{
		uint32_t sig;
		uint8_t name[26];
		uint16_t philite;
		uint16_t nords;
		uint16_t nins;
		uint16_t nsmps;
		uint16_t npats;
		uint16_t cwtv;
		uint16_t cmwt;
		uint16_t flags;
		uint16_t special;
		uint8_t gvol;
		uint8_t mvol;
		uint8_t ispd;
		uint8_t itmp;
		uint8_t chsep;
		uint8_t pwd;
		uint16_t msglen;
		uint32_t msgoff;
		uint32_t _d3;
		uint8_t pan[64];
		uint8_t vol[64];
	} hdr;

	int signedsamp; /* boolean */
	int maxchan;

#define MAX_ORDERS 256
#define MAX_SAMPLES 100
#define MAX_INSTRUMENTS 100
#define MAX_PATTERNS 200
	
	uint8_t ords[MAX_ORDERS];
	uint32_t sampoff[MAX_SAMPLES];
	uint32_t insoff[MAX_INSTRUMENTS];
	uint32_t patoff[MAX_PATTERNS];

	this->nchan=0;
	this->ninst=0;
	this->nsampi=0;
	this->nsamp=0;
	this->npat=0;
	this->nord=0;
	this->orders=0;
	this->patlens=0;
	this->patterns=0;
	this->samples=0;
	this->instruments=0;
	this->sampleinfos=0;
	this->midicmds=0;
	this->deltapacked=0;
	this->message=0;

	fseek(file, 0, SEEK_SET);
	
	if (fread(&hdr, sizeof(hdr), 1, file) != 1)
	{
		fprintf(stderr, "[IT]: fread() failed #1\n");
		return errFileRead;
	}
	hdr.sig     = uint32_little (hdr.sig);
	hdr.philite = uint16_little (hdr.philite);
	hdr.nords   = uint16_little (hdr.nords);
	hdr.nins    = uint16_little (hdr.nins);
	hdr.nsmps   = uint16_little (hdr.nsmps);
	hdr.npats   = uint16_little (hdr.npats);
	hdr.cwtv    = uint16_little (hdr.cwtv);
	hdr.cmwt    = uint16_little (hdr.cmwt);
	hdr.flags   = uint16_little (hdr.flags);
	hdr.special = uint16_little (hdr.special);
	hdr.msglen  = uint16_little (hdr.msglen);
	hdr.msgoff  = uint32_little (hdr.msgoff);
	hdr._d3     = uint32_little (hdr._d3);
#ifdef IT_LOAD_DEBUG
	fprintf(stderr, __FILE__ ": hdr.sig %d\n", (int)hdr.sig);
	fprintf(stderr, __FILE__ ": hdr.philite %d\n", (int)hdr.philite);
	fprintf(stderr, __FILE__ ": hdr.nords %d\n", (int)hdr.nords);
	fprintf(stderr, __FILE__ ": hdr.nins %d\n", (int)hdr.nins);
	fprintf(stderr, __FILE__ ": hdr.nsmps %d\n", (int)hdr.nsmps);
	fprintf(stderr, __FILE__ ": hdr.npats %d\n", (int)hdr.npats);
	fprintf(stderr, __FILE__ ": hdr.cwtv %d\n", (int)hdr.cwtv);
	fprintf(stderr, __FILE__ ": hdr.cmwt %d\n", (int)hdr.cmwt);
	fprintf(stderr, __FILE__ ": hdr.flags %d\n", (int)hdr.flags);
	fprintf(stderr, __FILE__ ": hdr.special %d\n", (int)hdr.special);
	fprintf(stderr, __FILE__ ": hdr.msglen %d\n", (int)hdr.msglen);
	fprintf(stderr, __FILE__ ": hdr.msgoff %d\n", (int)hdr.msgoff);
	fprintf(stderr, __FILE__ ": hdr._d3 %d\n", (int)hdr._d3);
#endif
	if (hdr.sig!=0x4D504D49)
	{
		fprintf(stderr, "IT: Invalid header\n");
		return errFormSig;
	}
	if ((hdr.flags&4)&&(hdr.cmwt<0x200))
	{
		fprintf(stderr, "IT: Too old format\n");
		return errFormOldVer;
	}
	if (hdr.nords>MAX_ORDERS)
	{
		fprintf(stderr, "IT: Too many orders\n");
		return errFormStruc;
	}
	if (hdr.nins>MAX_INSTRUMENTS)
	{
		fprintf(stderr, "IT: Too many instruments\n");
		return errFormStruc;
	}
	if (hdr.nsmps>MAX_SAMPLES)
	{
		fprintf(stderr, "IT: Too many samples\n");
		return errFormStruc;
	}
	if (hdr.npats>MAX_PATTERNS)
	{
		fprintf(stderr, "IT: Too many patterns\n");
		return errFormStruc;
	}
	
	signedsamp=!(hdr.cwtv<0x202);
	this->deltapacked=(hdr.cmwt>=0x215);

	if (fread(ords, hdr.nords*sizeof(uint8_t), 1, file) != 1)
	{
		fprintf(stderr, "[IT]: fread() failed #2\n");
		return errFileRead;
	}
	if (hdr.nins)
		if (fread(insoff, hdr.nins*sizeof(uint32_t), 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #3 (hdr.nins=%d)\n", hdr.nins);
			return errFileRead;
		}
	if (fread(sampoff, hdr.nsmps*sizeof(uint32_t), 1, file) != 1)
	{
		fprintf(stderr, "[IT]: fread() failed #4\n");
		return errFileRead;
	}
	if (fread(patoff, hdr.npats*sizeof(uint32_t), 1, file) != 1)
	{
		fprintf(stderr, "[IT]: fread() failed #5\n");
		return errFileRead;
	}
	for (i=0;i<hdr.nins;i++)
		insoff[i] = uint32_little (insoff[i]);
	for (i=0;i<hdr.nsmps;i++)
		sampoff[i] = uint32_little (sampoff[i]);
	for (i=0;i<hdr.npats;i++)
		patoff[i] = uint32_little (patoff[i]);

	memcpy(this->name, hdr.name, 26);
	for ( n=0; n<26; n++ )
		if (!this->name[n])
			this->name[n]=32;
	this->name[26]=0;
	this->linearfreq=!!(hdr.flags&8);

	this->inispeed=hdr.ispd;
	this->initempo=hdr.itmp;
	this->inigvol=hdr.gvol;

	this->chsep=(hdr.flags&1)?hdr.chsep:0;
	this->instmode=hdr.flags&4;
	this->linear=hdr.flags&8;
	this->oldfx=hdr.flags&16;
	this->geffect=hdr.flags&32;
	this->instmode=hdr.flags&2;

	memcpy(this->inipan, hdr.pan, 64);
	memcpy(this->inivol, hdr.vol, 64);

	if (hdr.special&4)
	{
		uint16_t usage;
		char dummy[8];
		if (fread(&usage, sizeof(usage), 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #6\n");
			return errFileRead;
		}
		usage = uint16_little (usage);
		for (i=0; i<usage; i++)
			if (fread(dummy, 8, 1, file) != 1)
			{
				fprintf(stderr, "[IT]: fread() failed #7\n");
				return errFileRead;
			}
	}


	if (hdr.special&8)
	{
		char cmd[33];

		if (!(this->midicmds=malloc(sizeof(char *)*(9+16+128))))
		{
			fprintf(stderr, __FILE__ ": malloc(%d) failed #1\n", (int)(sizeof(char *)*(9+16+128)));
			return errAllocMem;
		}

		memset(cmd,0,sizeof(cmd));

		for (i=0; i<(9+16+128); i++)
		{
			if (fread(cmd, 32, 1, file) != 1)
			{
				fprintf(stderr, "[IT]: fread() failed #8\n");
				return errFileRead;
			}
			for ( n=0; n<32; n++ )
				if (!cmd[n])
					cmd[n]=32;
			if (!(this->midicmds[i]=malloc(sizeof(char)*(strlen(cmd)+1))))
			{
				fprintf(stderr, __FILE__ ": malloc(%d) failed #2\n", (int)(sizeof(char)*(strlen(cmd)+1)));
				return errAllocMem;
			}
			strcpy(this->midicmds[i], cmd);
			strupr(this->midicmds[i]);
		}
	}

	if (hdr.special&1)
	{
		int linect;
		char *msg;

		if (!(msg=malloc(sizeof(char)*(hdr.msglen+1))))
		{
			fprintf(stderr, __FILE__ ": malloc(%d) failed #3\n", (int)(sizeof(char)*(hdr.msglen+1)));
			return errAllocMem;
		}
		linect=1;
		fseek(file, hdr.msgoff, SEEK_SET);
		if (fread(msg, hdr.msglen, 1, file) !=1)
		{
			fprintf(stderr, "[IT]: fread() failed #9\n");
			free(msg);
			return errFileRead;
		}
		msg[hdr.msglen]=0;
		for (i=0; i<hdr.msglen; i++)
		{
			if (msg[i]==0)
				break;
			if (msg[i]==13)
				linect++;
		}
		if (!(this->message=calloc(sizeof(char *), linect+1)))
		{
			fprintf(stderr, __FILE__ ": calloc(%d) failed #4\n", (int)(sizeof(char *)*linect+1));
			free(msg);
			return errAllocMem;
		}
		*this->message=msg;
		linect=1;
		for (i=0; i<hdr.msglen; i++)
		{
			if (msg[i]==0)
				break;
			if (msg[i]==13)
			{
				msg[i]=0;
				this->message[linect++]=msg+i+1;
			}
		}
		this->message[linect]=0;
	}
	
	this->npat=hdr.npats+1;

	this->nord=hdr.nords;
	for (i=this->nord-1; i>=0; i--)
	{
		if (ords[i]<254)
			break;
		this->nord--;
	}
	if (!this->nord)
	{
		fprintf(stderr, "IT: No orders\n");
		return errFormMiss;
	}

	for (i=0; i<this->nord; i++)
		if (ords[i]==255)
			break;
	for (i=i; i>0; i--)
		if (ords[i-1]!=254)
			break;
	this->endord=i;

	if (!(this->orders=malloc(sizeof(uint16_t)*this->nord)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #5\n", (int)(sizeof(uint16_t)*this->nord));
		return errAllocMem;
	}

	for (i=0; i<this->nord; i++)
		this->orders[i]=(ords[i]==254)?0xFFFF:(ords[i]>=hdr.npats)?hdr.npats:ords[i];

	if (!(this->patlens=malloc(sizeof(uint16_t)*this->npat)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #6\n", (int)(sizeof(uint16_t)*this->npat));
		return errAllocMem;
	}
	if (!(this->patterns=malloc(sizeof(uint8_t*)*this->npat)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #7\n", (int)(sizeof(uint8_t*)*this->npat));
		return errAllocMem;
	}
			
	this->patlens[this->npat-1]=64;
	this->patterns[this->npat-1]=malloc(sizeof(uint8_t)*64);

	if (!this->patterns[this->npat-1])
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #8\n", (int)(sizeof(uint8_t)*64));
		return errAllocMem;
	}

	memset(this->patterns[this->npat-1], 0, 64);

	maxchan=0;

	for (k=0; k<hdr.npats; k++)
	{
		uint16_t patlen, patrows;
		uint8_t *patbuf;
		uint8_t lastmask[64]={0}; /* init values */
		uint8_t lastnote[64]={0}; /* init values */
		uint8_t lastins[64]={0}; /* init values */
		uint8_t lastvolpan[64]={0}; /* init values */
		uint8_t lastcmd[64]={0}; /* init values */
		uint8_t lastdata[64]={0}; /* init values */
		uint8_t *pp;
		uint8_t *wp;
	
		if (!patoff[k])
		{
			this->patlens[k]=64;
			if (!(this->patterns[k]=malloc(sizeof(uint8_t)*this->patlens[k])))
			{
				fprintf(stderr, __FILE__ ": malloc(%d) failed #9\n", (int)(sizeof(uint8_t)*this->patlens[k]));
				return errAllocMem;
			}
			memset(this->patterns[k], 0, this->patlens[k]);
			continue;
		}
		fseek(file, patoff[k], SEEK_SET);

		if (fread(&patlen, sizeof(uint16_t), 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #10\n");
			return errFileRead;
		}
		patlen = uint16_little (patlen);
		if (fread(&patrows, sizeof(uint16_t), 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #11\n");
			return errFileRead;
		}
		patrows = uint16_little (patrows);
		fseek(file, 4, SEEK_CUR);

		if (!(patbuf=malloc(sizeof(uint8_t)*patlen)))
		{
			fprintf(stderr, __FILE__ ": malloc(%d) failed #10\n", (int)(sizeof(uint8_t)*patlen));
			return errAllocMem;
		}

		if (fread(patbuf, patlen, 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #12\n");
			free(patbuf);
			return errFileRead;
		}
		this->patlens[k]=patrows;
		if (!(this->patterns[k]=malloc(sizeof(uint8_t)*((6*64+1)*this->patlens[k]))))
		{
			free(patbuf);
			fprintf(stderr, __FILE__ ": malloc(%d) failed #11\n", (int)(sizeof(uint8_t)*((6*64+1)*this->patlens[k])));
			return errAllocMem;
		}

		pp=patbuf;
		wp=this->patterns[k];
		for (i=0; i<this->patlens[k]; i++)
		{
			while (1)
			{
				int chvar=*pp++;
				uint8_t chn;
				int c;
				
				if (!chvar)
					break;
				chn=(chvar-1)&63;
				if (chvar&128)
					lastmask[chn]=*pp++;
				c=lastmask[chn];
				if (!c)
					continue;
				if (maxchan<=chn)
					maxchan=chn+1;
				if (c&1)
				{
					lastnote[chn]=(*pp<=120)?(*pp+1):*pp;
					pp++;
				}
				if (c&2)
					lastins[chn]=*pp++;
				if (c&4)
				{
					lastvolpan[chn]=1+*pp++;
				}
				if (c&8)
				{
					lastcmd[chn]=*pp++;
					lastdata[chn]=*pp++;
				}
				*wp++=chn+1;
				*wp++=(c&0x11)?lastnote[chn]:0;
				*wp++=(c&0x22)?lastins[chn]:0;
				*wp++=(c&0x44)?lastvolpan[chn]:0;
				*wp++=(c&0x88)?lastcmd[chn]:0;
				*wp++=(c&0x88)?lastdata[chn]:0;
			}
			*wp++=0;
		}
		free(patbuf);
		{
			void *new;
			if (!(new=realloc(this->patterns[k], wp-this->patterns[k])))
			{
				/* we loose the pointer for-ever here */
				fprintf(stderr, __FILE__ ": realloc(%d) failed #12\n", (int)(wp-this->patterns[k]));
				return errAllocMem;
			}
			this->patterns[k]=(uint8_t *)new;
		}
	}

	if (!maxchan)
	{
		fprintf(stderr, "IT: No channels used?\n");
		return errFormSupp;
	}
	this->nchan=maxchan;

	this->nsampi=hdr.nsmps;
	this->nsamp=hdr.nsmps;

	if (!(this->sampleinfos=malloc(sizeof(struct it_sampleinfo)*this->nsampi)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #13\n", (int)(sizeof(struct it_sampleinfo)*this->nsampi));
		return errAllocMem;
	}
	if (!(this->samples=malloc(sizeof(struct it_sample)*this->nsamp)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #14\n", (int)(sizeof(struct it_sample)*this->nsamp));
		return errAllocMem;
	}
	memset(this->sampleinfos, 0, this->nsampi*sizeof(struct it_sampleinfo));
	memset(this->samples, 0, this->nsamp*sizeof(struct it_sample));

	for (i=0; i<hdr.nsmps; i++)
		this->samples[i].handle=0xFFFF;

	for (i=0; i<hdr.nsmps; i++)
	{
		struct __attribute__((packed))
		{
			uint32_t sig;
			char dosname[13];
			uint8_t gvl;
			uint8_t flags;
			uint8_t vol;
			char name[26];
			uint8_t cvt;
			uint8_t dfp;
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			uint32_t c5spd;
			uint32_t sloopstart;
			uint32_t sloopend;
			uint32_t off;
			uint8_t vis;
			uint8_t vid;
			uint8_t vir;
			uint8_t vit;
		} shdr;

		struct it_sampleinfo *sip=&this->sampleinfos[i];
		struct it_sample *sp=&this->samples[i];

		fseek(file, sampoff[i], SEEK_SET);
		if (fread(&shdr, sizeof(shdr), 1, file) != 1)
		{
			fprintf(stderr, "[IT]: fread() failed #13\n");
			return errFileRead;
		}
		shdr.sig        = uint32_little (shdr.sig);
		shdr.length     = uint32_little (shdr.length);
		shdr.loopstart  = uint32_little (shdr.loopstart);
		shdr.loopend    = uint32_little (shdr.loopend);
		shdr.c5spd      = uint32_little (shdr.c5spd);
		shdr.sloopstart = uint32_little (shdr.sloopstart);
		shdr.sloopend   = uint32_little (shdr.sloopend);
		shdr.off        = uint32_little (shdr.off);

		sampoff[i]=shdr.off;
		strupr(shdr.dosname);

		memcpy(sp->name, shdr.name, 26);
		for ( n=0; n<26; n++ )
			if (!sp->name[n])
				sp->name[n]=32;
		sp->name[26]=0;

		if (!(shdr.flags&1))
		{
			fprintf(stderr, "itload.c: A sample with flag 0x01 set, skipped\n");
			continue;
		}

		if (!(shdr.length))
		{
			fprintf(stderr, "itload.c: A sample with length=0, skipped\n");
			continue;
		}

		sp->vol=shdr.vol;
		sp->gvl=shdr.gvl;
		sp->vir=shdr.vir;
		sp->vid=shdr.vid;
		sp->vit=shdr.vit;
		sp->vis=shdr.vis;
		sp->dfp=shdr.dfp;
		sp->handle=i;
		sp->normnote=-mcpGetNote8363(shdr.c5spd);
		sip->length=shdr.length;
		sip->samprate=shdr.c5spd>>((shdr.flags&2)?1:0);
		sip->samprate=8363;

		sip->loopstart=shdr.loopstart;
		sip->loopend=shdr.loopend;
		sip->sloopstart=shdr.sloopstart;
		sip->sloopend=shdr.sloopend;
		sip->type=((shdr.flags&2)?mcpSamp16Bit:0)|((shdr.flags&4)?mcpSampStereo:0)|(signedsamp?0:mcpSampUnsigned)|((shdr.flags&0x10)?mcpSampLoop:0)|((shdr.flags&0x40)?mcpSampBiDi:0)|((shdr.flags&0x80)?mcpSampSBiDi:0)|((shdr.flags&0x20)?mcpSampSLoop:0);

		sp->packed=(shdr.flags&8?1:0);
		if (sp->packed && this->deltapacked && (shdr.cvt & 4))
			sp->packed|=2;

		if (!(sip->ptr=malloc((sip->length+512)<<((sip->type&mcpSamp16Bit)?1:0))))
		{
			fprintf(stderr, __FILE__ ": malloc(%d) failed #15\n", (sip->length+512)<<((sip->type&mcpSamp16Bit)?1:0));
			return errAllocMem;
		}
	}

	for (i=0; i<hdr.nsmps; i++)
	{
		struct it_sampleinfo *sip=&this->sampleinfos[i];
		struct it_sample *sp=&this->samples[i];

		if (!sip->ptr)
			continue;
		fseek(file, sampoff[i], SEEK_SET);
		
		if (sp->packed) {
			if (sip->type & mcpSamp16Bit)
				decompress16(file, sip->ptr, sip->length, sp->packed&2);
			else
				decompress8(file, sip->ptr, sip->length, sp->packed&2);
		} else {
			if (fread(sip->ptr, sip->length<<((sip->type&mcpSamp16Bit)?1:0), 1, file) != 1)
			{
				fprintf(stderr, "[IT]: fread() failed #14 (sip-ptr=%p sip->length=%d 16bit=%d)\n", sip->ptr, (int)sip->length, !!(sip->type&mcpSamp16Bit));
				return errFileRead;
			}
		}
	}

	this->ninst=(hdr.flags&4)?hdr.nins:hdr.nsmps;
	if (!(this->instruments=malloc(sizeof(struct it_instrument)*this->ninst)))
	{
		fprintf(stderr, __FILE__ ": malloc(%d) failed #16\n", (int)(sizeof(struct it_instrument)*this->ninst));
		return errAllocMem;
	}
	memset(this->instruments, 0, this->ninst*sizeof(*this->instruments));

	for (k=0; k<this->ninst; k++)
	{
	        struct __attribute__((packed)) envp
		{
			int8_t v;
			uint16_t p;
		};
		struct __attribute__((packed)) env
		{
			uint8_t flags;
			uint8_t num;
			uint8_t lpb;
			uint8_t lpe;
			uint8_t slb;
			uint8_t sle;
			struct envp pts[25];
			uint8_t _d1;
		};
		struct __attribute__((packed)) itiheader
		{
			uint32_t sig;
			char dosname[13];
			uint8_t nna;
			uint8_t dct;
			uint8_t dca;
			uint16_t fadeout;
			uint8_t pps;
			uint8_t ppc;
			uint8_t gbv;
			uint8_t dfp;
			uint8_t rv;
			uint8_t rp;
			uint16_t tver;
			uint8_t nos;
			uint8_t _d3;
			char name[26];
			uint8_t ifp;
			uint8_t ifr;
			uint8_t mch;
			uint8_t mpr;
			uint16_t midibnk;
			uint8_t keytab[120][2];
			struct env envs[3];
		};
		struct itiheader ihdr;

		struct it_instrument *ip=&this->instruments[k];

		if (hdr.flags&4)
		{
			int i, j;
			fseek(file, insoff[k], SEEK_SET);
			if (fread(&ihdr, sizeof(ihdr), 1, file) != 1)
			{
				fprintf(stderr, "[IT]: fread() failed #15\n");
				return errFileRead;
			}
			ihdr.sig     = uint32_little (ihdr.sig);
			ihdr.fadeout = uint16_little (ihdr.fadeout);
			ihdr.tver    = uint16_little (ihdr.tver);
			ihdr.midibnk = uint16_little (ihdr.midibnk);
			for (i=0;i<4;i++)
				for (j=0;j<26;j++)
					ihdr.envs[i].pts[j].p = uint16_little (ihdr.envs[i].pts[j].p);
		} else {
			memset(&ihdr, 0, sizeof(ihdr));
			ihdr.sig=ihdr.tver=ihdr.nos=ihdr._d3=0;
#ifdef _WATCOM_
			if(ihdr.dosname); /*suppress stupid watcom warning */
#endif
			for (i=0; i<120; i++)
			{
				ihdr.keytab[i][0]=i;
				ihdr.keytab[i][1]=k+1;
			}
			memcpy(ihdr.name, this->samples[k].name, 26);
			ihdr.name[25]=0;
			ihdr.dfp=0x80;
			ihdr.gbv=128;
		}
		memcpy(ip->name, ihdr.name, 26);
		for ( n=0; n<26; n++ )
			if (!ip->name[n])
				ip->name[n]=32;
		ip->name[26]=0;
		ip->name[27]=0;
		ip->name[28]=0;
		ip->handle=k;
		ip->fadeout=ihdr.fadeout;
		ip->nna=ihdr.nna;
		ip->dct=ihdr.dct;
		ip->dca=ihdr.dca;
		ip->pps=ihdr.pps;
		ip->ppc=ihdr.ppc;
		ip->gbv=ihdr.gbv;
		ip->dfp=ihdr.dfp;
		ip->ifp=ihdr.ifp;
		ip->ifr=ihdr.ifr;
		ip->mch=ihdr.mch;
		ip->mpr=ihdr.mpr;
		ip->midibnk=ihdr.midibnk;
		ip->rv=ihdr.rv;
		ip->rp=ihdr.rp;

		memcpy(ip->keytab, ihdr.keytab, 120*2);

		for (i=0; i<3; i++)
		{
			struct env *ie=&ihdr.envs[i];
			struct it_envelope *e=&ip->envs[i];
			e->type=((ie->flags&1)?active:0)|
				((ie->flags&2)?looped:0)|
				((ie->flags&4)?slooped:0)|
				((ie->flags&8)?carry:0)|
				((ie->flags&128)?filter:0);
			e->len=ie->num-1;
			e->sloops=ie->slb;
			e->sloope=ie->sle;
			e->loops=ie->lpb;
			e->loope=ie->lpe;
			
#ifdef _WATCOM_
			ie->_d1=ie->_d1; /* suppress stupid watcom warning */
#endif

			for (j=0; j<ie->num; j++)
			{
				e->y[j]=ie->pts[j].v;
				e->x[j]=ie->pts[j].p;
			}
		}
	}
	return 0;
}

void __attribute__ ((visibility ("internal"))) it_free(struct it_module *this)
{
	int i;

	if (this->sampleinfos)
	{
		for (i=0; i<this->nsampi; i++)
			if (this->sampleinfos[i].ptr)
				free(this->sampleinfos[i].ptr);
		free(this->sampleinfos);
	}
	if (this->samples)
		free(this->samples);
	if (this->instruments)
		free(this->instruments);
	if (this->patterns)
	{
		for (i=0; i<this->npat; i++)
			if (this->patterns[i])
				free(this->patterns[i]);
		free(this->patterns);
	}
	if (this->patlens)
		free(this->patlens);
	if (this->orders)
		free(this->orders);
	if (this->message)
	{
		free(*this->message);
		free(this->message);
	}
	if (this->midicmds)
	{
		for (i=0; i<(9+16+128); i++)
			if (this->midicmds[i])	
				free(this->midicmds[i]);
		free(this->midicmds);
	}
}
