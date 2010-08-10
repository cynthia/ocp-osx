/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * assembler routines for FPU mixer
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
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -first release
 *  -ryg990426  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -extreeeeem kbchangesapplying+sklavenarbeitverrichting
 *     (was mir angst macht, ich finds nichmal schlimm)
 *  -ryg990504  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -added float postprocs, the key to player realtimeruling
 *  -kb990531   Tammo Hinrichs <opengp@gmx.net>
 *    -fixed mono playback
 *    -cubic spline interpolation now works
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -ported to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made it optimizesafe
 *
 * dominators und doc rooles geiler floating point mixer mit volume ramps
 * (die man gar nicht benutzen kann (kb sagt man kann). und mit
 * ultra-rauschabstand und viel geil interpolation.
 * wir sind besser als ihr...
 */

#include "config.h"
#include "types.h"
#include "devwmixf.h"
#include "dwmixfa.h"

#ifndef I386_ASM
#ifdef I386_ASM_EMU
#include "asm_emu/x86.h"
#endif
#endif

#ifdef I386_ASM

#define MAXVOICES MIXF_MAXCHAN
#define FLAG_DISABLED (~MIXF_PLAYING)
float   *tempbuf;               /* pointer to 32 bit mix buffer (nsamples * 4) */
void    *outbuf;                /* pointer to mixed buffer (nsamples * 2) */
uint32_t nsamples;              /* # of samples to mix */
uint32_t nvoices;               /* # of voices to mix */
uint32_t freqw[MAXVOICES];      /* frequency (whole part) */
uint32_t freqf[MAXVOICES];      /* frequency (fractional part) */
float   *smpposw[MAXVOICES];    /* sample position (whole part (pointer!)) */
uint32_t smpposf[MAXVOICES];    /* sample position (fractional part) */
float   *loopend[MAXVOICES];    /* pointer to loop end */
uint32_t looplen[MAXVOICES];    /* loop length in samples */
float    volleft[MAXVOICES];    /* float: left volume (1.0=normal) */
float    volright[MAXVOICES];   /* float: rite volume (1.0=normal) */
float    rampleft[MAXVOICES];   /* float: left volramp (dvol/sample) */
float    rampright[MAXVOICES];  /* float: rite volramp (dvol/sample) */
uint32_t voiceflags[MAXVOICES]; /* voice status flags */
float    ffreq[MAXVOICES];      /* filter frequency (0<=x<=1) */
float    freso[MAXVOICES];      /* filter resonance (0<=x<1) */
float    fadeleft=0.0;          /* 0 */
float    fl1[MAXVOICES];        /* filter lp buffer */
float    fb1[MAXVOICES];        /* filter bp buffer */
float    faderight=0.0;         /* 0 */
int      isstereo;              /* flag for stereo output */
int      outfmt;                /* output format */
float    voll=0.0;
float    volr=0.0;
float    ct0[256];              /* interpolation tab for s[-1] */
float    ct1[256];              /* interpolation tab for s[0] */
float    ct2[256];              /* interpolation tab for s[1] */
float    ct3[256];              /* interpolation tab for s[2] */
struct mixfpostprocregstruct *postprocs;
                                /* pointer to postproc list */
uint32_t samprate;              /* sampling rate */




static float __attribute__ ((used)) volrl;
static float __attribute__ ((used)) volrr;

static float __attribute__ ((used)) eins=1.0;
static float __attribute__ ((used)) minuseins=-1.0;
static float __attribute__ ((used)) clampmax=32767.0;
static float __attribute__ ((used)) clampmin=-32767.0;
static float __attribute__ ((used)) cremoveconst=0.992;
static float __attribute__ ((used)) minampl=0.0001; /* what the fuck? why is this a float? - stian */
static float __attribute__ ((used)) magic1;  /* 32bit in assembler used */
static uint16_t __attribute__ ((used)) clipval; /* 16bit in assembler used */
static uint32_t __attribute__ ((used)) mixlooplen; /* 32bit in assembler used, decimal. lenght of loop in samples*/
static uint32_t __attribute__ ((used)) looptype; /* 32bit in assembler used, local version of voiceflags[N] */
static float __attribute__ ((used)) ffrq;
static float __attribute__ ((used)) frez;
static float __attribute__ ((used)) __fl1;
static float __attribute__ ((used)) __fb1;

void start_dwmixfa(void)
{
	volrl=volrl;
	volrr=volrr;
	eins=eins;
	minuseins=minuseins;
	clampmin=clampmin;
	clampmax=clampmax;
	cremoveconst=cremoveconst;
	minampl=minampl;
	magic1=magic1;
	clipval=clipval;
	mixlooplen=mixlooplen;
	looptype=looptype;
	ffrq=ffrq;
	frez=frez;
	__fl1=__fl1;
	__fb1=__fb1;
}

void prepare_mixer (void)
{
	__asm__ __volatile__
	(
		"xorl %%eax, %%eax\n"
		"movl %%eax, fadeleft\n"
		"movl %%eax, faderight\n"
		"movl %%eax, volrl\n"
		"movl %%eax, volrr\n"
		"xorl %%ecx, %%ecx\n"
	"prepare_mixer_fillloop:\n"
		"movl %%eax, volleft(,%%ecx,4)\n"
		"movl %%eax, volright(,%%ecx,4)\n"
		"incl %%ecx\n"
		"cmpl %0, %%ecx\n"
		"jne prepare_mixer_fillloop\n"
		:
		: "n" (MAXVOICES)
		: "eax"
	);
}


void mixer (void)
{
#ifdef DEBUG
	fprintf(stderr, "mixer()");
	fprintf(stderr, "tempbuf=%p\n", tempbuf);
	fprintf(stderr, "outbuf=%p\n", outbuf);
	fprintf(stderr, "nsamples=%d (samples to mix)\n", (int)nsamples);
	fprintf(stderr, "nvoices=%d (voices to mix)\n", (int)nvoices);
	{
		int i;
		for (i=0;i<nvoices;i++)
		{
			fprintf(stderr, "freqw.f[%d]=%u.%u\n", i, (unsigned int)freqw[i], (unsigned int)freqf[i]);
			fprintf(stderr, "smpposw.f[%d]=%p.%u\n", i, smpposw[i], (unsigned int)smpposf[i]);
			fprintf(stderr, "loopend[%d]=%p\n", i, loopend[i]);
			fprintf(stderr, "looplen[%d]=%u\n", i, (unsigned int)looplen[i]);
			fprintf(stderr, "volleft[%d]=%f\n", i, volleft[i]);
			fprintf(stderr, "volright[%d]=%f\n", i, volright[i]);
			fprintf(stderr, "rampleft[%d]=%f\n", i, rampleft[i]);
			fprintf(stderr, "rampright[%d]=%f\n", i, rampright[i]);
			fprintf(stderr, "voiceflags[%d]=0x%08x\n", i, (unsigned int)voiceflags[i]);
			fprintf(stderr, "ffreq[%d]=%f\n", i, ffreq[i]);
			fprintf(stderr, "freso[%d]=%f\n", i, freso[i]);
			fprintf(stderr, "fl1[%d]=%f\n", i, fl1[i]);
			fprintf(stderr, "fb1[%d]=%f\n", i, fb1[i]);
		}
	}
	fprintf(stderr, "fadeleft=%f\n", fadeleft);
	fprintf(stderr, "faderight=%f\n", faderight);
	fprintf(stderr, "isstereo=%d\n", isstereo);
	fprintf(stderr, "outfmt=%d\n", outfmt);
	/* ct0, ct1, ct2, ct3 */
	fprintf(stderr, "\n");
#endif

	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
	 	"pushl %%ebp\n"
		"finit\n"

		/* range check for declick values */
		"  xorl %%ebx, %%ebx\n"
		"  movl fadeleft, %%eax\n"
		"  andl $0x7fffffff, %%eax\n"
		"  cmpl %%eax, minampl\n"
		"  ja mixer_nocutfl\n"
		"  movl %%ebx, fadeleft\n"
	"  mixer_nocutfl:\n"
		"  movl faderight, %%eax\n"
		"  andl $0x7fffffff, %%eax\n"
		"  cmpl %%eax, minampl\n"
		"  ja mixer_nocutfr\n"
		"  movl %%ebx, faderight\n"
	"  mixer_nocutfr:\n"

		/* clear and declick buffer */
		"  movl tempbuf, %%edi\n"
		"  movl nsamples, %%ecx\n"
		"  orl %%ecx, %%ecx\n"
		"  jz mixer_endall\n"
		"  movl isstereo, %%eax\n"  /* STEREO DEP 1 EAX */
		"  orl %%eax, %%eax\n"      /* STEREO DEP 1 EAX */
		"  jnz mixer_clearst\n"     /* STEREO DEP 1 BRANCH */
		"    call clearbufm\n"      /* STEREO DEP 1 BRANCH */
		"    jmp mixer_clearend\n"  /* STEREO DEP 1 BRANCH */
	"  mixer_clearst:\n"                /* STEREO DEP 1 BRANCH */
		"  call clearbufs\n"        /* STEREO DEP 1 BRANCH */
	"  mixer_clearend:\n"               /* STEREO DEP 1 BRANCH */

		"  movl nvoices, %%ecx\n"
		"  decl %%ecx\n"

	"  mixer_MixNext:\n"
		"    movl voiceflags(,%%ecx,4), %%eax\n" /* VOICEFLAGS DEP 2 EAX */
		"    testl %0, %%eax\n"                  /* VOICEFLAGS DEP 2 EAX */
		"    jz mixer_SkipVoice\n"               /* VOICEFLAGS DEP 2 BRANCH */
  
		/* set loop type */
		"    movl %%eax, looptype\n"             /* VOICEFLAGS DEP 2 EAX,LOOPTYPE */

		/* calc l/r relative vols from vol/panning/amplification */
		"    movl volleft(,%%ecx,4), %%eax\n"
		"    movl volright(,%%ecx,4), %%ebx\n"
		"    movl %%eax, voll\n"
		"    movl %%ebx, volr\n"

		"    movl rampleft(,%%ecx,4), %%eax\n"
		"    movl rampright(,%%ecx,4), %%ebx\n"
		"    movl %%eax, volrl\n"
		"    movl %%ebx, volrr\n"

		/* set up filter vals */
		"    movl ffreq(,%%ecx,4), %%eax\n"
		"    movl %%eax, ffrq\n"
		"    movl freso(,%%ecx,4), %%eax\n"
		"    movl %%eax, frez\n"
		"    movl fl1(,%%ecx,4), %%eax\n"
		"    movl %%eax, __fl1\n"
		"    movl fb1(,%%ecx,4), %%eax\n"
		"    movl %%eax, __fb1\n"
  
		/* length of loop */
		"    movl looplen(,%%ecx,4), %%eax\n"
		"    movl %%eax, mixlooplen\n"
  
		/* sample delta: */
		"    movl freqw(,%%ecx,4), %%ebx\n"
		"    movl freqf(,%%ecx,4), %%esi\n"

		/* Sample base Pointer */
		"    movl smpposw(,%%ecx,4), %%eax\n"
  
		/* sample base ptr fraction part */
		"    movl smpposf(,%%ecx,4), %%edx\n"

		/* Loop end Pointer */
		"    movl loopend(,%%ecx,4), %%ebp\n"

		"    pushl %%ecx\n"
		"    movl tempbuf, %%edi\n"
#warning ISSTEREO is masked in at reserved input bit for now...
		"    movl isstereo, %%ecx\n"             /* STEREO DEP 3, ECX */
		"    orl voiceflags(,%%ecx,4), %%ecx\n"  /* VOICEFLAGS,STEREO DEP 3, ECX.. we can use looptype instead, less complex */
		"    andl $15, %%ecx\n"
		"    movl mixers(,%%ecx,4), %%ecx\n"     /* VOICEFLAGS,STEREO,MIXERS DEP 3,ECX */
		"    call *%%ecx\n"                      /* this call modifies LOOPTYPE */
		"    popl %%ecx\n"

		/* calculate sample relative position */
		"    movl %%eax, smpposw(,%%ecx,4)\n"
		"    movl %%edx, smpposf(,%%ecx,4)\n"

		/* update flags */
		"    movl looptype, %%eax\n"            /* VOICEFLAG DEP 4, EAX */
		"    movl %%eax, voiceflags(,%%ecx,4)\n"/* VOICEFLAG DEP 4, EAX,VOICEFLAGS (copy back from LOOPTYPE) */ 

		/* update volumes */
		"    movl voll, %%eax\n"
		"    movl %%eax, volleft(,%%ecx,4)\n"
		"    movl volr, %%eax\n"
		"    movl %%eax, volright(,%%ecx,4)\n"

		/* update filter buffers */
		"    movl __fl1, %%eax\n"
		"    movl %%eax, fl1(,%%ecx,4)\n"
		"    movl __fb1, %%eax\n"
		"    movl %%eax, fb1(,%%ecx,4)\n"

	"    mixer_SkipVoice:\n"
		"    decl %%ecx\n"
		"  jns mixer_MixNext\n"

/* ryg990504 - changes for floatpostprocs start here */

/* how parameters are sent needs to be redone for gcc
 *
 * (and even gcc can been overriden for an arch due to optimization)
 *          - Stian    TODO TODO TODO TODO
 */
#warning this needs to be updated into more generic code
		"  movl postprocs, %%esi\n"

	"  mixer_PostprocLoop:\n"
		"    orl %%esi, %%esi\n"
		"    jz mixer_PostprocEnd\n"

		"    movl nsamples, %%edx\n"
		"    movl isstereo, %%ecx\n"
		"    movl samprate, %%ebx\n"
		"    movl tempbuf, %%eax\n"
		"    call *%%esi\n"

#warning 12 here should be asm-parameter
		"    movl 12(%%esi), %%esi\n"

		"  jmp mixer_PostprocLoop\n"

	"mixer_PostprocEnd:\n"

/* ryg990504 - changes for floatpostprocs end here */
		"  movl outfmt, %%eax\n"
		"  movl clippers(,%%eax,4), %%eax\n"

		"  movl outbuf, %%edi\n"
		"  movl tempbuf, %%esi\n"
		"  movl nsamples, %%ecx\n"

		"  movl isstereo, %%edx\n"
		"  orl %%edx, %%edx\n"
		"  jz mixer_clipmono\n"
		"    addl %%ecx, %%ecx\n"
	"mixer_clipmono:\n"

		"  call *%%eax\n"

	"mixer_endall:\n"
		"popl %%ebp\n"
#ifdef __PIC__
	 	"popl %%ebx\n"
#endif
		:
		: "n"(MIXF_PLAYING)
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx", "edi", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "edi", "esi"
#endif
	);
}

static __attribute__ ((used)) void dummy(void)
{
	
/* clear routines:
 * edi : 32 bit float buffer
 * ecx : # of samples
 */

/* clears and declicks tempbuffer (mono) */
	__asm__ __volatile__
	(
	"clearbufm:\n"
		"flds cremoveconst\n"        /* (fc) */
		"flds fadeleft\n"            /* (fl) (fc) */

	"clearbufm_clloop:\n"
		"  fsts (%edi)\n"
		"  fmul %st(1),%st\n"        /* (fl') (fc) */
		"  leal 4(%edi), %edi\n"
		"  decl %ecx\n"
		"jnz clearbufm_clloop\n"

		"fstps fadeleft\n"           /* (fc) */
		"fstp %st\n"                 /* - */

		"ret\n"
	);


/* clears and declicks tempbuffer (stereo)
 * edi : 32 bit float buffer
 * ecx : # of samples
 */
	__asm__ __volatile__
	(
	"clearbufs:\n"
		"flds cremoveconst\n"        /* (fc) */
		"flds faderight\n"           /* (fr) (fc) */
		"flds fadeleft\n"            /* (fl) (fr) (fc) */

	"clearbufs_clloop:\n"
		"  fsts (%edi)\n"
		"  fmul %st(2), %st\n"       /* (fl') (fr) (fc) */
		"  fxch %st(1)\n"            /* (fr) (fl') (fc) */
		"  fsts 4(%edi)\n"
		"  fmul %st(2), %st\n"       /* (fr') (fl') (fc) */
		"  fxch %st(1)\n"            /* (fl') (fr') (fc) */
		"  leal 8(%edi),%edi\n"
		"  decl %ecx\n"
		"jnz clearbufs_clloop\n"

		"fstps fadeleft\n"           /* (fr') (fc) */
		"fstps faderight\n"          /* (fc) */
		"fstp %st\n"                 /* - */

		"ret\n"
	);


/*
 * mixing routines:
 * eax = sample loop length.
 * edi = dest ptr auf outbuffer
 * ecx = # of samples to mix
 * ebx = delta to next sample (whole part)
 * edx = fraction of sample position
 * esi = fraction of sample delta
 * ebp = ptr to loop end
 */

	__asm__ __volatile__
	(
	"mix_0:\n"
	/* mixing, MUTED
	 * quite sub-obtimal to do this with a loop, too, but this is really
	 * the only way to ensure maximum precision - and it's fully using
	 * the vast potential of the coder's lazyness.
	 */
		"movl nsamples, %%ecx\n"
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, 2+mix_0_SM1\n" /* nasty self-modifying code. Kids, don't do this at home */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	"mix_0_next:\n"
		"  addl %%esi, %%edx\n"
		"  adcl %%ebx, %%ebp\n"
	"mix_0_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mix_0_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mix_0_LoopHandler\n"
		"  decl %%ecx\n"
		"jnz mix_0_next\n"
	"mix_0_ende:\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"
	"mix_0_LoopHandler:\n"
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mix_0_loopme\n"
		/*"movl looptype, %%eax\n"*/
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mix_0_ende\n"
	"mix_0_loopme:\n"
		"subl mixlooplen, %%ebp\n"
		"jmp mix_0_looped\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_n:\n"
	/* mixing, mono w/o interpolation
	 */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_n_SM1+2\n" /* self modifying code.  set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword we don't need.. alignment is 32bit by default on gnu i386*/
	"mixm_n_next:\n"                     /* (vl) */
		"  flds (,%%ebp,4)\n"        /* (wert) (vl) */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (left) */
		"  fadds volrl\n"            /* (vl') (left) */
		"  fxch %%st(1)\n"           /* (left) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') */
	"mixm_n_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_n_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixm_n_LoopHandler\n"
		"  fstps -4(%%edi)\n"        /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_n_next\n"
	"mixm_n_ende:\n"
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_n_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_n_loopme\n"
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) */
	"mixm_n_fill:\n" /*  sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"        /* (wert) (vl) */
		"  fstps -4(%%edi)\n"        /* (wert) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (wert) */
		"  fadds volrl\n"            /* (vl') (wert) */
		"  fxch %%st(1)\n"           /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_n_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) */
		"fadds fadeleft\n"           /* (fl') (vl) */
		"fstps fadeleft\n"           /* (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_n_ende\n"

	"mixm_n_loopme:\n" /* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_n_loopme\n"
		"decl %%ecx\n"
		"jz mixm_n_ende\n"
		"jmp mixm_n_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_n:\n"
	/* mixing, stereo w/o interpolation
	 */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"flds volr\n"                /* (vr) (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp,2+mixs_n_SM1\n"  /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword.... we are already align 32bit */
	"mixs_n_next:\n"
		"  flds (,%%ebp,4)\n"        /* (wert) (vr) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_n_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_n_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixs_n_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) */
		"  fxch %%st(1)\n"           /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_n_next\n"
	"mixs_n_ende:\n"
		"fstps volr\n"               /* (vl) */
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_n_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_n_loopme\n"
		"fxch %%st(1)\n"             /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_n_fill:\n" /* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_n_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"             /* (wert) (vl) (vr) */
		"fld %%st\n"                 /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"       /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"             /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"       /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"             /* (left) (rite) (vl) (vr) */
		"fadds fadeleft\n"           /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vl) (vr) */
		"fadds faderight\n"          /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft\n"           /* (fr') (vl) (vr) */
		"fstps faderight\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_n_ende\n"

	"mixs_n_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_n_loopme\n"
		"decl %%ecx\n"
		"jz mixs_n_ende\n"
		"jmp mixs_n_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_i:\n"
	/* mixing, mono+interpolation */
		"movl nsamples, %%ecx\n"
		"flds minuseins\n"           /* (-1) */
		"flds voll\n"                /* (vl) (-1) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_i_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1\n"

	/* align dword... we don't need to align shit here? */
	"mixm_i_next:\n"                     /* (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"       /* (a) (vl) (-1) */
		"  fld %%st(0)\n"            /* (a) (a) (vl) (-1) */
		"  fld %%st(3)\n"            /* (-1) (a) (a) (vl) (-1) */
		"  fadds magic1\n"           /* (t) (a) (a) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (a) (t) (a) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"     /* (b-a) (t) (a) (vl) (-1) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st(1)\n"          /* ((b-a)*t) (a) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"          /* (wert) (vl) (-1) */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) (-1) */
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (vl) (left) (-1) */
		"  fadds volrl\n"            /* (vl') (left) (-1) */
		"  fxch %%st(1)\n"           /* (left) (vl) (-1) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixm_i_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_i_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  movl %%eax, magic1\n"
		"  jae mixm_i_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_i_next\n"
	"mixm_i_ende:\n"
		"fstps voll\n"               /* (whatever) */
		"fstp %%st\n"                /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_i_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (-1) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i_loopme\n"
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl)  (-1) */
	"mixm_i_fill:\n" /* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl)  (-1) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) (-1) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (wert) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (vl) (wert) (-1) */
		"  fadds volrl\n"            /* (vl') (wert) (-1) */
		"  fxch %%st(1)\n"           /* (wert) (vl') (-1) */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) (-1) */
		"fadds fadeleft\n"           /* (fl') (vl) (-1) */
		"fstps fadeleft\n"           /* (vl) (-1) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_i_ende\n"

	"mixm_i_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i_loopme\n"
		"decl %%ecx\n"
		"jz mixm_i_ende\n"
		"jmp mixm_i_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_i:\n"
	/* mixing, stereo+interpolation */
		"movl nsamples, %%ecx\n"
		"flds minuseins\n"           /* (-1) */
		"flds voll\n"                /* (vl) (-1) */
		"flds volr\n"                /* (vr) (vl) (-1) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, 2+mixs_i_SM1\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1\n"

	/* align dword... njet! */
	"mixs_i_next:\n"                     /* (vr) (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"       /* (a) (vr) (vl) (-1) */
		"  fld %%st(0)\n"            /* (a) (a) (vr) (vl) (-1) */
		"  fld %%st(4)\n"            /* (-1) (a) (a) (vr) (vl) (-1) */
		"  fadds magic1\n"           /* (t) (a) (a) (vr) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (a) (t) (a) (vr) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"     /* (b-a) (t) (a) (vr) (vl) (-1) */
		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st(1)\n"          /* ((b-a)*t) (a) (vr) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"          /* (wert) (vr) (vl) (-1) */
		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) (-1) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) (-1) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) (-1) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) (-1) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) (-1) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) (-1) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) (-1) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) (-1) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) (-1) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') (-1) */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') (-1) */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') (-1) */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixs_i_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_i_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  movl %%eax, magic1\n"
		"  jae mixs_i_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) <vr'> (-1) */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr) (-1) */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) (-1) */
		"  fxch %%st(1)\n"           /* (vr) (vl) (-1) */
		"  decl %%ecx\n"
		"jnz mixs_i_next\n"
	"mixs_i_ende:\n"
		"fstps volr\n"
		"fstps voll\n"
		"fstp %%st\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_i_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) <vr'> (-1) */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) (-1) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) (-1) */
		"fxch %%st(1)\n"             /* (vr) (vl) (-1) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i_loopme\n"
		"fxch %%st(2)\n"             /* (-1) (vl) (vr) */
		"fstp %%st\n"                /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_i_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"        /* (vr) (vl) (wert)            This should be (lfinal) (vr) (right) (vl) (wert) - stian */
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"        /* (vr) (vl) (wert)            This should be (rfinal) (vr) (vl) (wert) - stian */
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_i_fill\n"
	/* update click-removal fade values */
		"fld %%st(2)\n"              /* (wert) (vr) (vl) (wert) */
		"fld %%st\n"                 /* (wert) (wert) (vr) (vl) (wert) */
		"fmul %%st(3), %%st\n"       /* (left) (wert) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (wert) (left) (vr) (vl) (wert) */
		"fmul %%st(2), %%st\n"       /* (rite) (left) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (left) (rite) (vr) (vl) (wert) */
		"fadds fadeleft\n"           /* (fl') (rite) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vr) (vl) (wert) */
		"fadds faderight\n"          /* (fr') (fl') (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vr) (vl) (wert) */
		"fstps fadeleft\n"           /* (fr') (vr) (vl) (wert) */
		"fstps faderight\n"          /* (vr) (vl) (wert) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_i_ende\n"

	"mixs_i_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i_loopme\n"
		"decl %%ecx\n"
		"jz mixs_i_ende\n"
		"jmp mixs_i_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_i2:\n"
	/* mixing, mono w/ cubic interpolation */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_i2_SM1\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword we don't give a rats ass about */
	"mixm_i2_next:\n"                    /* (vl) */
		"  flds (,%%ebp,4)\n"        /* (w0) (vl) */
		"  fmuls ct0(,%%eax,4)\n"    /* (w0') (vl) */
		"  flds 4(,%%ebp,4)\n"       /* (w1) (w0') (vl) */
		"  fmuls ct1(,%%eax,4)\n"    /* (w1') (w0') (vl) */
		"  flds 8(,%%ebp,4)\n"       /* (w2) (w1') (w0') (vl) */
		"  fmuls ct2(,%%eax,4)\n"    /* (w2') (w1') (w0') (vl) */
		"  flds 12(,%%ebp,4)\n"      /* (w3) (w2') (w1') (w0') (vl) */
		"  fmuls ct3(,%%eax,4)\n"    /* (w3') (w2') (w1') (w0') (vl) */
		"  fxch %%st(2)\n"           /* (w1') (w2') (w3') (w0') (vl) */
		"  faddp %%st, %%st(3)\n"    /* (w2') (w3') (w0+w1) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"    /* (w2+w3) (w0+w1) (vl) */ /* I find this to be wrong - Stian TODO  faddp %st %st(1) anybody ?  */
		"  adcl %%ebx, %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st,%%st(1)\n"     /* (wert) (vl) */ /* But since we add them together here it all ends correct - Stian */
		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (left) */
		"  fadds volrl\n"            /* (vl') (left) */
		"  fxch %%st(1)\n"           /* (left) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') */
	"mixm_i2_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_i2_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixm_i2_LoopHandler\n"
		"  fstps -4(%%edi)\n"        /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_i2_next\n"
	"mixm_i2_ende:\n"
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_i2_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') */
		"pushl %%eax\n"
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i2_loopme\n"
		"popl %%eax\n"
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) */
	"mixm_i2_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"        /* (wert) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (wert) */
		"  fadds volrl\n"            /* (vl') (wert) */
		"  fxch %%st(1)\n"           /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i2_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) */
		"fadds fadeleft\n"           /* (fl') (vl) */
		"fstps fadeleft\n"           /* (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_i2_ende\n"

	"mixm_i2_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixm_i2_loopme2:\n"
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i2_loopme2\n"
		"decl %%ecx\n"
		"jz mixm_i2_ende\n"
		"jmp mixm_i2_next\n"

		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_i2:\n"
	/* mixing, stereo w/ cubic interpolation */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"flds volr\n"                /* (vr) (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixs_i2_SM1+2\n"/* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword... see I care to do that */
	"mixs_i2_next:\n"
		"  flds (,%%ebp,4)\n"        /* (w0) (vr) (vl) */
		"  fmuls ct0(,%%eax,4)\n"    /* (w0') (vr) (vl) */
		"  flds 4(,%%ebp,4)\n"       /* (w1) (w0') (vr) (vl) */
		"  fmuls ct1(,%%eax,4)\n"    /* (w1') (w0') (vr) (vl) */
		"  flds 8(,%%ebp,4)\n"       /* (w2) (w1') (w0') (vr) (vl) */
		"  fmuls ct2(,%%eax,4)\n"    /* (w2') (w1') (w0') (vr) (vl) */
		"  flds 12(,%%ebp,4)\n"      /* (w3) (w2') (w1') (w0') (vr) (vl) */
		"  fmuls ct3(,%%eax,4)\n"    /* (w3') (w2') (w1') (w0') (vr) (vl) */
		"  fxch %%st(2)\n"           /* (w1') (w2') (w3') (w0') (vr) (vl) */
		"  faddp %%st, %%st(3)\n"    /* (w2') (w3') (w0+w1) (vr) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"    /* (w2+w3) (w0+w1) (vr) (vl)     I find this comment to be wrong, be the next addp merges them all together - Stian*/
		"  adcl %%ebx, %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"    /* wert) (vr) (vl) */
		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_i2_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_i2_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixs_i2_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr)  */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) */
		"  fxch %%st(1)\n"           /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_i2_next\n"
	"mixs_i2_ende:\n"
		"fstps volr\n"               /* (vl) */
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_i2_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"pushl %%eax\n"
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i2_loopme\n"
		"popl %%eax\n"
		"fxch %%st(1)\n"             /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_i2_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_i2_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"             /* (wert) (vl) (vr) */
		"fld %%st\n"                 /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"       /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"             /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"       /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"             /* (left) (rite) (vl) (vr) */
		"fadds fadeleft\n"           /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vl) (vr) */
		"fadds faderight\n"          /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft\n"           /* (fr') (vl) (vr) */
		"fstps faderight\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_i2_ende\n"

	"mixs_i2_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixs_i2_loopme2:\n"
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i2_loopme2\n"
		"decl %%ecx\n"
		"jz mixs_i2_ende\n"
		"jmp mixs_i2_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_nf:\n"
	/* mixing, mono w/o interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_nf_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		/* align dword sucks */
	"mixm_nf_next:\n"                    /* (vl) */
		"  flds (,%%ebp,4)\n"        /* (wert) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vl) */
	
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (left)  */
		"  fadds volrl\n"            /* (vl') (left) */
		"  fxch %%st(1)\n"           /* (left) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') */
	"mixm_nf_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_nf_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixm_nf_LoopHandler\n"
		"  fstps -4(%%edi)\n"        /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_nf_next\n"
	"mixm_nf_ende:\n"
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_nf_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_nf_loopme\n"
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) */
	"mixm_nf_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"        /* (wert) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (wert) */
		"  fadds volrl\n"            /* (vl') (wert) */
		"  fxch %%st(1)\n"           /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_nf_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) */
		"fadds fadeleft\n"           /* (fl') (vl) */
		"fstps fadeleft\n"           /* (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_nf_ende\n"

	"mixm_nf_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_nf_loopme\n"
		"decl %%ecx\n"
		"jz mixm_nf_ende\n"
		"jmp mixm_nf_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_nf:\n"
	/* mixing, stereo w/o interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"flds volr\n"                /* (vr) (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixs_nf_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
	/* align dword is for clows */
"mixs_nf_next:\n"
		"  flds (,%%ebp,4)\n"        /* (wert) (vr) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vr) (vl) */

		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_nf_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_nf_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixs_nf_LoopHandler\n"
       /* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) */
		"  fxch %%st(1)\n"           /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_nf_next\n"
	"mixs_nf_ende:\n"
		"fstps volr\n"               /* (vl) */
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_nf_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_nf_loopme\n"
		"fxch %%st(1)\n"             /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_nf_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_nf_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"             /* (wert) (vl) (vr) */
		"fld %%st\n"                 /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"       /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"             /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"       /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"             /* (left) (rite) (vl) (vr) */
		"fadds fadeleft\n"           /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vl) (vr) */
		"fadds faderight\n"          /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft\n"           /* (fr') (vl) (vr) */
		"fstps faderight\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_nf_ende\n"

	"mixs_nf_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_nf_loopme\n"
		"decl %%ecx\n"
		"jz mixs_nf_ende\n"
		"jmp mixs_nf_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_if:\n"
	/* mixing, mono+interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds minuseins\n"           /* (-1) */
		"flds voll\n"                /* (vl) (-1) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_if_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1\n"

	/* align dword is for pussies */
	"mixm_if_next:\n"                    /* (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"       /* (a) (vl) (-1) */
		"  fld %%st(0)\n"            /* (a) (a) (vl) (-1) */
		"  fld %%st(3)\n"            /* (-1) (a) (a) (vl) (-1) */
		"  fadds magic1\n"           /* (t) (a) (a) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (a) (t) (a) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"     /* (b-a) (t) (a) (vl) (-1) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st(1)\n"          /* ((b-a)*t) (a) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"          /* (wert) (vl) (-1) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vl) (-1) */

		"  fld %%st(1)\n"            /* (vl) (wert) (vl) (-1) */
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (vl) (left) (-1) */
		"  fadds volrl\n"            /* (vl') (left) (-1) */
		"  fxch %%st(1)\n"           /* (left) (vl) (-1) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixm_if_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_if_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  movl %%eax, magic1\n"
		"  jae mixm_if_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (-1) */
		"  decl %%ecx\n"
		"jnz mixm_if_next\n"
	"mixm_if_ende:\n"
		"fstps voll\n"               /* (whatever) */
		"fstp %%st\n"                /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_if_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (-1) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_if_loopme\n"
		"subl %%esi, %%edx\n"
		"sbb %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl)  (-1) */
	"mixm_if_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl)  (-1) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) (-1) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (wert) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (vl) (wert) (-1) */
		"  fadds volrl\n"            /* (vl') (wert) (-1) */
		"  fxch %%st(1)\n"           /* (wert) (vl') (-1) */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_if_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) (-1) */
		"fadds fadeleft\n"           /* (fl') (vl) (-1) */
		"fstps fadeleft\n"           /* (vl) (-1) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_if_ende\n"

	"mixm_if_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_if_loopme\n"
		"decl %%ecx\n"
		"jz mixm_if_ende\n"
		"jmp mixm_if_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_if:\n"
	/* mixing, stereo+interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds minuseins\n"           /* (-1) */
		"flds voll\n"                /* (vl) (-1) */
		"flds volr\n"                /* (vr) (vl) (-1) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixs_if_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $9, %%eax\n"
		"shrl $2, %%ebp\n"
		"orl $0x3f800000, %%eax\n"
		"movl %%eax, magic1\n"

	/* align dword is boring */
	"mixs_if_next:\n"                    /* (vr) (vl) (-1) */
		"  flds 0(,%%ebp,4)\n"       /* (a) (vr) (vl) (-1) */
		"  fld %%st(0)\n"            /* (a) (a) (vr) (vl) (-1) */
		"  fld %%st(4)\n"            /* (-1) (a) (a) (vr) (vl) (-1) */
		"  fadds magic1\n"           /* (t) (a) (a) (vr) (vl) (-1) */
		"  fxch %%st(1)\n"           /* (a) (t) (a) (vr) (vl) (-1) */
		"  fsubrs 4(,%%ebp,4)\n"     /* (b-a) (t) (a) (vr) (vl) (-1) */
		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  adcl %%ebx, %%ebp\n"
		"  fmulp %%st(1)\n"          /* ((b-a)*t) (a) (vr) (vl) (-1) */
		"  movl %%edx, %%eax\n"
		"  shrl $9, %%eax\n"
		"  faddp %%st(1)\n"          /* (wert) (vr) (vl) (-1) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vr) (vl) */

		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) (-1) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) (-1) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) (-1) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) (-1) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) (-1) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) (-1) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) (-1) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) (-1) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) (-1) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') (-1) */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') (-1) */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') (-1) */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') (-1) */
		"  orl $0x3f800000, %%eax\n"
	"mixs_if_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_if_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  movl %%eax, magic1\n"
		"  jae mixs_if_LoopHandler;\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) <vr'> (-1) */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr) (-1) */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) (-1) */
		"  fxch %%st(1)\n"           /* (vr) (vl) (-1) */
		"  decl %%ecx\n"
		"jnz mixs_if_next\n"
	"mixs_if_ende:\n"
		"fstps volr\n"
		"fstps voll\n"
		"fstp %%st\n"
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_if_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) (vr') (-1) */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) (-1) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) (-1) */
		"fxch %%st(1)\n"             /* (vr) (vl) (-1) */
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_if_loopme\n"
		"fxch %%st(2)\n"             /* (-1) (vl) (vr) */
		"fstp %%st\n"                /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_if_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_if_fill\n"
	/* update click-removal fade values */
		"fld %%st(2)\n"              /* (wert) (vr) (vl) (wert) */
		"fld %%st\n"                 /* (wert) (wert) (vr) (vl) (wert) */
		"fmul %%st(3), %%st\n"       /* (left) (wert) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (wert) (left) (vr) (vl) (wert) */
		"fmul %%st(2), %%st\n"       /* (rite) (left) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (left) (rite) (vr) (vl) (wert) */
		"fadds fadeleft\n"           /* (fl') (rite) (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vr) (vl) (wert) */
		"fadds faderight\n"          /* (fr') (fl') (vr) (vl) (wert) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vr) (vl) (wert) */
		"fstps fadeleft\n"           /* (fr') (vr) (vl) (wert) */
		"fstps faderight\n"          /* (vr) (vl) (wert) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_if_ende\n"

	"mixs_if_loopme:\n"
	/* sample loops -> jump to loop start */
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_if_loopme\n"
		"decl %%ecx\n"
		"jz mixs_if_ende\n"
		"jmp mixs_if_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixm_i2f:\n"
	/* mixing, mono w/ cubic interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixm_i2f_SM1+2\n" /* self modifying code. set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* align dword... how many times have we ignored this now? */
	"mixm_i2f_next:\n"                   /* (vl) */
		"  flds (,%%ebp,4)\n"        /* (w0) (vl) */
		"  fmuls ct0(,%%eax,4)\n"    /* (w0') (vl) */
		"  flds 4(,%%ebp,4)\n"       /* (w1) (w0') (vl) */
		"  fmuls ct1(,%%eax,4)\n"    /* (w1') (w0') (vl) */
		"  flds 8(,%%ebp,4)\n"       /* (w2) (w1') (w0') (vl) */
		"  fmuls ct2(,%%eax,4)\n"    /* (w2') (w1') (w0') (vl) */
		"  flds 12(,%%ebp,4)\n"      /* (w3) (w2') (w1') (w0') (vl) */
		"  fmuls ct3(,%%eax,4)\n"    /* (w3') (w2') (w1') (w0') (vl) */
		"  fxch %%st(2)\n"           /* (w1') (w2') (w3') (w0') (vl) */
		"  faddp %%st, %%st(3)\n"    /* (w2') (w3') (w0+w1) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 4(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"    /* (w2+w3) (w0+w1) (vl) */
		"  adcl %%ebx, %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"    /* (wert) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vr) (vl) */

		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmulp %%st, %%st(1)\n"    /* (left) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (left) */
		"  fadds volrl\n"            /* (vl') (left) */
		"  fxch %%st(1)\n"           /* (left) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (vl') */
	"mixm_i2f_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixm_i2f_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixm_i2f_LoopHandler\n"
		"  fstps -4(%%edi)\n"        /* (vl') */
		"  decl %%ecx\n"
		"jnz mixm_i2f_next\n"
	"mixm_i2f_ende:\n"
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixm_i2f_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') */
		"pushl %%eax\n"
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixm_i2f_loopme\n"
		"popl %%eax\n"
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) */
	"mixm_i2f_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (wert) (vl) */
		"  fmul %%st(1), %%st\n"     /* (left) (wert) (vl) */
		"  fadds -4(%%edi)\n"        /* (lfinal) (wert) (vl) */
		"  fstps -4(%%edi)\n"        /* (wert) (vl) */
		"  fxch %%st(1)\n"           /* (vl) (wert) */
		"  fadds volrl\n"            /* (vl') (wert) */
		"  fxch %%st(1)\n"           /* (wert) (vl') */
		"  leal 4(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"jnz mixm_i2f_fill\n"
	/* update click-removal fade values */
		"fmul %%st(1), %%st\n"       /* (left) (vl) */
		"fadds fadeleft\n"           /* (fl') (vl) */
		"fstps fadeleft\n"           /* (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixm_i2f_ende\n"

	"mixm_i2f_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixm_i2f_loopme2:\n"
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixm_i2f_loopme2\n"
		"decl %%ecx\n"
		"jz mixm_i2f_ende\n"
		"jmp mixm_i2f_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

	__asm__ __volatile__
	(
	"mixs_i2f:\n"
	/* mixing, stereo w/ cubic interpolation, FILTERED */
		"movl nsamples, %%ecx\n"
		"flds voll\n"                /* (vl) */
		"flds volr\n"                /* (vr) (vl) */
		"shrl $2, %%ebp\n"
#if 1
		"pushl %%ebp\n"
#else
		"movl %%ebp, mixs_i2f_SM1+2\n" /* self modifying code.  set loop end position */
#endif
		"movl %%eax, %%ebp\n"
		"shrl $2, %%ebp\n"
		"movl %%edx, %%eax\n"
		"shrl $24, %%eax\n"
	/* and again we ignore align dword */
	"mixs_i2f_next:\n"
		"  flds (,%%ebp,4)\n"        /* (w0) (vr) (vl) */
		"  fmuls ct0(,%%eax,4)\n"    /* (w0') (vr) (vl) */
		"  flds 4(,%%ebp,4)\n"       /* (w1) (w0') (vr) (vl) */
		"  fmuls ct1(,%%eax,4)\n"    /* (w1') (w0') (vr) (vl) */
		"  flds 8(,%%ebp,4)\n"       /* (w2) (w1') (w0') (vr) (vl) */
		"  fmuls ct2(,%%eax,4)\n"    /* (w2') (w1') (w0') (vr) (vl) */
		"  flds 12(,%%ebp,4)\n"      /* (w3) (w2') (w1') (w0') (vr) (vl) */
		"  fmuls ct3(,%%eax,4)\n"    /* (w3') (w2') (w1') (w0') (vr) (vl) */
		"  fxch %%st(2)\n"           /* (w1') (w2') (w3') (w0') (vr) (vl) */
		"  faddp %%st, %%st(3)\n"    /* (w2') (w3') (w0+w1) (vr) (vl) */
		"  addl %%esi, %%edx\n"
		"  leal 8(%%edi), %%edi\n"
		"  faddp %%st, %%st(2)\n"    /* (w2+w3) (w0+w1) (vr) (vl) */
		"  adcl %%ebx, %%ebp\n"
		"  movl %%edx, %%eax\n"
		"  faddp %%st, %%st(1)\n"    /* (wert) (vr) (vl) */

	/* FILTER HIER:
	 * b=reso*b+freq*(in-l);
	 * l+=freq*b;
	 */
		"  fsubs __fl1\n"            /* (in-l) .. */
		"  fmuls ffrq\n"             /* (f*(in-l)) .. */
		"  flds __fb1\n"             /* (b) (f*(in-l)) .. */
		"  fmuls frez\n"             /* (r*b) (f*(in-l)) .. */
		"  faddp %%st, %%st(1)\n"    /* (b') .. */
		"  fsts __fb1\n"
		"  fmuls ffrq\n"             /* (f*b') .. */
		"  fadds __fl1\n"            /* (l') .. */
		"  fsts __fl1\n"             /* (out) (vr) (vl) */

		"  shrl $24, %%eax\n"
		"  fld %%st(1)\n"            /* (vr) (wert) (vr) (vl) */
		"  fld %%st(3)\n"            /* (vl) (vr) (wert) (vr) (vl) */
		"  fmul %%st(2), %%st\n"     /* (left) (vr) (wert) (vr) (vl) */
		"  fxch %%st(4)\n"           /* (vl)  (vr) (wert) (vr) (left) */
		"  fadds volrl\n"            /* (vl') (vr) (wert) (vr) (left) */
		"  fxch %%st(2)\n"           /* (wert) (vr) (vl') (vr) (left) */
		"  fmulp %%st(1)\n"          /* (right) (vl') (vr) (left) */
		"  fxch %%st(2)\n"           /* (vr) (vl') (right) (left) */
		"  fadds volrr\n"            /* (vr') (vl') (right) (left) */
		"  fxch %%st(3)\n"           /* (left)  (vl') (right) (vr') */
		"  fadds -8(%%edi)\n"        /* (lfinal) (vl') <right> (vr') */
		"  fxch %%st(2)\n"           /* (right) (vl') (lfinal) (vr') */
		"  fadds -4(%%edi)\n"        /* (rfinal) (vl') (lfinal) (vr') */
	"mixs_i2f_looped:\n"
#if 1
		"  cmpl (%%esp), %%ebp\n"
#else
	"mixs_i2f_SM1:\n"
		"  cmpl $12345678, %%ebp\n"
#endif
		"  jae mixs_i2f_LoopHandler\n"
	/* hier 1 cycle frei */
		"  fstps -4(%%edi)\n"        /* (vl') (lfinal) (vr') */
		"  fxch %%st(1)\n"           /* (lfinal) (vl) (vr) */
		"  fstps -8(%%edi)\n"        /* (vl) (vr) */
		"  fxch %%st(1)\n"           /* (vr) (vl) */
		"  decl %%ecx\n"
		"jnz mixs_i2f_next\n"
	"mixs_i2f_ende:\n"
		"fstps volr\n"               /* (vl) */
		"fstps voll\n"               /* - */
		"shll $2, %%ebp\n"
		"movl %%ebp, %%eax\n"
#if 1
		"popl %%ecx\n" /* just a garbage register */
#endif
		"ret\n"

	"mixs_i2f_LoopHandler:\n"
		"fstps -4(%%edi)\n"          /* (vl') (lfinal) (vr') */
		"fxch %%st(1)\n"             /* (lfinal) (vl) (vr) */
		"fstps -8(%%edi)\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"pushl %%eax\n"
		"movl looptype, %%eax\n"
		"testl %0, %%eax\n"
		"jnz mixs_i2f_loopme\n"
		"popl %%eax\n"
		"fxch %%st(1)\n"             /* (vl) (vr) */
		"subl %%esi, %%edx\n"
		"sbbl %%ebx, %%ebp\n"
		"flds (,%%ebp,4)\n"          /* (wert) (vl) (vr) */
		"fxch %%st(2)\n"             /* (vr) (vl) (wert) */
	"mixs_i2f_fill:\n"
	/* sample ends -> fill rest of buffer with last sample value */
		"  fld %%st(1)\n"            /* (vl) (vr) (vl) (wert) */
		"  fmul %%st(3), %%st\n"     /* (left) (vr) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vr) (left) (vl) (wert) */
		"  fld %%st\n"               /* (vr) (vr) (left) (vl) (wert) */
		"  fmul %%st(4), %%st\n"     /* (right) (vr) (left) (vl) (wert) */
		"  fxch %%st(2)\n"           /* (left) (vr) (right) (vl) (wert) */
		"  fadds -8(%%edi)\n"
		"  fstps -8(%%edi)\n"        /* (vr) (right) (vl) (wert) */
		"  fxch %%st(1)\n"           /* (right) (vr) (vl) (wert) */
		"  fadds -4(%%edi)\n"
		"  fstps -4(%%edi)\n"        /* (vr) (vl) (wert) */
		"  fadds volrr\n"            /* (vr') (vl) (wert) */
		"  fxch %%st(1)\n"           /* (vl) (vr') (wert) */
		"  leal 8(%%edi), %%edi\n"
		"  decl %%ecx\n"
		"  fadds volrl\n"            /* (vl') (vr') (wert) */
		"  fxch %%st(1)\n"           /* (vr') (vl') (wert) */
		"jnz mixs_i2f_fill\n"
	/* update click-removal fade values */
		"fxch %%st(2)\n"             /* (wert) (vl) (vr) */
		"fld %%st\n"                 /* (wert) (wert) (vl) (vr) */
		"fmul %%st(2), %%st\n"       /* (left) (wert) (vl) (vr) */
		"fxch %%st(1)\n"             /* (wert) (left) (vl) (vr) */
		"fmul %%st(3), %%st\n"       /* (rite) (left) (vl) (vr) */
		"fxch %%st(1)\n"             /* (left) (rite) (vl) (vr) */
		"fadds fadeleft\n"           /* (fl') (rite) (vl) (vr) */
		"fxch %%st(1)\n"             /* (rite) (fl') (vl) (vr) */
		"fadds faderight\n"          /* (fr') (fl') (vl) (vr) */
		"fxch %%st(1)\n"             /* (fl') (fr') (vl) (vr) */
		"fstps fadeleft\n"           /* (fr') (vl) (vr) */
		"fstps faderight\n"          /* (vl) (vr) */
		"fxch %%st(1)\n"             /* (vr) (vl) */
		"movl looptype, %%eax\n"
		"andl %1, %%eax\n"
		"movl %%eax, looptype\n"
		"jmp mixs_i2f_ende\n"

	"mixs_i2f_loopme:\n"
	/* sample loops -> jump to loop start */
		"popl %%eax\n"
	"mixs_i2f_loopme2:\n"
		"subl mixlooplen, %%ebp\n"
		"  cmpl (%%esp), %%ebp\n"
		"jae mixs_i2f_loopme2\n"
		"decl %%ecx\n"
		"jz mixs_i2f_ende\n"
		"jmp mixs_i2f_next\n"
		:
		: "n"(MIXF_LOOPED),
		  "n"(FLAG_DISABLED)
	);

/*
 * clip routines:
 * esi : 32 bit float input buffer
 * edi : 8/16 bit output buffer
 * ecx : # of samples to clamp (*2 if stereo!)
 */
	__asm__ __volatile__
	(
	"clip_16s:\n"
	/* convert/clip samples, 16bit signed */

		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
		"movw $32767, %bx\n"
		"movw $-32768, %dx\n"

	"clip_16s_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_16s_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_16s_min\n"
		"  fistps (%edi)\n"          /* (max) (min) fi*s is 16bit */
	"clip_16s_next:\n"
		"  addl $4, %esi\n"
		"  addl $2, %edi\n"
		"  decl %ecx\n"
		"jnz clip_16s_lp\n"
		"jmp clip_16s_ende\n"

	"clip_16s_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bx, (%edi)\n"
		"jmp clip_16s_next\n"
	"clip_16s_min:\n"
		"fstp %st\n"
		"movw %dx, (%edi)\n"
		"jmp clip_16s_next\n"

	"clip_16s_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	);

	__asm__ __volatile__
	(
	"clip_16u:\n"
	/* convert/clip samples, 16bit unsigned */

		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
		"movw $32767, %bx\n"
		"movw $-32768, %dx\n"

	"clip_16u_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_16u_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_16u_min\n"
		"  fistps clipval\n"         /* (max) (min) */
		"  movw clipval, %ax\n"
	"clip_16u_next:\n"
		"  xorw $0x8000, %ax\n"
		"  movw %ax, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $2, %edi\n"
		"  decl %ecx\n"
		"jnz clip_16u_lp\n"
		"jmp clip_16u_ende\n"

	"clip_16u_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bx, %ax\n"
		"jmp clip_16u_next\n"
	"clip_16u_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_16u_next\n"

	"clip_16u_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	);

	__asm__ __volatile__
	(
	"clip_8s:\n"
	/* convert/clip samples, 8bit signed */
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
		"movw $32767, %bx\n"
		"movw $-32768, %dx\n"

	"clip_8s_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_8s_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_8s_min\n"
		"  fistps clipval\n"         /* (max) (min) */
		"  movw clipval, %ax\n"
	"clip_8s_next:\n"
		"  movb %ah, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $1, %edi\n"
		"  decl %ecx\n"
		"jnz clip_8s_lp\n"
		"jmp clip_8s_ende\n"

	"clip_8s_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bx, %ax\n"
		"jmp clip_8s_next\n"
	"clip_8s_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_8s_next\n"

	"clip_8s_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	);

	__asm__ __volatile__
	(
	"clip_8u:\n"
	/* convert/clip samples, 8bit unsigned */
		"flds clampmin\n"            /* (min) */
		"flds clampmax\n"            /* (max) (min) */
		"movw $32767, %bx\n"
		"movw $-32767, %dx\n"

	"clip_8u_lp:\n"
		"  flds (%esi)\n"            /* (ls) (max) (min) */
		"  fcom %st(1)\n"
		"  fnstsw %ax\n"
		"  sahf\n"
		"  ja clip_8u_max\n"
		"  fcom %st(2)\n"
		"  fstsw %ax\n"
		"  sahf\n"
		"  jb clip_8u_min\n"
		"  fistps clipval\n"         /* (max) (min) */
		"  movw clipval, %ax\n"
	"clip_8u_next:\n"
		"  xorw $0x8000, %ax\n"
		"  movb %ah, (%edi)\n"
		"  addl $4, %esi\n"
		"  addl $1, %edi\n"
		"  decl %ecx\n"
		"jnz clip_8u_lp\n"
		"jmp clip_8u_ende\n"

	"clip_8u_max:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %bx, %ax\n"
		"jmp clip_8u_next\n"
	"clip_8u_min:\n"
		"fstp %st\n"                 /* (max) (min) */
		"movw %dx, %ax\n"
		"jmp clip_8u_next\n"

	"clip_8u_ende:\n"
		"fstp %st\n"                 /* (min) */
		"fstp %st\n"                 /* - */
		"ret\n"
	);

	__asm__ __volatile__
	(
	".data\n"
	"clippers:\n"
		".long clip_8s, clip_8u, clip_16s, clip_16u\n"

	"mixers:\n"
	/* bit 0 stereo/mono output  (this needs to move when we add support for stereo input samples)
         * bit 1 interpolate
         * bit 2 interpolateq
         * bit 3 FILTER
         */
		".long mixm_n, mixs_n, mixm_i, mixs_i\n"     /* 0,0,0,0  0,0,0,1  0,0,1,0  0,0,1,1 */
		".long mixm_i2, mixs_i2, mix_0, mix_0\n"     /* 0,1,0,0  0,1,0,1  0,1,1,0  0,1,1,1 */
		".long mixm_n, mixs_nf, mixm_if, mixs_if\n"  /* 1,0,0,0  1,0,0,1  1,0,1,0  1,0,1,1 */
		".long mixm_i2f, mixs_i2f, mix_0, mix_0\n"   /* 1,1,0,0  1,1,0,1  1,1,1,0  1,1,1,1 */
	".previous\n"
	);
}

void getchanvol (int n, int len)
{
	int d0;
	__asm__ __volatile__
	(
	/* parm
	 * eax : channel #
	 * ecx : no of samples
	 */
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
		"pushl %%ebp\n"
		"fldz\n"                     /* (0) */
		"movl %%ecx, nsamples\n"

		"movl voiceflags(,%%eax,4), %%ebx\n"
		"testl %3, %%ebx\n"
		"jz getchanvol_SkipVoice\n"
		"movl looplen(,%%eax,4), %%ebx\n"
		"movl %%ebx, mixlooplen\n"
		"movl freqw(,%%eax,4), %%ebx\n"
		"movl freqf(,%%eax,4), %%esi\n"
		"movl smpposf(,%%eax,4), %%edx\n"
		"movl loopend(,%%eax,4), %%edi\n"
		"shrl $2, %%edi\n"
		"movl smpposw(,%%eax,4), %%ebp\n"
		"shrl $2, %%ebp\n"

	"getchanvol_next:\n"                 /* (sum) */
		"  flds (,%%ebp,4)\n"        /* (wert) (sum) */
		"  testl $0x80000000, (,%%ebp,4)\n"
		"  jnz getchanvol_neg\n"
		"  faddp %%st, %%st(1)\n"    /* (sum') */
		"  jmp getchanvol_goon\n"
	"getchanvol_neg:\n"
		"  fsubp %%st, %%st(1)\n"
	"getchanvol_goon:\n"
		"  addl %%esi, %%edx\n"
		"  adcl %%ebx, %%ebp\n"
	"getchanvol_looped:\n"
		"  cmpl %%edi, %%ebp\n"
		"  jae getchanvol_LoopHandler\n"
		"  decl %%ecx\n"
		"jnz getchanvol_next\n"
		"jmp getchanvol_SkipVoice\n"
	"getchanvol_LoopHandler:\n"
		"testl %2, voiceflags(,%%eax,4)\n"
		"jz getchanvol_SkipVoice\n"
		"subl looplen(,%%eax,4), %%ebp\n"
		"jmp getchanvol_looped\n"
	"getchanvol_SkipVoice:\n"
		"fidivl nsamples\n"          /* (sum'') */
		"fld %%st(0)\n"              /* (s) (s) */
		"fmuls volleft(,%%eax,4)\n"  /* (sl) (s) */
		"fstps voll\n"               /* (s) */
		"fmuls volright(,%%eax,4)\n" /* (sr) */
		"fstps volr\n"               /* - */

		"popl %%ebp\n"
#ifdef __PIC__
	 	"popl %%ebx\n"
#endif
		: "=&c" (d0)
		: "a" (n),
		  "n" (MIXF_LOOPED),
		  "n" (MIXF_PLAYING),
		  "0" (len)
#ifdef __PIC__
		: "edx", "edi", "esi"
#else
		: "ebx", "edx", "edi", "esi"
#endif
	);
}

void stop_dwmixfa(void)
{
}

#else

#ifdef I386_ASM_EMU

/*#define ASM_DEBUG 1*/
#ifdef ASM_DEBUG
#include <stdarg.h>
#include <stdio.h>
static void debug_printf(const char* format, ...)
{
        va_list args;

	fprintf(stderr, "[dwmixfa.c]: ");
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

}
#else
#define debug_printf(format, args...) ((void)0)
#endif


#define MAXVOICES MIXF_MAXCHAN
#define FLAG_DISABLED (~MIXF_PLAYING)
float   *tempbuf;               /* pointer to 32 bit mix buffer (nsamples * 4) */
void    *outbuf;                /* pointer to mixed buffer (nsamples * 2) */
uint32_t nsamples;              /* # of samples to mix */
uint32_t nvoices;               /* # of voices to mix */
uint32_t freqw[MAXVOICES];      /* frequency (whole part) */
uint32_t freqf[MAXVOICES];      /* frequency (fractional part) */
float   *smpposw[MAXVOICES];    /* sample position (whole part (pointer!)) */
uint32_t smpposf[MAXVOICES];    /* sample position (fractional part) */
float   *loopend[MAXVOICES];    /* pointer to loop end */
uint32_t looplen[MAXVOICES];    /* loop length in samples */
float    volleft[MAXVOICES];    /* float: left volume (1.0=normal) */
float    volright[MAXVOICES];   /* float: rite volume (1.0=normal) */
float    rampleft[MAXVOICES];   /* float: left volramp (dvol/sample) */
float    rampright[MAXVOICES];  /* float: rite volramp (dvol/sample) */
uint32_t voiceflags[MAXVOICES]; /* voice status flags */
float    ffreq[MAXVOICES];      /* filter frequency (0<=x<=1) */
float    freso[MAXVOICES];      /* filter resonance (0<=x<1) */
float    fadeleft=0.0;          /* 0 */
float    fl1[MAXVOICES];        /* filter lp buffer */
float    fb1[MAXVOICES];        /* filter bp buffer */
float    faderight=0.0;         /* 0 */
int      isstereo;              /* flag for stereo output */
int      outfmt;                /* output format */
float    voll=0.0;
float    volr=0.0;
float    ct0[256];              /* interpolation tab for s[-1] */
float    ct1[256];              /* interpolation tab for s[0] */
float    ct2[256];              /* interpolation tab for s[1] */
float    ct3[256];              /* interpolation tab for s[2] */
struct mixfpostprocregstruct *postprocs;
                                /* pointer to postproc list */
uint32_t samprate;              /* sampling rate */



static float volrl;
static float volrr;

static float eins=1.0;
static float minuseins=-1.0;
static float clampmax=32767.0;
static float clampmin=-32767.0;
static float __attribute__ ((used)) cremoveconst=0.992;
static float minampl=0.0001; /* what the fuck? why is this a float? - stian */
static uint32_t magic1;  /* 32bit in assembler used */
static uint16_t clipval; /* 16bit in assembler used */
static uint32_t mixlooplen; /* 32bit in assembler used, decimal. lenght of loop in samples*/
static uint32_t __attribute__ ((used)) looptype; /* 32bit in assembler used, local version of voiceflags[N] */
static float __attribute__ ((used)) ffrq;
static float __attribute__ ((used)) frez;
static float __attribute__ ((used)) __fl1;
static float __attribute__ ((used)) __fb1;


typedef void(*clippercall)(float *input, void *output, uint_fast32_t count);

static void clip_16s(float *input, void *output, uint_fast32_t count);
static void clip_16u(float *input, void *output, uint_fast32_t count);
static void clip_8s(float *input, void *output, uint_fast32_t count);
static void clip_8u(float *input, void *output, uint_fast32_t count);

static const clippercall clippers[4] = {clip_8s, clip_8u, clip_16s, clip_16u};

/* additional data come from globals:
	mixlooplen = length of sample loop  R
	volr                                R
	voll                                R
	fadeleft                            R
	faderight                           R
	looptype = sample flags             RW
*/	
typedef void(*mixercall)(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mix_0   (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixm_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);
static void mixs_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend);

static const mixercall mixers[16] = {
	mixm_n,   mixs_n,   mixm_i,  mixs_i,
	mixm_i2,  mixs_i2,  mix_0,   mix_0,
	mixm_nf,  mixs_nf,  mixm_if, mixs_if,
	mixm_i2f, mixs_i2f, mix_0,   mix_0
};

static void writecallback(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
}

static uint_fast32_t readcallback(uint_fast16_t selector, uint_fast32_t addr, int size)
{
	return 0;
}

void start_dwmixfa(void)
{
	volrl=volrl;
	volrr=volrr;
	eins=eins;
	minuseins=minuseins;
	clampmin=clampmin;
	clampmax=clampmax;
	cremoveconst=cremoveconst;
	minampl=minampl;
	magic1=magic1;
	clipval=clipval;
	mixlooplen=mixlooplen;
	looptype=looptype;
	ffrq=ffrq;
	frez=frez;
	__fl1=__fl1;
	__fb1=__fb1;

}

void prepare_mixer (void)
{
	struct assembler_state_t state;

	init_assembler_state(&state, writecallback, readcallback);
	asm_xorl(&state, state.eax, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&fadeleft);
	asm_movl(&state, state.eax, (uint32_t *)&faderight);
	asm_movl(&state, state.eax, (uint32_t *)&volrl);
	asm_movl(&state, state.eax, (uint32_t *)&volrr);
	asm_xorl(&state, state.ecx, &state.ecx);
prepare_mixer_fillloop:
	asm_movl(&state, state.eax, (uint32_t *)&volleft[state.ecx]);
	asm_movl(&state, state.eax, (uint32_t *)&volright[state.ecx]);
	asm_incl(&state, &state.ecx);
	asm_cmpl(&state, MAXVOICES, state.ecx);
	asm_jne(&state, prepare_mixer_fillloop);
}

static inline void clearbufm(float **edi_buffer, uint32_t *count)
{
	struct assembler_state_t state;

	debug_printf("clearbufm {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, 0x12345678/**edi_buffer*/, &state.edi);
	asm_movl(&state, *count, &state.ecx);
	
	asm_flds(&state, cremoveconst);
	asm_flds(&state, fadeleft);
clearbufm_clloop:
		asm_fsts(&state, *edi_buffer+0);
		asm_fmul(&state, 1, 0);
		asm_leal(&state, state.edi+4, &state.edi); *edi_buffer+=1;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, clearbufm_clloop);

	asm_fstps(&state, &fadeleft);
	asm_fstp_st(&state, 0);

	asm_movl(&state, state.ecx, count);
	debug_printf("}\n");
}

static inline void clearbufs(float **edi_buffer, uint32_t *count)
{
	struct assembler_state_t state;

	debug_printf("clearbufs {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, 0x12345678/**edi_buffer*/, &state.edi);
	asm_movl(&state, *count, &state.ecx);

	asm_flds(&state, cremoveconst);
	asm_flds(&state, faderight);
	asm_flds(&state, fadeleft);
clearbufs_clloop:
		asm_fsts(&state, *edi_buffer+0);
		asm_fmul(&state, 2, 0);
		asm_fxch_st(&state, 1);
		asm_fsts(&state, *edi_buffer+1);
		asm_fmul(&state, 2, 0);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); *edi_buffer+=2;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, clearbufs_clloop);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_fstp_st(&state, 0);

	asm_movl(&state, state.ecx, count);

	debug_printf("}\n");
}


void mixer (void)
{
	struct assembler_state_t state;
	float *edi_mirror;
	void *edi_mirror2;
	float *esi_mirror2;
	float *eax_mirror;
	float *ebp_mirror;
	mixercall ecx_mirror;
	clippercall eax_mirror2;
	struct mixfpostprocregstruct *esi_mirror;

	init_assembler_state(&state, writecallback, readcallback);

	debug_printf("mixer {\n");

	asm_pushl(&state, state.ebp);
	asm_finit(&state);
	asm_xorl(&state, state.ebx, &state.ebx);
	asm_movl(&state, *(uint32_t *)&fadeleft, &state.eax);
	asm_andl(&state, 0x7fffffff, &state.eax);
	asm_cmpl(&state, state.eax, minampl); /* TODO, comparing of floats, typecasted to uint32_t */
	asm_ja(&state, mixer_nocutfl);
	asm_movl(&state, state.ebx, (uint32_t *)&fadeleft); /* mixing of float and integer numbers.... "great" */
mixer_nocutfl:
	asm_movl(&state, *(uint32_t *)&faderight, &state.eax);
	asm_andl(&state, 0x7fffffff, &state.eax);
	asm_cmpl(&state, state.eax, minampl); /* TODO, comparing of floats, typecasted to uint32_t */
	asm_ja(&state, mixer_nocutfr);
	asm_movl(&state, state.ebx, (uint32_t *)&faderight); /* mixing of float and integer numbers.... "great" */
mixer_nocutfr:
	asm_movl(&state, 0x12345678/*tempbuf*/, &state.edi); edi_mirror = tempbuf;
	asm_movl(&state, nsamples, &state.ecx);
	asm_orl(&state, state.ecx, &state.ecx);
	asm_jz(&state, mixer_endall);
	asm_movl(&state, isstereo, &state.eax);
	asm_orl(&state, state.eax, &state.eax);
	asm_jnz(&state, mixer_clearst);
		clearbufm(&edi_mirror, &state.ecx);
	asm_jmp(&state, mixer_clearend);
mixer_clearst:
		clearbufs(&edi_mirror, &state.ecx);
mixer_clearend:
	asm_movl(&state, nvoices, &state.ecx);
	asm_decl(&state, &state.ecx);

mixer_MixNext:
	debug_printf("Doing channel: %d\n", state.ecx);
	asm_movl(&state, voiceflags[state.ecx], &state.eax);
	asm_testl(&state, MIXF_PLAYING, state.eax);
	asm_jz(&state, mixer_SkipVoice);
 
	asm_movl(&state, state.eax, &looptype); 

	asm_movl(&state, *(uint32_t *)&volleft[state.ecx], &state.eax);
	asm_movl(&state, *(uint32_t *)&volright[state.ecx], &state.ebx);
	asm_movl(&state, state.eax, (uint32_t *)&voll);
	asm_movl(&state, state.ebx, (uint32_t *)&volr);

	asm_movl(&state, *(uint32_t *)&rampleft[state.ecx], &state.eax);
	asm_movl(&state, *(uint32_t *)&rampright[state.ecx], &state.ebx);
	asm_movl(&state, state.eax, (uint32_t *)&volrl);
	asm_movl(&state, state.ebx, (uint32_t *)&volrr);

	asm_movl(&state, *(uint32_t *)&ffreq[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&ffrq);
	asm_movl(&state, *(uint32_t *)&freso[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&frez);
	asm_movl(&state, *(uint32_t *)&fl1[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&__fl1);
	asm_movl(&state, *(uint32_t *)&fb1[state.ecx], &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&__fb1);

	asm_movl(&state, looplen[state.ecx], &state.eax);
	asm_movl(&state, state.eax, &mixlooplen);

	asm_movl(&state, freqw[state.ecx], &state.ebx);
	asm_movl(&state, freqf[state.ecx], &state.esi); 

	asm_movl(&state, 0x12345678, &state.eax); eax_mirror = smpposw[state.ecx];

	asm_movl(&state, smpposf[state.ecx], &state.edx);

	asm_movl(&state, 0x12345678, &state.ebp); ebp_mirror = loopend[state.ecx];

	asm_pushl(&state, state.ecx);
	asm_movl(&state, 0x12345678, &state.edi); edi_mirror = tempbuf;
	asm_movl(&state, isstereo, &state.ecx);
	asm_orl(&state, voiceflags[state.ecx], &state.ecx);
	asm_andl(&state, 15, &state.ecx);
	/*asm_movl(&state, 0x12345678, &state.ecx);*/ ecx_mirror = mixers[state.ecx];
		ecx_mirror(edi_mirror, &eax_mirror, &state.edx, state.ebx, state.esi, ebp_mirror);
	asm_popl(&state, &state.ecx);
/*	asm_movl(&state, eax, smposw[state.ecx]);*/smpposw[state.ecx] = eax_mirror;
	asm_movl(&state, state.edx, &smpposf[state.ecx]);

	asm_movl(&state, looptype, &state.eax);
	asm_movl(&state, state.eax, &voiceflags[state.ecx]);

	/* update volumes */
	asm_movl(&state, *(uint32_t *)&voll, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&volleft[state.ecx]);
	asm_movl(&state, *(uint32_t *)&volr, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&volright[state.ecx]);

	asm_movl(&state, *(uint32_t *)&__fl1, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&fl1[state.ecx]);
	asm_movl(&state, *(uint32_t *)&__fb1, &state.eax);
	asm_movl(&state, state.eax, (uint32_t *)&fb1[state.ecx]);

mixer_SkipVoice:
		asm_decl(&state, &state.ecx);
	asm_jns(&state, mixer_MixNext);

	asm_movl(&state, 0x12345678 /*postprocs	*/, &state.esi); esi_mirror = postprocs;
mixer_PostprocLoop:
/*	asm_orl(&state, state.esi, state.esi);*/ write_zf(state.eflags, !esi_mirror);
	asm_jz(&state, mixer_PostprocEnd);
	asm_movl(&state, nsamples, &state.edx);
	asm_movl(&state, isstereo, &state.ecx);
	asm_movl(&state, samprate, &state.ebx);
	asm_movl(&state, 0x12345678, &state.eax); eax_mirror = tempbuf;
	/* call *state.esi*/ esi_mirror->Process(eax_mirror, state.edx, state.ebx, state.ecx);
	asm_movl(&state, state.esi+12, &state.esi); esi_mirror = esi_mirror->next;
	
	asm_jmp(&state, mixer_PostprocLoop);

mixer_PostprocEnd:

	asm_movl(&state, outfmt, &state.eax);
/*	{
		int i;
		for (i=0;i<nsamples;i++)
		{
			fprintf(stderr, "%f\n", tempbuf[i]);
			if (i==8)
				break;
		}
	}
*/
	/*asm_movl(&state, clippers[state.eax], &state.eax);*/ eax_mirror2 = clippers[state.eax];

	asm_movl(&state, 0x12345678/*outbuf*/, &state.edi); edi_mirror2 = outbuf;
	asm_movl(&state, 0x12345678/*tempbuf*/, &state.esi); esi_mirror2 = tempbuf;
	asm_movl(&state, nsamples, &state.ecx);

	asm_movl(&state, isstereo, &state.edx);
	asm_orl(&state, state.edx, &state.edx);
	asm_jz(&state, mixer_clipmono);
	asm_addl(&state, state.ecx, &state.ecx);
mixer_clipmono:
	/* call *state.eax*/ eax_mirror2(esi_mirror2, edi_mirror2, state.ecx);

mixer_endall:
	asm_popl(&state, &state.ebp);

	debug_printf("}\n");

}





















static void mix_0   (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mix_0 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mix_0_next:
		asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
		asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
mix_0_looped:
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mix_0_LoopHandler);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mix_0_next);
mix_0_ende:
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);


	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mix_0_LoopHandler:
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mix_0_loopme);
	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mix_0_ende);
mix_0_loopme:
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	asm_jmp(&state, mix_0_looped);
}

static void mixm_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_n {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixm_n_next:
	asm_flds(&state, *ebp_mirror);
	asm_fld(&state, 1);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_n_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_n_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_n_next);
mixm_n_ende:
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("mixer }\n");
	return;

mixm_n_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_n_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_n_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_n_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_n_ende);

mixm_n_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_n_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_n_ende);
	asm_jmp(&state, mixm_n_next);
}

static void mixs_n  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_n {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_flds(&state, volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixs_n_next:
	asm_flds(&state, *ebp_mirror);
	asm_addl(&state, state.esi, &state.edx);if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fld(&state, 1);
	asm_fld(&state, 3);	
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_st(&state, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);

/*mixs_n_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_n_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_n_next);
mixs_n_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("mixer }\n");
	return;

mixs_n_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_n_loopme);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);

	asm_fxch_st(&state, 2);
mixs_n_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_n_fill);
	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_fxch_st(&state, 1);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_n_ende);

mixs_n_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_n_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_n_ende);
	asm_jmp(&state, mixs_n_next);
}

static void mixm_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, &magic1);
mixm_i_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 3);
	asm_fadds(&state, *(float *)&magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixm_i_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, &magic1);
		asm_jae(&state, mixm_i_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i_next);
mixm_i_ende:
	asm_fstps(&state, &voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_i_ende);

mixm_i_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i_ende);
	asm_jmp(&state, mixm_i_next);
}

static void mixs_i  (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, voll);
	asm_flds(&state, volr);
	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);



	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, &magic1);


mixs_i_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 4);
	asm_fadds(&state, *(float *)&magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixs_i_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, &magic1);
		asm_jae(&state, mixs_i_LoopHandler);

		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i_next);
mixs_i_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;

	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1); 
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i_loopme);
	asm_fxch_st(&state, 2);
	asm_fstp_st(&state, 0);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i_fill);

	asm_fld(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_i_ende);

mixs_i_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i_ende);
	asm_jmp(&state, mixs_i_next);
}

static void mixm_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i2 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);
mixm_i2_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_i2_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2_next);
mixm_i2_ende:
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i2_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i2_loopme);
	asm_popl(&state, &state.eax);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i2_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_i2_ende);

mixm_i2_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i2_ende);
	asm_jmp(&state, mixm_i2_next);
}

static void mixs_i2 (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i2 {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_flds(&state, volr);

	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);


	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);

mixs_i2_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);
	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_i2_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2_LoopHandler);

		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i2_next);
mixs_i2_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i2_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i2_loopme);
	asm_popl(&state, &state.eax);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i2_fill: /*  sample ends -> fill rest of buffer with last sample value */

		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i2_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_i2_ende);

mixs_i2_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i2_ende);
	asm_jmp(&state, mixs_i2_next);
}

static void mixm_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_nf {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixm_nf_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_fld(&state, 1);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*ixm_nf_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_nf_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_nf_next);
mixm_nf_ende:
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_nf_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_nf_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_nf_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_nf_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_nf_ende);

mixm_nf_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_nf_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_nf_ende);
	asm_jmp(&state, mixm_nf_next);
}

static void mixs_nf (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_nf {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_flds(&state, volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
mixs_nf_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_nf_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_nf_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_nf_next);
mixs_nf_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_nf_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_stst(&state, 0, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_stst(&state, 0, 1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_nf_loopme);
	asm_fxch_stst(&state, 0, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_stst(&state, 0, 2);
mixs_nf_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_nf_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_nf_ende);

mixs_nf_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_nf_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_nf_ende);
	asm_jmp(&state, mixs_nf_next);
}

static void mixm_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_if {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, &magic1);
mixm_if_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 3);
	asm_fadds(&state, *(float *)&magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	



	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixm_if_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, &magic1);
		asm_jae(&state, mixm_if_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_if_next);
mixm_if_ende:
	asm_fstps(&state, &voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_if_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_if_loopme);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_if_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_if_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_if_ende);

mixm_if_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_if_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_if_ende);
	asm_jmp(&state, mixm_if_next);
}

static void mixs_if (float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_if {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, minuseins);
	asm_flds(&state, voll);
	asm_flds(&state, volr);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_shrl(&state, 2, &state.ebp);
	asm_orl(&state, 0x3f800000, &state.eax);
	asm_movl(&state, state.eax, &magic1);
mixs_if_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fld(&state, 0);
	asm_fld(&state, 4);
	asm_fadds(&state, *(float *)&magic1);
	asm_fxch_st(&state, 1);
	asm_fsubrs(&state, ebp_mirror[1]);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_fmulp_st(&state, 1);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 9, &state.eax);
	asm_faddp_stst(&state, 0, 1);

	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
	asm_orl(&state, 0x3f800000, &state.eax);
/*mixs_if_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_movl(&state, state.eax, &magic1);
		asm_jae(&state, mixs_if_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_if_next);
mixs_if_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_fstp_st(&state, 0);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_if_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1); 
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_if_loopme);
	asm_fxch_st(&state, 2);
	asm_fstp_st(&state, 0);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_if_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-2);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_if_fill);
	/*asm_fmul(&state, 1, 0);*/
	asm_fld(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_if_ende);

mixs_if_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_if_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_if_ende);
	asm_jmp(&state, mixs_if_next);
}

static void mixm_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixm_i2f {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_shrl(&state, 2, &state.ebp);
	asm_pushl(&state, state.ebp);
	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);
mixm_i2f_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);





	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, edi_destptr[-1]);
/*mixm_i2f_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2f_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2f_next);
mixm_i2f_ende:
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixm_i2f_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixm_i2f_loopme);
	asm_popl(&state, &state.eax);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
mixm_i2f_fill: /*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 1, 0);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+4, &state.edi); edi_destptr++;
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixm_i2f_fill);
	asm_fmul(&state, 1, 0);
	asm_fadds(&state, fadeleft);
	asm_fstps(&state, &fadeleft);

	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixm_i2f_ende);

mixm_i2f_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixm_i2f_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixm_i2f_ende);
	asm_jmp(&state, mixm_i2f_next);
}

static void mixs_i2f(float *edi_destptr, float **eax_sample_pos, uint32_t *edx_sample_pos_fract, uint32_t ebx_sample_pitch, uint32_t esi_sample_pitch_fract, float *ebp_loopend)
{
	struct assembler_state_t state;
	float *ebp_mirror;

	debug_printf("mixs_i2f {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*edi_destptr*/ 0x12345678, &state.edi);
	asm_movl(&state, /*eax_sample_pos*/0x12345678, &state.eax);
	asm_movl(&state, *edx_sample_pos_fract, &state.edx);
	asm_movl(&state, ebx_sample_pitch, &state.ebx);
	asm_movl(&state, esi_sample_pitch_fract, &state.esi);
	asm_movl(&state, /*ebp_loopend*/0x12345678, &state.ebp);


	asm_movl(&state, nsamples, &state.ecx);
	asm_flds(&state, voll);
	asm_flds(&state, volr);
	asm_shrl(&state, 2, &state.ebp);

	asm_pushl(&state, state.ebp);



	asm_movl(&state, state.eax, &state.ebp); ebp_mirror = *eax_sample_pos;
	asm_shrl(&state, 2, &state.ebp);
	asm_movl(&state, state.edx, &state.eax);
	asm_shrl(&state, 24, &state.eax);

mixs_i2f_next:
	asm_flds(&state, ebp_mirror[0]);
	asm_fmuls(&state, ct0[state.eax]);
	asm_flds(&state, ebp_mirror[1]);
	asm_fmuls(&state, ct1[state.eax]);
	asm_flds(&state, ebp_mirror[2]);
	asm_fmuls(&state, ct2[state.eax]);
	asm_flds(&state, ebp_mirror[3]);
	asm_fmuls(&state, ct3[state.eax]);
	asm_fxch_st(&state, 2);
	asm_faddp_stst(&state, 0, 3);
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
	asm_faddp_stst(&state, 0, 2);
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
	asm_movl(&state, state.edx, &state.eax);
	asm_faddp_stst(&state, 0, 1);





	asm_fsubs(&state, __fl1);
	asm_fmuls(&state, ffrq);
	asm_flds(&state, __fb1);
	asm_fmuls(&state, frez);
	asm_faddp_stst(&state, 0, 1);
	asm_fsts(&state, &__fb1);
	asm_fmuls(&state, ffrq);
	asm_fadds(&state, __fl1);
	asm_fsts(&state, &__fl1);

	asm_shrl(&state, 24, &state.eax);
	asm_fld(&state, 1);
	asm_fld(&state, 3);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 4);
	asm_fadds(&state, volrl);
	asm_fxch_st(&state, 2);
	asm_fmulp_stst(&state, 0, 1);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, volrr);
	asm_fxch_st(&state, 3);
	asm_fadds(&state, edi_destptr[-2]);
	asm_fxch_st(&state, 2);
	asm_fadds(&state, edi_destptr[-1]);
/*mixs_i2f_looped:*/
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2f_LoopHandler);
		asm_fstps(&state, edi_destptr-1);
		asm_fxch_st(&state, 1);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_decl(&state, &state.ecx);
	asm_jnz(&state, mixs_i2f_next);
mixs_i2f_ende:
	asm_fstps(&state, &volr);
	asm_fstps(&state, &voll);
	asm_shll(&state, 2, &state.ebp);
	asm_movl(&state, state.ebp, &state.eax); *eax_sample_pos = ebp_mirror;
	asm_popl(&state, &state.ecx);

	asm_movl(&state, state.edx, edx_sample_pos_fract);
	debug_printf("}\n");
	return;

mixs_i2f_LoopHandler:
	asm_fstps(&state, edi_destptr-1);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, edi_destptr-2);
	asm_fxch_st(&state, 1);
	asm_pushl(&state, state.eax);
	asm_movl(&state, looptype, &state.eax);
	asm_testl(&state, MIXF_LOOPED, state.eax);
	asm_jnz(&state, mixs_i2f_loopme);
	asm_popl(&state, &state.eax);
	asm_fxch_st(&state, 1);
	asm_subl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror--;
	asm_sbbl(&state, state.ebx, &state.ebp); ebp_mirror -= state.ebx;
	asm_flds(&state, *ebp_mirror);
	asm_fxch_st(&state, 2);
mixs_i2f_fill:
	/*  sample ends -> fill rest of buffer with last sample value */
		asm_fld(&state, 1);
		asm_fmul(&state, 3, 0);
		asm_fxch_st(&state, 1);
		asm_fld(&state, 0);
		asm_fmul(&state, 4, 0);
		asm_fxch_st(&state, 2);
		asm_fadds(&state, edi_destptr[-2]);
		asm_fstps(&state, edi_destptr-2);
		asm_fxch_st(&state, 1);
		asm_fadds(&state, edi_destptr[-1]);
		asm_fstps(&state, edi_destptr-1);
		asm_fadds(&state, volrr);
		asm_fxch_st(&state, 1);
		asm_leal(&state, state.edi+8, &state.edi); edi_destptr+=2;
		asm_decl(&state, &state.ecx);
		asm_fadds(&state, volrl);
		asm_fxch_st(&state, 1);
	asm_jnz(&state, mixs_i2f_fill);

	asm_fxch_st(&state, 2);
	asm_fld(&state, 0);
	asm_fmul(&state, 2, 0);
	asm_fxch_st(&state, 1);
	asm_fmul(&state, 3, 0);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, fadeleft);
	asm_fxch_st(&state, 1);
	asm_fadds(&state, faderight);
	asm_fxch_st(&state, 1);
	asm_fstps(&state, &fadeleft);
	asm_fstps(&state, &faderight);
	asm_fxch_st(&state, 1);
	asm_movl(&state, looptype, &state.eax); /* NOT NEEDED */
	asm_andl(&state, FLAG_DISABLED, &state.eax);
	asm_movl(&state, state.eax, &looptype);
	asm_jmp(&state, mixs_i2f_ende);

mixs_i2f_loopme: /* sample loops -> jump to loop start */
	asm_subl(&state, mixlooplen, &state.ebp); ebp_mirror -= mixlooplen;
	/* asm_cmpl(&state, (%esp) has ebp_loopend, &state.ebp);*/
	if (ebp_loopend == ebp_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror < ebp_loopend)
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
		asm_jae(&state, mixs_i2f_loopme);
	asm_decl(&state, &state.ecx);
	asm_jz(&state, mixs_i2f_ende);
	asm_jmp(&state, mixs_i2f_next);
}

static void clip_16s(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint16_t *edi_mirror;

	debug_printf("clip_16s {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);
	
clip_16s_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_16s_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_16s_min);
	asm_fistps(&state, edi_mirror);
clip_16s_next:
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 2, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_16s_lp);
	asm_jmp(&state, clip_16s_ende);
clip_16s_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, edi_mirror);
	asm_jmp(&state, clip_16s_next);

clip_16s_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, edi_mirror);
	asm_jmp(&state, clip_16s_next);

clip_16s_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_16u(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint16_t *edi_mirror;

	debug_printf("clip_16u {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);
	
clip_16u_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_16u_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_16u_min);
	asm_fistps(&state, &clipval);
	asm_movw(&state, clipval, &state.ax);
clip_16u_next:
	asm_xorw(&state, 0x8000, &state.ax);
	asm_movw(&state, state.ax, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 2, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_16u_lp);
	asm_jmp(&state, clip_16u_ende);
clip_16u_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_16u_next);

clip_16u_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_16u_next);

clip_16u_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_8s(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint8_t *edi_mirror;

	debug_printf("clip_8s {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);
	
clip_8s_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_8s_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_8s_min);
	asm_fistps(&state, &clipval);
	asm_movw(&state, clipval, &state.ax);
clip_8s_next:
	asm_movb(&state, state.ah, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 1, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_8s_lp);
	asm_jmp(&state, clip_8s_ende);
clip_8s_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_8s_next);

clip_8s_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, &state.ax);
	asm_jmp(&state, clip_8s_next);

clip_8s_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

static void clip_8u(float *input, void *output, uint_fast32_t count)
{
	struct assembler_state_t state;
	float *esi_mirror;
	uint8_t *edi_mirror;

	debug_printf("clip_8u {\n");

	init_assembler_state(&state, writecallback, readcallback);
	asm_movl(&state, /*input*/ 0x12345678, &state.esi); esi_mirror = input;
	asm_movl(&state, /*_output*/0x87654321, &state.edi); edi_mirror = output;
	asm_movl(&state, count, &state.ecx);

	asm_flds(&state, clampmin);
	asm_flds(&state, clampmax);
	asm_movw(&state, 32767, &state.bx);
	asm_movw(&state, -32768, &state.dx);
	
clip_8u_lp:
	asm_flds(&state, *esi_mirror);
	asm_fcom_st(&state, 1);
	asm_fnstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_ja(&state, clip_8u_max);
	asm_fcom_st(&state, 2);
	asm_fstsw(&state, &state.ax);
	asm_sahf(&state);
	asm_jb(&state, clip_8u_min);
	asm_fistps(&state, &clipval);
	asm_movw(&state, clipval, &state.ax);
clip_8u_next:
	asm_xorw(&state, 0x8000, &state.ax);
	asm_movb(&state, state.ah, edi_mirror);
	asm_addl(&state, 4, &state.esi); esi_mirror++;
	asm_addl(&state, 1, &state.edi); edi_mirror++;
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, clip_8u_lp);
	asm_jmp(&state, clip_8u_ende);
clip_8u_max:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.bx, &state.ax);
	asm_jmp(&state, clip_8u_next);

clip_8u_min:
	asm_fstp_st(&state, 0);
	asm_movw(&state, state.dx, &state.ax);
	asm_jmp(&state, clip_8u_next);

clip_8u_ende:
	asm_fstp_st(&state, 0);
	asm_fstp_st(&state, 0);
	debug_printf("}\n");
}

void getchanvol (int n, int len)
{
	struct assembler_state_t state;

	float *ebp_mirror;
	float *edi_mirror;

	debug_printf("getchanvol {\n");

	init_assembler_state(&state, writecallback, readcallback);

	state.ecx = len; /* assembler entry config */

	asm_pushl(&state, state.ebp);
	asm_fldz(&state);
	asm_movl(&state, state.ecx, &nsamples);

	asm_movl(&state, voiceflags[state.eax], &state.ebx);
	asm_testl(&state, MIXF_PLAYING, state.ebx);
	asm_jz(&state, getchanvol_SkipVoice);
	asm_movl(&state, looplen[state.eax], &state.ebx);
	asm_movl(&state, state.ebx, &mixlooplen);
	asm_movl(&state, freqw[state.eax], &state.ebx);
	asm_movl(&state, freqf[state.eax], &state.esi);
	asm_movl(&state, smpposf[state.eax], &state.edx);
	asm_movl(&state, /*loopend[state.eax]*/0x12345678, &state.edi); edi_mirror = loopend[state.eax];
	asm_shrl(&state, 2, &state.edi); /* this is fucked up logic :-p */
	asm_movl(&state, /*smpposw[state.eax]*/0x87654321, &state.ebp); ebp_mirror = smpposw[state.eax];
	asm_shrl(&state, 2, &state.ebp); /* this is fucked up logic :-p */
/*getchanvol_next:*/
	asm_flds(&state, *ebp_mirror); /* (,%ebp,4)*/
	asm_testl(&state, 0x80000000, *(uint32_t *)ebp_mirror); /* sign og *ebp_mirror */
	asm_jnz(&state, getchanvol_neg);
	asm_faddp_stst(&state, 0, 1);
	asm_jmp(&state, getchanvol_goon);
getchanvol_neg:
	asm_fsubp_stst(&state, 0, 1);
getchanvol_goon:
	asm_addl(&state, state.esi, &state.edx); if (read_cf(state.eflags)) ebp_mirror++;
	asm_adcl(&state, state.ebx, &state.ebp); ebp_mirror += state.ebx;
getchanvol_looped:
/*	asm_cmpl(&state, state.edi, state.ebp);*/
	if (ebp_mirror == edi_mirror)
	{
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 1);
	} else if (ebp_mirror>edi_mirror) /* pos > loopend */
	{
		write_cf(state.eflags, 1);
		write_zf(state.eflags, 0);
	} else {
		write_cf(state.eflags, 0);
		write_zf(state.eflags, 0);
	}
	asm_decl(&state, &state.ecx);
	asm_jnz(&state, getchanvol_LoopHandler);
	asm_jmp(&state, getchanvol_SkipVoice);
getchanvol_LoopHandler:
	asm_testl(&state, MIXF_LOOPED, voiceflags[state.eax]);
	asm_jz(&state, getchanvol_SkipVoice);
	asm_subl(&state, looplen[state.eax], &state.ebp); ebp_mirror -= looplen[state.eax];
	asm_jmp(&state, getchanvol_looped);	
getchanvol_SkipVoice:
	asm_fidivl(&state, nsamples);
	asm_fldx(&state, read_fpu_st(&state, 0));
	asm_fmuls(&state, volleft[state.eax]);
	asm_fstps(&state, &voll);
	asm_fmuls(&state, volright[state.eax]);
	asm_fstps(&state, &volr);

	asm_popl(&state, &state.ebp);
	debug_printf("}\n");
}

#else

#warning We need a C version

#endif
#endif
