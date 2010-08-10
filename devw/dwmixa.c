/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * assembler routines for 8-bit MCP mixer
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
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -ported to gcc
 *  -ss040908   Stian Skjelstad <stian@nixia.no>
 *    -made it optimizesafe
 */

#include "config.h"
#include "types.h"
#include "dwmix.h"
#include "dwmixa.h"

#include "dev/mix.h"

#ifndef I386_ASM
#ifdef I386_ASM_EMU
#include "asm_emu/x86.h"
#endif
#endif

#ifdef I386_ASM

#ifndef NULL
#define NULL ((void *)0)
#endif

void remap_range1_start(void){}

void nonepublic_dwmixa3(void)
{
	__asm__ __volatile__
	(
		"mixrFadeChannel_:\n"
		"  movl %0(%%edi), %%ebx\n"       /*  %0 = curvol[0] */
		"  movl %1(%%edi), %%ecx\n"       /*  %1 = curvol[1] */
		"  shll $8, %%ebx\n"
		"  shll $8, %%ecx\n"
		"  movl %2(%%edi),%%eax\n"        /*  %2 = ch->samp */
		"  addl %3(%%edi),%%eax\n"        /*  %3 = ch->pos */
		"  testb %5, %4(%%edi)\n"         /*  %5 = MIXRQ_PLAY16BIT */
		                                  /*  %4 = ch->status, */
		"  jnz mixrFadeChannel16\n"
		"    movb (%%eax), %%bl\n"
		"  jmp mixrFadeChanneldo\n"
		"mixrFadeChannel16:\n"
		"    movb 1(,%%eax,2),%%bl\n"
		"mixrFadeChanneldo:\n"
		"  movb %%bl, %%cl\n"
		"  movl 1234(,%%ebx,4),%%ebx\n"
		"mixrFadeChannelvoltab1:\n"
		"  movl 1234(,%%ecx,4),%%ecx\n"
		"mixrFadeChannelvoltab2:\n"
		"  addl %%ebx, (%%esi)\n"
		"  addl %%ecx, 4(%%esi)\n"
		"  movl $0, %0(%%edi)\n"          /* %0 = curvol[0] */
		"  movl $0, %1(%%edi)\n"          /* %1 = curvol[1] */
		"  ret\n"
		"setupfade:\n" /* CALLED FROM EXTERNAL */
		"  movl %%eax, (mixrFadeChannelvoltab1-4)\n"
		"  movl %%eax, (mixrFadeChannelvoltab2-4)\n"
		"  ret\n"
		:
		: "m" (((struct channel *)NULL)->curvols[0]), /*  0  */
		  "m" (((struct channel *)NULL)->curvols[1]), /*  1  */
		  "m" (((struct channel *)NULL)->samp),       /*  2  */
		  "m" (((struct channel *)NULL)->pos),        /*  3  */
		  "m" (((struct channel *)NULL)->status),     /*  4  */
		  "n" (MIXRQ_PLAY16BIT)                       /*  5  */
	);
}

void mixrFadeChannel(int32_t *fade, struct channel *ch)
{
	int d0, d1;
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
		"call mixrFadeChannel_\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S"(d0),
		  "=&D"(d1)
		: "0"(fade),
		  "1"(ch)
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx"
#else
		: "memory", "eax", "ebx", "ecx", "edx"
#endif
	);
}

void nonepublic_dwmixa1(void)
{
	__asm__ __volatile__
	(
		"playquiet:\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmono:\n"
		"playmonolp:\n"
		"    movb (%esi), %bl\n"
		"    addl $1234,%edx\n"
		"monostepl:\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playmonomonosteplvol1:\n"
		"    adcl $1234, %esi\n"
		"monosteph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"monoramp:\n"
		"  cmpl $1234, %edi\n"
		"monoendp:\n"
		"  jb playmonolp\n"
		"  ret\n"

		"setupmono:\n" /* CALLED FROM EXTERNAL */
		"  movl %eax, (playmonomonosteplvol1-4)\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmono16:\n"
		"playmono16lp:\n"
		"    movb 1(%esi,%esi), %bl\n"
		"    addl $1234, %edx\n"
		"mono16stepl:\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playmono16vol1:\n"
		"    adcl $1234, %esi\n"
		"mono16steph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"mono16ramp:\n"
		"    cmpl $1234, %edi\n"
		"mono16endp:\n"
		"  jb playmono16lp\n"
		"  ret\n"

		"setupmono16:\n" /* usual CALLED from EXTERNAL crap*/
		"  movl %eax, (playmono16vol1-4)\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi:\n"
		"playmonoilp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb (%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playmonoiint0:\n"
		"    movb 1(%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playmonoiint1:\n"

		"    addl $1234, %edx\n"
		"monoistepl:\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playmonoivol1:\n"
		"    adcl $1234, %esi\n"
		"monoisteph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"monoiramp:\n"
		"  cmpl $1234, %edi\n"
		"monoiendp:\n"
		"  jb playmonoilp\n"
		"  ret\n"

		"setupmonoi:\n" /* need to comment??? external */
		"  movl %eax, (playmonoivol1-4)\n"
		"  movl %ebx, (playmonoiint0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playmonoiint1-4)\n"
		"  decl %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi16:\n"
		"playmonoi16lp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb 1(%esi,%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playmonoi16int0:\n"
		"    movb 3(%esi,%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playmonoi16int1:\n"

		"    addl $1234, %edx\n"
		"monoi16stepl:\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playmonoi16vol1:\n"
		"    adcl $1234, %esi\n"
		"monoi16steph:\n"
		"    addl %eax, (%edi)\n"
		"    addl $4, %edi\n"
		"    addl $1234, %ebx\n"
		"monoi16ramp:\n"
		"  cmpl $1234, %edi\n"
		"monoi16endp:\n"
		"  jb playmonoi16lp\n"
		"  ret\n"

		"setupmonoi16:\n" /* WE ARE NOT SHOCKED ABOUT EXTERNAL STUFF? */
		"  movl %eax, (playmonoi16vol1-4)\n"
		"  movl %ebx, (playmonoi16int0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playmonoi16int1-4)\n"
		"  decl %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playstereo:\n"
		"playstereolp:\n"
		"    movb (%esi), %bl\n"
		"    addl $1234, %edx\n"
		"stereostepl:\n"
		"    movb (%esi), %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereovol1:\n"
		"    adcl $1234, %esi\n"
		"stereosteph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereovol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoendp:\n"
		"  jb playstereolp\n"
		"  ret\n"

		"setupstereo:\n" /* TAKE A WILD GUESS */
		"  movl %eax, (playstereovol1-4)\n"
		"  movl %eax, (playstereovol2-4)\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playstereo16:\n"
		"playstereo16lp:\n"
		"    movb 1(%esi,%esi), %bl\n"
		"    addl $1234, %edx\n"
		"stereo16stepl:\n"
		"    movb 1(%esi,%esi), %cl\n"
		"    movl 1234(,%ebx,4),%eax\n"
		"playstereo16vol1:\n"
		"    adcl $1234, %esi\n"
		"stereo16steph:\n"
		"    addl %eax,(%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereo16vol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234,%ebx\n"
		"stereo16ramp0:\n"
		"    addl $1234,%ecx\n"
		"stereo16ramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereo16endp:\n"
		"  jb playstereo16lp\n"
		"  ret\n"

		"setupstereo16:\n" /* GUESS TWO TIMES? */
		"  movl %eax, (playstereo16vol1-4)\n"
		"  movl %eax, (playstereo16vol2-4)\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playstereoi:\n"
		"playstereoilp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb (%esi), %al\n"
		"    movb 1234(%eax,%eax), %bl\n"
		"playstereoiint0:\n"
		"    movb 1(%esi), %al\n"
		"    addb 1234(%eax,%eax), %bl\n"
		"playstereoiint1:\n"

		"    addl $1234, %edx\n"
		"stereoistepl:\n"
		"    movb %bl, %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereoivol1:\n"
		"    adcl $1234, %esi\n"
		"stereoisteph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4),%eax\n"
		"playstereoivol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoiramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoiramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoiendp:\n"
		"  jb playstereoilp\n"
		"  ret\n"

		"setupstereoi:\n" /* THESE ARE STARTING TO BECOME A HABIT NOW*/
		"  movl %eax, (playstereoivol1-4)\n"
		"  movl %eax, (playstereoivol2-4)\n"
		"  movl %ebx, (playstereoiint0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playstereoiint1-4)\n"
		"  decl %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playstereoi16:\n"
		"playstereoi16lp:\n"
		"    movl %edx, %eax\n"
		"    shrl $20, %eax\n"
		"    movb 1(%esi,%esi), %al\n"
		"    movb 1234(%eax, %eax), %bl\n"
		"playstereoi16int0:\n"
		"    movb 3(%esi, %esi), %al\n"
		"    addb 1234(%eax, %eax), %bl\n"
		"playstereoi16int1:\n"

		"    addl $1234, %edx\n"
		"stereoi16stepl:\n"
		"    movb %bl, %cl\n"
		"    movl 1234(,%ebx,4), %eax\n"
		"playstereoi16vol1:\n"
		"    adcl $1234, %esi\n"
		"stereoi16steph:\n"
		"    addl %eax, (%edi)\n"
		"    movl 1234(,%ecx,4), %eax\n"
		"playstereoi16vol2:\n"
		"    addl %eax, 4(%edi)\n"
		"    addl $8, %edi\n"
		"    addl $1234, %ebx\n"
		"stereoi16ramp0:\n"
		"    addl $1234, %ecx\n"
		"stereoi16ramp1:\n"
		"  cmpl $1234, %edi\n"
		"stereoi16endp:\n"
		"  jb playstereoi16lp\n"
		"  ret\n"

		"setupstereoi16:" /* THIS IS THE LAST ONE!!!!!!!!!! */
		"  movl %eax, (playstereoi16vol1-4)\n"
		"  movl %eax, (playstereoi16vol2-4)\n"
		"  movl %ebx, (playstereoi16int0-4)\n"
		"  incl %ebx\n"
		"  movl %ebx, (playstereoi16int1-4)\n"
		"  decl %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		".section .data\n"
		"dummydd: .long 0\n"

		"routq:\n"
		".long   playquiet,       dummydd,         dummydd,         dummydd,         dummydd,         dummydd,         0,0\n"
		"routtab:\n"
		".long   playmono,        monostepl-4,     monosteph-4,     monoramp-4,      dummydd,         monoendp-4,      0,0\n"
		".long   playmono16,      mono16stepl-4,   mono16steph-4,   mono16ramp-4,    dummydd,         mono16endp-4,    0,0\n"
		".long   playmonoi,       monoistepl-4,    monoisteph-4,    monoiramp-4,     dummydd,         monoiendp-4,     0,0\n"
		".long   playmonoi16,     monoi16stepl-4,  monoi16steph-4,  monoi16ramp-4,   dummydd,         monoi16endp-4,   0,0\n"
		".long   playstereo,      stereostepl-4,   stereosteph-4,   stereoramp0-4,   stereoramp1-4,   stereoendp-4,    0,0\n"
		".long   playstereo16,    stereo16stepl-4, stereo16steph-4, stereo16ramp0-4, stereo16ramp1-4, stereo16endp-4,  0,0\n"
		".long   playstereoi,     stereoistepl-4,  stereoisteph-4,  stereoiramp0-4,  stereoiramp1-4,  stereoiendp-4,   0,0\n"
		".long   playstereoi16,   stereoi16stepl-4,stereoi16steph-4,stereoi16ramp0-4,stereoi16ramp1-4,stereoi16endp-4, 0,0\n"

		".previous\n"
	);
}

void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan, int stereo)
{
	void *routptr;
	uint32_t filllen,
	         ramping[2];
	int inloop;
	int ramploop;
	int dofade;
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
		"  movl %3, %%edi\n"              /*  %3 = chan */
		"  testb %25, %12(%%edi)\n"       /* %25 = MIXRQ_PLAYING */
		                                  /* %12 = status */
		"  jz mixrPlayChannelexit\n"

		"  movl $0, %6\n"                 /*  %6 = fillen */
		"  movl $0, %11\n"                /* %11 = dofade */

		"  xorl %%eax, %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelnostereo\n"
		"    addl $4, %%eax\n"
		"mixrPlayChannelnostereo:\n"
		"  testb %27, %12(%%edi)\n"       /* %27 = MIXRQ_INTERPOLATE */
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelnointr\n"
		"    addl $2, %%eax\n"
		"mixrPlayChannelnointr:\n"
		"  testb %26, %12(%%edi)\n"       /* %26 = MIXRQ_PLAY16BIT */
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelpsetrtn\n"
		"    incl %%eax\n"
		"mixrPlayChannelpsetrtn:\n"
		"  shll $5, %%eax\n"
		"  addl $routtab, %%eax\n"
		"  movl %%eax, %5\n"              /*  %5 = routeptr*/

		"mixrPlayChannelbigloop:\n"
		"  movl %2, %%ecx\n"              /*  %2 = len */
		"  movl %13(%%edi), %%ebx\n"      /* %13 = ch->step */
		"  movl %14(%%edi), %%edx\n"      /* %14 = ch->pos */
		"  movw %15(%%edi), %%si\n"       /* %15 = ch->fpos */
		"  movb $0, %9\n"                 /*  %9 = inloop */
		"  cmpl $0, %%ebx\n"

		"  je mixrPlayChannelplayecx\n"
		"  jg mixrPlayChannelforward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"
		"    testb %28, %12(%%edi)\n"     /* %28 = MIXRQ_LOOPED */
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"
		"    cmpl %16(%%edi), %%edx\n"    /* %16 = ch->loopstart */
		"    jb mixrPlayChannelmaxplaylen\n"
		"    subl %16(%%edi), %%eax\n"    /* %16 = ch->loopstart */
		"    movb $1, %9\n"               /*  %9 = inloop */
		"    jmp mixrPlayChannelmaxplaylen\n"
		"mixrPlayChannelforward:\n"
		"    movl %18(%%edi), %%eax\n"    /* %18 = length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb %28, %12(%%edi)\n"     /* %28 = MIXRQ_LOOPED */
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"
		"    cmpl %17(%%edi), %%edx\n"    /* %17 = ch->loopend */
		"    jae mixrPlayChannelmaxplaylen\n"
		"    subl %18(%%edi), %%eax\n"    /* %18 = ch->length */
		"    addl %17(%%edi), %%eax\n"    /* %17 = ch->loopend*/
		"    movb $1, %9\n"               /*  %9 = inloop */

		"mixrPlayChannelmaxplaylen:\n"
		"  xorl %%edx, %%edx\n"
		"  shld $16, %%eax, %%edx\n"
		"  shll $16, %%esi\n"
		"  shld $16, %%esi, %%eax\n"
		"  addl %%ebx, %%eax\n"
		"  adcl $0, %%edx\n"
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		"  cmpl %%ebx, %%edx\n"
		"  jae mixrPlayChannelplayecx\n"
		"  divl %%ebx\n"
		"  cmpl %%eax, %%ecx\n"
		"  jb mixrPlayChannelplayecx\n"
		"    movl %%eax, %%ecx\n"
		"    cmpb $0, %9\n"               /*  %9 = inloop */
		"    jnz mixrPlayChannelplayecx\n"
#if MIXRQ_PLAYING != 1
#error This line bellow depends on MIXRQ_PLAYING = 1
#endif
		"      andb $254, %12(%%edi)\n"   /* 254 = 255-MIXRQ_PLAYING */
		                                  /* %12 = ch->status */
		"      movl $1, %11\n"            /* %11 = dofade */
		"      movl %2, %%eax\n"          /*  %2 = len */
		"      subl %%ecx, %%eax\n"
		"      addl %%eax, %6\n"          /*  %6 = filllen */
		"      movl %%ecx, %2\n"          /*  %2 = len */

		"mixrPlayChannelplayecx:\n"
		"  movb $0, %10\n"                /* %10 = ramploop */
		"  movl $0, %7\n"                 /*  %7 = ramping[0] */
		"  movl $0, %8\n"                 /*  %8 = ramping[1] */

		"  cmpl $0, %%ecx\n"
		"  je mixrPlayChannelnoplay\n"

		"  movl %21(%%edi), %%edx\n"      /* %21 = ch->dstvols[0] */
		"  subl %19(%%edi), %%edx\n"      /* %19 = ch->curvols[0] */
		"  je mixrPlayChannelnoramp0\n"
		"  jl mixrPlayChannelramp0down\n"
		"    movl $1, %7\n"               /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp0\n"
		"mixrPlayChannelramp0down:\n"
		"    negl %%edx\n"
		"    movl $-1, %7\n"              /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp0:\n"

		"  movl %22(%%edi), %%edx\n"      /* %22 = ch->dstvols[1] */
		"  subl %20(%%edi), %%edx\n"      /* %20 = ch->curvols[1] */
		"  je mixrPlayChannelnoramp1\n"
		"  jl mixrPlayChannelramp1down\n"
		"    movl $1, %8\n"               /*  %8 = ramping[4] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp1\n"
		"mixrPlayChannelramp1down:\n"
		"    negl %%edx\n"
		"    movl $-1, %8\n"              /*  %8 = ramping[1] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp1:\n"

		"  movl %5, %%edx\n"              /*  %5 = routptr */
		"  cmpl $0, %7\n"                 /*  %7 = ramping[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %8\n"                 /*  %8 = ramping[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %19(%%edi)\n"         /* %19 = ch->curvols[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %20(%%edi)\n"         /* %20 = ch->curvols[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"    movl $routq, %%edx\n"

		"mixrPlayChannelnotquiet:\n"
		"  movl 4(%%edx), %%ebx\n"
		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 8(%%edx), %%ebx\n"
		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 12(%%edx), %%ebx\n"
		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 16(%%edx), %%ebx\n"
		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		"  movl 20(%%edx), %%ebx\n"
		"  leal (,%%ecx,4), %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm1\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm1:\n"
		"  addl %0, %%eax\n"              /*  %0 = buf */
		"  movl %%eax, (%%ebx)\n"
		"  pushl %%ecx\n"
		"  movl (%%edx), %%eax\n"

		"  movl %19(%%edi), %%ebx\n"      /* %19 = ch->curvols[0] */
		"  shll $8, %%ebx\n"
		"  movl %20(%%edi), %%ecx\n"      /* %20 = ch->curvols[1] */
		"  shll $8, %%ecx\n"
		"  movw %15(%%edi), %%dx\n"       /* %15 = ch->fpos */
		"  shll $16, %%edx\n"
		"  movl %14(%%edi), %%esi\n"      /* %14 = ch->chpos */
		"  addl %23(%%edi), %%esi\n"      /* %23 = ch->samp */
		"  movl %0, %%edi\n"              /*  %0 = buf */

		"  call *%%eax\n"

		"  popl %%ecx\n"
		"  movl %3, %%edi\n"              /*  %3 = chan */

		"mixrPlayChannelnoplay:\n"
		"  movl %%ecx, %%eax\n"
		"  shll $2, %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm2\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm2:\n"
		"  addl %%eax, %0\n"              /*  %0 = buf */
		"  subl %%ecx, %2\n"              /*  %2 = len */

		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  imul %%ecx\n"
		"  shld $16, %%eax, %%edx\n"
		"  addw %%ax, %15(%%edi)\n"       /* %15 = ch->fpos */
		"  adcl %%edx, %14(%%edi)\n"      /* %14 = ch->pos */

		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %19(%%edi)\n"      /* %19 = ch->curvols[0] */
		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %20(%%edi)\n"      /* %20 = ch->curvols[1] */

		"  cmpb $0, %10\n"                /* %10 = ramploop */
		"  jnz mixrPlayChannelbigloop\n"

		"  cmpb $0, %9\n"                 /*  %9 = inloop */
		"  jz mixrPlayChannelfill\n"

		"  movl %14(%%edi), %%eax\n"      /* %14 = ch->pos */
		"  cmpl $0, %13(%%edi)\n"         /* %13 = ch->step */
		"  jge mixrPlayChannelforward2\n"
		"    cmpl %16(%%edi), %%eax\n"    /* %16 = ch->loopstart */
		"    jge mixrPlayChannelexit\n"
		"    testb %29, %12(%%edi)\n"     /* %29 = MIXRQ_PINGPONGLOOP */
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelpong\n"
		"      addl %24(%%edi), %%eax\n"  /* %24 = ch->replen */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelpong:\n"
		"      negl %13(%%edi)\n"         /* %13 = ch->step */
		"      negw %15(%%edi)\n"         /* %15 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %16(%%edi), %%eax\n"  /* %16 = ch->loopstart */
		"      addl %16(%%edi), %%eax\n"  /* %16 = ch->loopstart */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelforward2:\n"
		"    cmpl %17(%%edi), %%eax\n"    /* %17 = ch->loopend */
		"    jb mixrPlayChannelexit\n"
		"    testb %29, %12(%%edi)\n"     /* %29 = MIXRQ_PINGPONGLOOP */
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelping\n"
		"      subl %24(%%edi), %%eax\n"  /* %24 = ch->replen */
		"      jmp mixrPlayChannelloopiflen\n"
		"mixrPlayChannelping:\n"
		"      negl %13(%%edi)\n"         /* %13 = ch->step */
		"      negw %15(%%edi)\n"         /* %15 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %17(%%edi), %%eax\n"  /* %17 = ch->loopend */
		"      addl %17(%%edi), %%eax\n"  /* %17 = ch->loopend */

		"mixrPlayChannelloopiflen:\n"
		"  movl %%eax, %14(%%edi)\n"      /* %14 = ch->pos */
		"  cmpl $0, %2\n"                 /*  %2 = len */
		"  jne mixrPlayChannelbigloop\n"
		"  jmp mixrPlayChannelexit\n"

		"mixrPlayChannelfill:\n"
		"  cmpl $0, %6\n"                 /*  %6 = filllen */
		"  je mixrPlayChannelfadechk\n"
		"  movl %18(%%edi), %%eax\n"      /* %18 = ch->length */
		"  movl %%eax, %14(%%edi)\n"      /* %14 = ch->pos */
		"  addl %23(%%edi), %%eax\n"      /* %23 = ch->samp */
		"  movl %19(%%edi), %%ebx\n"      /* %19 = ch->curvols[0] */
		"  movl %20(%%edi), %%ecx\n"      /* %20 = ch->curvols[1] */
		"  shll $8, %%ebx\n"
		"  shll $8, %%ecx\n"
		"  testb %26, %12(%%edi)\n"       /* %26 = MIXRQ_PLAY16BIT */
		                                  /* %12 = ch->status */
		"  jnz mixrPlayChannelfill16\n"
		"    movb (%%eax), %%bl\n"
		"    jmp mixrPlayChannelfilldo\n"
		"mixrPlayChannelfill16:\n"
		"    movb 1(%%eax, %%eax), %%bl\n"
		"mixrPlayChannelfilldo:\n"
		"  movb %%bl, %%cl\n"
		"  movl 1234(,%%ebx,4), %%ebx\n"
		"mixrPlayChannelvoltab1:\n"
		"  movl 1234(,%%ecx,4), %%ecx\n"
		"mixrPlayChannelvoltab2:\n"
		"  movl %6, %%eax\n"              /*  %6 = filllen */
		"  movl %0, %%edi\n"              /*  %0 = buf */
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  jne mixrPlayChannelfillstereo\n"
		"mixrPlayChannelfillmono:\n"
		"    addl %%ebx,(%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%eax\n"
		"  jnz mixrPlayChannelfillmono\n"
		"  jmp mixrPlayChannelfade\n"
		"mixrPlayChannelfillstereo:\n"
		"    addl %%ebx, (%%edi)\n"
		"    addl %%ecx, 4(%%edi)\n"
		"    addl $8, %%edi\n"
		"  decl %%eax\n"
		"  jnz mixrPlayChannelfillstereo\n"
		"  jmp mixrPlayChannelfade\n"

		"mixrPlayChannelfadechk:\n"
		"  cmpl $0, %11\n"                /* %11 = dofade */
		"  je mixrPlayChannelexit\n"
		"mixrPlayChannelfade:\n"
		"  movl %3, %%edi\n"              /* %3 = chan */
		"  movl %1, %%esi\n"              /* %1 = fadebuf */
		"  call mixrFadeChannel_\n"
		"  jmp mixrPlayChannelexit\n"

		"setupplay:\n"
		"  movl %%eax, (mixrPlayChannelvoltab1-4)\n"
		"  movl %%eax, (mixrPlayChannelvoltab2-4)\n"
		"  ret\n"

		"mixrPlayChannelexit:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "m" (buf),                                  /*   0  */
		  "m" (fadebuf),                              /*   1  */
		  "m" (len),                                  /*   2  */
		  "m" (chan),                                 /*   3  */
		  "m" (stereo),                               /*   4  */
		  "m" (routptr),                              /*   5  */
		  "m" (filllen),                              /*   6  */
		  "m" (ramping[0]),                           /*   7  */
		  "m" (ramping[1]),                           /*   8  */
		  "m" (inloop),                               /*   9  */
		  "m" (ramploop),                             /*  10  */
		  "m" (dofade),                               /*  11  */
		  "m" (((struct channel *)NULL)->status),     /*  12  */
		  "m" (((struct channel *)NULL)->step),       /*  13  */
		  "m" (((struct channel *)NULL)->pos),        /*  14  */
		  "m" (((struct channel *)NULL)->fpos),       /*  15  */
		  "m" (((struct channel *)NULL)->loopstart),  /*  16  */
		  "m" (((struct channel *)NULL)->loopend),    /*  17  */
		  "m" (((struct channel *)NULL)->length),     /*  18  */
		  "m" (((struct channel *)NULL)->curvols[0]), /*  19  */
		  "m" (((struct channel *)NULL)->curvols[1]), /*  20  */
		  "m" (((struct channel *)NULL)->dstvols[0]), /*  21  */
		  "m" (((struct channel *)NULL)->dstvols[1]), /*  22  */
		  "m" (((struct channel *)NULL)->samp),       /*  23  */
		  "m" (((struct channel *)NULL)->replen),     /*  24  */
		  "n" (MIXRQ_PLAYING),                        /*  25  */
		  "n" (MIXRQ_PLAY16BIT),                      /*  26  */
		  "n" (MIXRQ_INTERPOLATE),                    /*  27  */
		  "n" (MIXRQ_LOOPED),                         /*  28  */
		  "n" (MIXRQ_PINGPONGLOOP)                    /*  29  */
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx", "edi", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "edi", "esi"
#endif
	);
}

void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2])
{
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
		"movl  %%ecx, %%ebx\n"
#endif
		"  call setupfade\n"
		"  call setupplay\n"
		"  call setupmono\n"
		"  call setupmono16\n"
		"  call setupmonoi\n"
		"  call setupmonoi16\n"
		"  call setupstereo\n"
		"  call setupstereo16\n"
		"  call setupstereoi\n"
		"  call setupstereoi16\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "a" (vol),
#ifdef __PIC__
		  "c" (intr)
#else
		  "b" (intr)
#endif
		/* no registers should change, and .data/.bss is not touched */
	);
}

void mixrFade(int32_t *buf, int32_t *fade, int len, int stereo)
{
	int d0, d1, d2;
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
		"  movl (%%esi), %%eax\n"
		"  movl 4(%%esi), %%ebx\n"
		"  cmpl $0, %%edx\n"
		"  jnz mixrFadestereo\n"
		"mixrFadelpm:\n"
		"      movl %%eax, (%%edi)\n"
		"      movl %%eax, %%edx\n"
		"      shll $7, %%eax\n"
		"      subl %%edx, %%eax\n"
		"      sarl $7, %%eax\n"
		"      addl $4, %%edi\n"
		"    decl %%ecx\n"
		"    jnz mixrFadelpm\n"
		"  jmp mixrFadedone\n"
		"mixrFadestereo:\n"
		"mixrFadelps:\n"
		"      movl %%eax, (%%edi)\n"
		"      movl %%ebx, 4(%%edi)\n"
		"      movl %%eax, %%edx\n"
		"      shll $7, %%eax\n"
		"      subl %%edx, %%eax\n"
		"      sarl $7, %%eax\n"
		"      movl %%ebx, %%edx\n"
		"      shll $7, %%ebx\n"
		"      subl %%edx, %%ebx\n"
		"      sarl $7, %%ebx\n"
		"      addl $8, %%edi\n"
		"    decl %%ecx\n"
		"    jnz mixrFadelps\n"
		"mixrFadedone:\n"
		"  movl %%eax, (%%esi)\n"
		"  movl %%ebx, 4(%%esi)\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&D"(d0),
		  "=&c"(d1),
		  "=&d"(d2)
  		: "S" (fade),
		  "0" (buf),
		  "1" (len),
		  "2" (stereo)
#ifdef __PIC__
		: "memory", "eax"
#else
		: "memory", "eax", "ebx"
#endif
	);
}

/******************************************************************************/
void nonepublic_dwmixa2(void)
{
	__asm__ __volatile__
	(
		"mixrClip8_:\n"
		"  movl %ebx, (mixrClip8amp1-4)\n"
		"  addl $512, %ebx\n"
		"  movl %ebx, (mixrClip8amp2-4)\n"
		"  addl $512, %ebx\n"
		"  movl %ebx, (mixrClip8amp3-4)\n"
		"  subl $1024, %ebx\n"
		"  movl %edx, (mixrClip8max-4)\n"
		"  negl %edx\n"
		"  movl %edx, (mixrClip8min-4)\n"

		"  xorl %edx, %edx\n"
		"  movb (mixrClip8min-4), %dl\n"
		"  movl (%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8min-3), %dl\n"
		"  addl 512(%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8min-2), %dl\n"
		"  addl 1024(%ebx, %edx, 2), %eax\n"
		"  movb %ah, (mixrClip8minv-1)\n"
		"  movb (mixrClip8max-4), %dl\n"
		"  movl (%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8max-3), %dl\n"
		"  addl 512(%ebx, %edx, 2), %eax\n"
		"  movb (mixrClip8max-2), %dl\n"
		"  addl 1024(%ebx, %edx, 2), %eax\n"
		"  movb %ah, (mixrClip8maxv-1)\n"
		"  leal (%ecx, %edi), %ecx\n"
		"  movl %ecx, (mixrClip8endp1-4)\n"
		"  movl %ecx, (mixrClip8endp2-4)\n"
		"  movl %ecx, (mixrClip8endp3-4)\n"
		"  xorl %ebx, %ebx\n"
		"  xorl %ecx, %ecx\n"
		"  xorl %edx, %edx\n"

		"mixrClip8lp:\n"
		"  cmpl $1234, (%esi)\n"
		"    mixrClip8min:\n"
		"  jl mixrClip8low\n"
		"  cmpl $1234, (%esi)\n"
		"    mixrClip8max:\n"
		"  jg mixrClip8high\n"

		"    movb (%esi), %bl\n"
		"    movb 1(%esi), %cl\n"
		"    movb 2(%esi), %dl\n"
		"    movl 1234(,%ebx,2), %eax\n"
		"      mixrClip8amp1:\n"
		"    addl 1234(,%ecx,2), %eax\n"
		"      mixrClip8amp2:\n"
		"    addl 1234(,%edx,2), %eax\n"
		"      mixrClip8amp3:\n"
		"    movb %ah, (%edi)\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"  cmpl $1234, %edi\n"
		"mixrClip8endp1:\n"
		"  jb mixrClip8lp\n"
		"mixrClip8done:\n"
		"  jmp mixrClip8out\n"

		"mixrClip8low:\n"
		"    movb $12, (%edi)\n"
		"      mixrClip8minv:\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"    cmpl $1234, %edi\n"
		"mixrClip8endp2:\n"
		"  jb mixrClip8lp\n"
		"  jmp mixrClip8done\n"

		"mixrClip8high:\n"
		"    movb $12, (%edi)\n"
		"      mixrClip8maxv:\n"
		"    incl %edi\n"
		"    addl $4, %esi\n"
		"    cmpl $1234, %edi\n"
		"mixrClip8endp3:\n"
		"  jb mixrClip8lp\n"
		"  jmp mixrClip8done\n"
	);
}

void mixrClip(void *dst, int32_t *src, int len, void *tab, int32_t max, int b16)
{
	int d0, d1, d2, d3, d4, d5;
#ifdef __PIC__
	d2=(int)tab;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
		"movl %10, %%ebx\n"
#endif
		"  cmpl $0, %%eax\n"
		"  je mixrClip8_\n"

		"  movl %%ebx, (mixrClipamp1-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (mixrClipamp2-4)\n"
		"  addl $512, %%ebx\n"
		"  movl %%ebx, (mixrClipamp3-4)\n"
		"  subl $1024, %%ebx\n"
		"  movl %%edx, (mixrClipmax-4)\n"
		"  negl %%edx\n"
		"  movl %%edx, (mixrClipmin-4)\n"

		"  xorl %%edx, %%edx\n"
		"  movb (mixrClipmin-4), %%dl\n"
		"  movl (%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmin-3), %%dl\n"
		"  addl 512(%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmin-2), %%dl\n"
		"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
		"  movw %%ax, (mixrClipminv-2)\n"
		"  movb (mixrClipmax-4), %%dl\n"
		"  movl (%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmax-3), %%dl\n"
		"  addl 512(%%ebx, %%edx, 2), %%eax\n"
		"  movb (mixrClipmax-2), %%dl\n"
		"  addl 1024(%%ebx, %%edx, 2), %%eax\n"
		"  movw %%ax, (mixrClipmaxv-2)\n"
		"  leal (%%edi, %%ecx, 2), %%ecx\n"
		"  movl %%ecx, (mixrClipendp1-4)\n"
		"  movl %%ecx, (mixrClipendp2-4)\n"
		"  movl %%ecx, (mixrClipendp3-4)\n"
		"  xorl %%ebx, %%ebx\n"
		"  xorl %%ecx, %%ecx\n"
		"  xorl %%edx, %%edx\n"

		"mixrCliplp:\n"
		"    cmpl $1234, (%%esi)\n"
		"      mixrClipmin:\n"
		"    jl mixrCliplow\n"
		"    cmpl $1234, (%%esi)\n"
		"      mixrClipmax:\n"
		"    jg mixrCliphigh\n"

		"    movb (%%esi), %%bl\n"
		"    movb 1(%%esi), %%cl\n"
		"    movb 2(%%esi), %%dl\n"
		"    movl 1234(,%%ebx,2), %%eax\n"
		"      mixrClipamp1:\n"
		"    addl 1234(,%%ecx,2), %%eax\n"
		"      mixrClipamp2:\n"
		"    addl 1234(,%%edx,2), %%eax\n"
		"      mixrClipamp3:\n"
		"    movw %%ax, (%%edi)\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp1:\n"
		"  jb mixrCliplp\n"
		"  jmp mixrClipdone\n"

		"mixrCliplow:\n"
		"    movw $1234, (%%edi)\n"
		"      mixrClipminv:\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp2:\n"
		"  jb mixrCliplp\n"
		"  jmp mixrClipdone\n"
		"mixrCliphigh:\n"
		"    movw $1234, (%%edi)\n"
		"      mixrClipmaxv:\n"
		"    addl $2, %%edi\n"
		"    addl $4, %%esi\n"
		"  cmpl $1234, %%edi\n"
		"    mixrClipendp3:\n"
		"  jb mixrCliplp\n"
		/* jmp mixrClipdone\n" */  
		"mixrClipdone:"
		"mixrClip8out:"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
		  "=&c" (d3),
		  "=&d" (d4),
#ifdef __PIC__
		  "=&a" (d5)
#else
		  "=&a" (d5),
		  "=&b" (d2)
#endif			  
		: "0" (src),
		  "1" (dst),
		  "2" (len),
		  "3" (max),
		  "4" (b16),
#ifdef __PIC__
		  "m" (tab)
#else
		  "5" (tab)
#endif
		: "memory"
	);
}

void remap_range1_stop(void){}

#else

/*static uint32_t (*mixrFadeChannelvoltab)[256];*/
static int32_t ramping[2];
static int32_t (*mixrFadeChannelvoltab)[256];
static uint8_t (*mixrFadeChannelintrtab)[256][2];

void mixrSetupAddresses(int32_t (*vol)[256], uint8_t (*intr)[256][2])
{
	mixrFadeChannelvoltab=vol;
	mixrFadeChannelintrtab=intr;
}

#include <stdio.h>
void mixrFadeChannel(int32_t *fade, struct channel *chan)
{
	if (chan->status&MIXRQ_PLAY16BIT)
	{
		fade[0]+=mixrFadeChannelvoltab[chan->curvols[0]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
		fade[1]+=mixrFadeChannelvoltab[chan->curvols[1]][((uint16_t)chan->realsamp.bit16[chan->pos])>>8];
	} else {
		fade[0]+=mixrFadeChannelvoltab[chan->curvols[0]][(uint8_t)chan->realsamp.bit8[chan->pos]];
		fade[1]+=mixrFadeChannelvoltab[chan->curvols[1]][(uint8_t)chan->realsamp.bit8[chan->pos]];
	}
	chan->curvols[0]=0;
	chan->curvols[1]=0;
}


static void playmono(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		*(buf++)+=mixrFadeChannelvoltab[vol0][(uint8_t)(chan->realsamp.bit8[pos])];
		fpos+=chan->step&0xffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmono16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		*(buf++)+=mixrFadeChannelvoltab[vol0][((uint16_t)(chan->realsamp.bit16[pos]))>>8];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmonoi(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos]))][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos+1]))][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playmonoi16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache = 
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos]))>>8][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos+1]))>>8][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		len--;
	}
}

static void playstereo(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=chan->realsamp.bit8[pos];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereo16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=((uint16_t)(chan->realsamp.bit16[pos]))>>8;
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereoi(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos]))][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint8_t)(chan->realsamp.bit8[pos+1]))][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void playstereoi16(int32_t *buf, uint32_t len, struct channel *chan)
{
	int32_t vol0=chan->curvols[0];
	int32_t vol0add=ramping[0];
	int32_t vol1=chan->curvols[1];
	int32_t vol1add=ramping[1];
	uint32_t pos=chan->pos;
	uint32_t fpos=chan->fpos;

	while (len)
	{
		uint8_t cache=
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos]))>>8][0]+
			mixrFadeChannelintrtab[fpos>>12][((uint16_t)(chan->realsamp.bit16[pos+1]))>>8][1];
		*(buf++)+=mixrFadeChannelvoltab[vol0][cache];
		*(buf++)+=mixrFadeChannelvoltab[vol1][cache];
		fpos+=chan->step&0x0000ffff;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0xffff;
		}
		pos+=chan->step>>16;
		vol0+=vol0add;
		vol1+=vol1add;
		len--;
	}
}

static void routequiet(int32_t *buf, uint32_t len, struct channel *chan)
{
}

typedef void (*route_func)(int32_t *buf, uint32_t len, struct channel *chan);
static const route_func routeptrs[8]=
{
	playmono,
	playmono16,
	playmonoi,
	playmonoi16,
	playstereo,
	playstereo16,
	playstereoi,
	playstereoi16
};

#ifdef I386_ASM_EMU
static void writecallback(uint_fast16_t selector, uint_fast32_t addr, int size, uint_fast32_t data)
{
}

static uint_fast32_t readcallback(uint_fast16_t selector, uint_fast32_t addr, int size)
{
	return 0;
}

void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan, int stereo)
{
	int32_t *orgbuf = buf;
	uint32_t orglen = len;

	uint32_t filllen;
/*	         ramping[2];*/
	uint32_t inloop;
	uint32_t ramploop;
	uint32_t dofade;
	route_func routeptr;

	uint32_t step_high;
	uint32_t step_low;
	uint32_t ramping0;
	uint32_t ramping1;
	uint32_t endofbuffer;
	uint32_t bufferlen;

	struct assembler_state_t state;
	init_assembler_state(&state, writecallback, readcallback);

	asm_movl(&state, 0xf0000000, &state.edi); /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */
	asm_testb(&state, MIXRQ_PLAYING, chan->status);

	asm_jz(&state, mixrPlayChannelexit);

	asm_movl(&state, 0, (uint32_t *)&filllen);
	asm_movl(&state, 0, (uint32_t *)&dofade);

	asm_xorl(&state, state.eax, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelnostereo);
		asm_addl(&state, 4, &state.eax);
mixrPlayChannelnostereo:
	asm_testb(&state, MIXRQ_INTERPOLATE, chan->status);

	asm_jz(&state, mixrPlayChannelnointr);
		asm_addl(&state, 2, &state.eax);
mixrPlayChannelnointr:
	asm_testb(&state, MIXRQ_PLAY16BIT, chan->status);

	asm_jz(&state, mixrPlayChannelpsetrtn);
		asm_incl(&state, &state.eax);	
mixrPlayChannelpsetrtn:
	asm_shll(&state, 5, &state.eax);
	asm_addl(&state, 0xf8000000, &state.eax);
	{
		unsigned int tmp = 0;
		asm_movl(&state, state.eax, &tmp);
		tmp-=0xf8000000;
		tmp/=8; /* 8 columns of information in the assembler version */
		if (tmp > sizeof(routeptrs))
		{
			fprintf(stderr, "#GP Exception occured (tmp=%d)\n", tmp);
			tmp = 0;
		}
		else if (tmp&3)
		{
			fprintf(stderr, "#AC(0) Exception occured\n");
			tmp = 0;
		}
		routeptr = routeptrs[tmp>>2];
	}

mixrPlayChannelbigloop:
	asm_movl(&state, len, &state.ecx);
	asm_movl(&state, chan->step, &state.ebx);
	asm_movl(&state, chan->pos, &state.edx);
	asm_movw(&state, chan->fpos, &state.si);
	asm_movl(&state, 0, &inloop);
	asm_cmpl(&state, 0, state.ebx);
	
	asm_je(&state, mixrPlayChannelplayecx);
	asm_jg(&state, mixrPlayChannelforward);
		asm_negl(&state, &state.ebx);
		asm_movl(&state, state.edx, &state.eax);
		asm_testb(&state, MIXRQ_LOOPED, chan->status);

		asm_jz(&state, mixrPlayChannelmaxplaylen);
		asm_cmpl(&state, chan->loopstart, state.edx);
		asm_jb(&state, mixrPlayChannelmaxplaylen);
		asm_subl(&state, chan->loopstart, &state.eax);
		asm_movl(&state, 1, &inloop);
		asm_jmp(&state, mixrPlayChannelmaxplaylen);
mixrPlayChannelforward:
		asm_movl(&state, chan->length, &state.eax);
		asm_negw(&state, &state.si);
		asm_sbbl(&state, state.edx, &state.eax);
		asm_testb(&state, MIXRQ_LOOPED, chan->status);

		asm_jz(&state, mixrPlayChannelmaxplaylen);
		asm_cmpl(&state, chan->loopend, state.edx);
		asm_jae(&state, mixrPlayChannelmaxplaylen);
		asm_subl(&state, chan->length, &state.eax);
		asm_addl(&state, chan->loopend, &state.eax);
		asm_movl(&state, 1, &inloop);

mixrPlayChannelmaxplaylen:
	asm_xorl(&state, state.edx, &state.edx);
	asm_shldl(&state, 16, state.eax, &state.edx);
	asm_shll(&state, 16, &state.esi);
	asm_shldl(&state, 16, state.esi, &state.eax);
	asm_addl(&state, state.ebx, &state.eax);
	asm_adcl(&state, 0, &state.edx);
	asm_subl(&state, 1, &state.eax);
	asm_sbbl(&state, 0, &state.edx);
	asm_cmpl(&state, state.ebx, state.edx);
	asm_jae(&state, mixrPlayChannelplayecx);
	asm_divl(&state, state.ebx);
	asm_cmpl(&state, state.eax, state.ecx);
	asm_jb(&state, mixrPlayChannelplayecx);
		asm_movl(&state, state.eax, &state.ecx);
		asm_cmpl(&state, 0, inloop);
		asm_jnz(&state, mixrPlayChannelplayecx);

		asm_andw(&state, ~MIXRQ_PLAYING, (uint16_t *)&chan->status);

		asm_movl(&state, 1, &dofade);
		asm_movl(&state, len, &state.eax);
		asm_subl(&state, state.ecx, &state.eax);
		asm_addl(&state, state.eax, &filllen);
		asm_movl(&state, state.ecx, &len);

mixrPlayChannelplayecx:
	asm_movl(&state, 0, &ramploop);
	asm_movl(&state, 0, (uint32_t *)&ramping[0]);
	asm_movl(&state, 0, (uint32_t *)&ramping[1]);

	asm_cmpl(&state, 0, state.ecx);
	asm_je(&state, mixrPlayChannelnoplay);

	asm_movl(&state, chan->dstvols[0], &state.edx);
	asm_subl(&state, chan->curvols[0], &state.edx);
	asm_je(&state, mixrPlayChannelnoramp0);
	asm_jl(&state, mixrPlayChannelramp0down);
		asm_movl(&state, 1, (uint32_t *)&ramping[0]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp0);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
			asm_jmp(&state, mixrPlayChannelnoramp0);
mixrPlayChannelramp0down:
	asm_negl(&state, &state.edx);
	asm_movl(&state, -1, (uint32_t *)&ramping[0]);
	asm_cmpl(&state, state.edx, state.ecx);
	asm_jbe(&state, mixrPlayChannelnoramp0);
		asm_movl(&state, 1, &ramploop);
		asm_movl(&state, state.edx, &state.ecx);
mixrPlayChannelnoramp0:

	asm_movl(&state, chan->dstvols[1], &state.edx);
	asm_subl(&state, chan->curvols[1], &state.edx);
	asm_je(&state, mixrPlayChannelnoramp1);
	asm_jl(&state, mixrPlayChannelramp1down);
		asm_movl(&state, 1, (uint32_t *)&ramping[1]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp1);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
			asm_jmp(&state, mixrPlayChannelnoramp1);
mixrPlayChannelramp1down:
		asm_negl(&state, &state.edx);
		asm_movl(&state, -1, (uint32_t *)&ramping[1]);
		asm_cmpl(&state, state.edx, state.ecx);
		asm_jbe(&state, mixrPlayChannelnoramp1);
			asm_movl(&state, 1, &ramploop);
			asm_movl(&state, state.edx, &state.ecx);
mixrPlayChannelnoramp1:

	asm_movl(&state, /*(uint32_t)routeptr*/0x12345678, &state.edx); /* NC value */
	asm_cmpl(&state, 0, ramping[0]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, ramping[1]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, chan->curvols[0]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_cmpl(&state, 0, chan->curvols[1]);
	asm_jne(&state, mixrPlayChannelnotquiet);
	asm_movl(&state, /*(uint32_t)routq*/0x12345678, &state.edx);
		routeptr = routequiet; /* work-around */

mixrPlayChannelnotquiet:
	asm_movl(&state, 0x12345678+4, &state.ebx);
	asm_movl(&state, chan->step, &state.eax);
	asm_shll(&state, 16, &state.eax);
	asm_movl(&state, state.eax, &step_high); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+8, &state.ebx);
	asm_movl(&state, chan->step, &state.eax);
	asm_sarl(&state, 16, &state.eax);
	asm_movl(&state, state.eax, &step_low); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+12, &state.ebx);
	asm_movl(&state, ramping[0], &state.eax);
	asm_shll(&state, 8, &state.eax);
	asm_movl(&state, state.eax, &ramping0); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+16, &state.ebx);
	asm_movl(&state, ramping[1], &state.eax);
	asm_shll(&state, 8, &state.eax);
	asm_movl(&state, state.eax, &ramping1); /* ref ebx to reach step_high */
	asm_movl(&state, 0x12345678+20, &state.ebx);
	bufferlen = state.ecx;
	endofbuffer = buf[state.ecx];
	asm_leal(&state, state.ecx*4, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelm1);
		asm_shll(&state, 1, &state.eax);
mixrPlayChannelm1:
	asm_addl(&state, 0x10000000, &state.eax); /* buf */
	//asm_movl(&state, eax, *(uint32_t **)(state.ebx));  we save these above */

	asm_pushl(&state, state.ecx);
	/* asm_movl(&state, *(uint32_t *)(state.edx)), &state.eax); */ asm_movl(&state, 0x12345678, &state.eax);
	
	asm_movl(&state, chan->curvols[0], &state.ebx);
	asm_shll(&state, 8, &state.ebx);
	asm_movl(&state, chan->curvols[1], &state.ecx);
	asm_shll(&state, 8, &state.ecx);
	asm_movw(&state, chan->fpos, &state.dx);
	asm_shll(&state, 16, &state.edx);
	asm_movl(&state, chan->pos, &state.esi);
	asm_addl(&state, /*chan->samp*/0x12345678, &state.esi);
	asm_movl(&state, 0x10000000, &state.edi); /* buf */

	/* asm_call(&state, *(uint32_t **)(state.eax)); */
	if (buf<orgbuf)
	{
		fprintf(stderr, "#GP buf<orgbuf\n");
	} else if ((buf+(len<<stereo)) > (orgbuf+(orglen<<stereo)))
	{
		fprintf(stderr, "#GP (buf+len) > (orgbuf+orglen)\n");
	} else
		routeptr(buf, bufferlen, chan);

	asm_popl(&state, &state.ecx);
	asm_movl(&state, 0xf0000000, &state.edi); /* &chan == 0xf0000000   only dummy, since we won't deref chan directly */

mixrPlayChannelnoplay:
	asm_movl(&state, state.ecx, &state.eax);
	asm_shll(&state, 2, &state.eax);
	asm_cmpl(&state, 0, stereo);
	asm_je(&state, mixrPlayChannelm2);
		asm_shll(&state, 1, &state.eax);
mixrPlayChannelm2:
	/*asm_addl(&state, state.eax, &buf);*/ buf = (int32_t *)((char *)buf + state.eax);
	/*asm_subl(&state, state.ecx, &len);*/ len -= state.ecx;

	asm_movl(&state, chan->step, &state.eax);
	asm_imul_1_l(&state, state.ecx);
	asm_shldl(&state, 16, state.eax, &state.edx);
	asm_addw(&state, state.eax, &chan->fpos);
	asm_adcl(&state, state.edx, &chan->pos);

	asm_movl(&state, ramping[0], &state.eax);
	asm_imul_2_l(&state, state.ecx, &state.eax);
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+0);
	asm_movl(&state, ramping[1], &state.eax);
	asm_imul_2_l(&state, state.ecx, &state.eax);
	asm_addl(&state, state.eax, (uint32_t *)chan->curvols+1);

	asm_cmpl(&state, 0, ramploop);
	asm_jnz(&state, mixrPlayChannelbigloop);

	asm_cmpl(&state, 0, inloop);
	asm_jz(&state, mixrPlayChannelfill);

	asm_movl(&state, chan->pos, &state.eax);
	asm_cmpl(&state, 0, chan->step);
	asm_jge(&state, mixrPlayChannelforward2);
		asm_cmpl(&state, chan->loopstart, state.eax);
		asm_jge(&state, mixrPlayChannelexit);
		asm_testb(&state, MIXRQ_PINGPONGLOOP, chan->status);

		asm_jnz(&state, mixrPlayChannelpong);
			asm_addl(&state, chan->replen, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelpong:
			asm_negl(&state, (uint32_t *)&chan->step);
			asm_negw(&state, &chan->fpos);
			asm_adcl(&state, 0, &state.eax);
			asm_negl(&state, &state.eax);
			asm_addl(&state, chan->loopstart, &state.eax);
			asm_addl(&state, chan->loopstart, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelforward2:
		asm_cmpl(&state, chan->loopend, state.eax);
		asm_jb(&state, mixrPlayChannelexit);
		asm_testb(&state, MIXRQ_PINGPONGLOOP, chan->status);

		asm_jnz(&state, mixrPlayChannelping);
			asm_subl(&state, chan->replen, &state.eax);
			asm_jmp(&state, mixrPlayChannelloopiflen);
mixrPlayChannelping:
			asm_negl(&state, (uint32_t *)&chan->step);
			asm_negw(&state, &chan->fpos);
			asm_adcl(&state, 0, &state.eax);
			asm_negl(&state, &state.eax);
			asm_addl(&state, chan->loopend, &state.eax);
			asm_addl(&state, chan->loopend, &state.eax);
mixrPlayChannelloopiflen:
	asm_movl(&state, state.eax, &chan->pos);
	asm_cmpl(&state, 0, len);
	asm_jne(&state, mixrPlayChannelbigloop);
	asm_jmp(&state, mixrPlayChannelexit);

mixrPlayChannelfill:
	asm_cmpl(&state, 0, filllen);
	asm_je(&state, mixrPlayChannelfadechk);
	asm_movl(&state, chan->length, &state.eax);
	asm_movl(&state, state.eax, &chan->pos);
	asm_addl(&state, /*chan->samp*/0x12345678, &state.eax);
	asm_movl(&state, chan->curvols[0], &state.ebx);
	asm_movl(&state, chan->curvols[1], &state.ecx);
	asm_shll(&state, 8, &state.ebx);
	asm_shll(&state, 8, &state.ecx);
	asm_testb(&state, MIXRQ_PLAY16BIT, chan->status);

	asm_jnz(&state, mixrPlayChannelfill16);
		asm_movb(&state, chan->realsamp.bit8[state.eax-0x12345678], &state.bl);
		asm_jmp(&state, mixrPlayChannelfilldo);
mixrPlayChannelfill16:
		asm_movb(&state, ((uint16_t *)chan->realsamp.bit16)[state.eax-0x12345678]>>8, &state.bl);
mixrPlayChannelfilldo:
	asm_movb(&state, state.bl, &state.cl);
	asm_movl(&state, ((uint32_t *)mixrFadeChannelvoltab)[state.ebx], &state.ebx);
	asm_movl(&state, ((uint32_t *)mixrFadeChannelvoltab)[state.ecx], &state.ecx);
	asm_movl(&state, filllen, &state.eax);
	asm_movl(&state, /*buf*/0x30000000, &state.edi);
	asm_cmpl(&state, 0, stereo);
	asm_jne(&state, mixrPlayChannelfillstereo);
mixrPlayChannelfillmono:
/*		asm_addl(&state, ebx, &(uint32_t *)state.edi);*/ buf[(state.edi-0x30000000)/4] += state.ebx;
		asm_addl(&state, 4, &state.edi);
	asm_decl(&state, &state.eax);
	asm_jnz(&state, mixrPlayChannelfillmono);
	asm_jmp(&state, mixrPlayChannelfade);
mixrPlayChannelfillstereo:
/*		asm_addl(&state, ebx, &(uint32_t *)state.edi);*/ buf[(state.edi-0x30000000)/4] += state.ebx;
/*		asm_addl(&state, ecx, &(uint32_t *)(state.edi+4));*/ buf[(state.edi-0x30000000+4)/4] += state.ecx;
		asm_addl(&state, 8, &state.edi);
	asm_decl(&state, &state.eax);
	asm_jnz(&state, mixrPlayChannelfillstereo);
	asm_jmp(&state, mixrPlayChannelfade);

mixrPlayChannelfadechk:
	asm_cmpl(&state, 0, dofade);
	asm_je(&state, mixrPlayChannelexit);
mixrPlayChannelfade:
	asm_movl(&state, /*chan*/0xf0000000, &state.edi);
	asm_movl(&state, /*fadebuf*/0xe0000000, &state.esi);
	/*asm_call(&state, mixrFadeChannel);*/ mixrFadeChannel(fadebuf, chan);
mixrPlayChannelexit:
	{
	}
}
#else

void mixrPlayChannel(int32_t *buf, int32_t *fadebuf, uint32_t len, struct channel *chan, int stereo)
{
	uint32_t fillen=0;
	int inloop;
	int ramploop;
	
	int dofade=0;
	int route=0;
	route_func routeptr;
	uint32_t mylen; /* ecx */

	stereo=!!stereo; /* make sure it is 0 or 1 */

	if (!(chan->status&MIXRQ_PLAYING))
		return;

	if (stereo)
		route+=4;

	if (chan->status&MIXRQ_INTERPOLATE)
		route+=2;

	if (chan->status&MIXRQ_PLAY16BIT)
		route++;

mixrPlayChannelbigloop:
	inloop=0;
	mylen=len; /* ecx */

	if (chan->step)
	{
		uint32_t mystep;  /* abs of chan->step */
		uint16_t myfpos; /* -abs of chan->fpos */
		uint32_t mypos;

		/* length = eax */
		if (chan->step<0)
		{ /* mixrPlayChannelbackward: */
			mypos=chan->pos;
			mystep=-chan->step;
			myfpos=chan->fpos;
			if (chan->status&MIXRQ_LOOPED)
			{
				if (chan->pos >= chan->loopstart)
				{
					mypos -= chan->loopstart;
					inloop = 1;
				}
			}
		} else { /* mixrPlayChannelforward */
			mystep=chan->step;
			mypos=chan->length - chan->pos - (!chan->fpos);
			myfpos=-chan->fpos;
			if (chan->status&MIXRQ_LOOPED)
			{
				if (chan->pos < chan->loopend)
				{
					mypos -= chan->length - chan->loopend;
					inloop = 1;
				}
			}
		}
		mixrPlayChannelmaxplaylen: /* here we analyze how much we can sample */
		{			
			uint64_t tmppos;
/*			fprintf(stderr, "Samples available to use: %d/0x%08x\n", mypos, mypos);*/
			tmppos=((((uint64_t)mypos)<<16)|myfpos)+((uint32_t)mystep)-1;
/*			fprintf(stderr, "Samples available to use hidef: %lld/0x%012llx\n", tmppos, tmppos);*/
			if ((tmppos>>32)<mystep)
			{/* this is the safe check to avoid overflow in div */
				uint32_t tmplen;
				tmplen = tmppos / mystep; /* eax */
/*				fprintf(stderr, "Samples at current playrate would be output into %d samples\n", tmplen);*/
				if (mylen>=tmplen)
				{
/*					fprintf(stderr, "world wants more data than we can provide, limi output\n");*/
					mylen=tmplen; /* ecx = eax */
					if (!inloop)
					{ /* add padding */
/*						fprintf(stderr, "We are not in loop, quit sample\n");*/
						dofade=1;
						chan->status&=~MIXRQ_PLAYING;
						fillen+=(len-mylen); /* the gap that is left */
						len=mylen;
					}
				}
			}
		}
	}
	mixrPlayChannelplayecx: /* here we play mylen samples */
	ramploop=0;
	ramping[0]=0;
	ramping[1]=0;
	if (mylen) /* ecx */
	{
		int32_t diff; /* edx */
		if ((diff=chan->dstvols[0]-chan->curvols[0]))
		{
			if (diff>0)
			{
				mixrPlayChannelramp0up:
				ramping[0]=1;
				if (mylen>diff)
				{
					ramploop=1;
					mylen=diff;
				}
			} else {
				mixrPlayChannelramp0down:
				ramping[0]=-1;
				diff=-diff;
				if (mylen>diff)
				{
					ramploop=1;
					mylen=diff;
				}
			}
		}
		mixrPlayChannelnoramp0:

		if ((diff=chan->dstvols[1]-chan->curvols[1]))
		{
			if (diff>0)
			{
				mixrPlayChannelramp1up:
				ramping[1]=1;
				if (mylen>diff)
				{
					ramploop=1;
					mylen=diff;
				}
			} else {
				mixrPlayChannelramp1down:
				ramping[1]=-1;
				diff=-diff;
				if (mylen>diff)
				{
					ramploop=1;
					mylen=diff;
				}
			}
		}
		mixrPlayChannelnoramp1:
		routeptr=routeptrs[route];
		if (!ramping[0]&&!ramping[1]&&!chan->curvols[0]&&!chan->curvols[1])
			routeptr=routequiet;
		mixrPlayChannelnotquiet:
		routeptr(buf, mylen, chan);
	}
mixrPlayChannelnoplay:
	buf+=mylen<<stereo;
	len-=mylen;
	{
		int64_t tmp64;
		int32_t tmp32;
		tmp64=((int64_t)chan->step)*mylen;
		tmp32=(tmp64&0xffff)+(uint16_t)chan->fpos;

		chan->fpos=tmp32&0xffff;
		chan->pos+=(tmp64>>16) + (tmp32>0xffff);
	}
	chan->curvols[0]+=mylen*ramping[0];
	chan->curvols[1]+=mylen*ramping[1];
	if (ramploop)
		goto mixrPlayChannelbigloop;

	if (inloop)
	{
		int32_t mypos = chan->pos; /* eax */
		if (chan->step<0)
		{
		 	if (mypos>=(int32_t)chan->loopstart)
				return;
			if (!(chan->status&MIXRQ_PINGPONGLOOP))
			{
				mypos+=chan->replen;
			} else {
mixrPlayChannelpong:
				chan->step=-chan->step;
				chan->fpos=-chan->fpos;
				mypos+=!!(chan->fpos);
				mypos=-mypos+chan->loopstart+chan->loopstart;
			}
		} else {
mixrPlayChannelforward2:
			if (mypos<chan->loopend)
				return;
			if (!(chan->status&MIXRQ_PINGPONGLOOP))
			{
				mypos-=chan->replen;
			} else {
mixrPlayChannelping:
				chan->step=-chan->step;
				chan->fpos=-chan->fpos;
				mypos+=!!(chan->fpos);
				mypos=-mypos+chan->loopend+chan->loopend;
			}
		}
mixrPlayChannelloopiflen:
		chan->pos=mypos;
		if (len)
			goto mixrPlayChannelbigloop;
		return;
	}
mixrPlayChannelfill:

	if (fillen)
	{
		uint32_t curvols[2];
		chan->pos=chan->length;
		if (chan->status&MIXRQ_PLAY16BIT)
		{
			curvols[0]=mixrFadeChannelvoltab[chan->curvols[0]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
			curvols[1]=mixrFadeChannelvoltab[chan->curvols[1]][((uint16_t)(chan->realsamp.bit16[chan->pos]))>>8];
		} else {
			curvols[0]=mixrFadeChannelvoltab[chan->curvols[0]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
			curvols[1]=mixrFadeChannelvoltab[chan->curvols[1]][(uint8_t)(chan->realsamp.bit8[chan->pos])];
		}
		if (!stereo)
		{
mixrPlayChannelfillmono:
			while (fillen)
			{
				*(buf++)+=curvols[0];
				fillen--;
			}
		} else {
mixrPlayChannelfillstereo:
			while (fillen)
			{
				*(buf++)+=curvols[0];
				*(buf++)+=curvols[1];
				fillen--;
			}
		}
	} else {
mixrPlayChannelfadechk:
		if (!dofade)
			return;
	}
mixrPlayChannelfade:
	mixrFadeChannel(fadebuf, chan);
#if 0
 #error We need assembler replace here
	void *routptr;
	uint32_t filllen,
	         ramping[2];
	int inloop;
	int ramploop;
	int dofade;
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
#endif
		"  movl %3, %%edi\n"              /*  %3 = chan */
		"  testb MIXRQ_PLAYING, %12(%%edi)\n"
		                                  /* %12 = status */
		"  jz mixrPlayChannelexit\n"

		edi = chan
		if (chan->status&MIXRQ_PLAYING) return

		"  movl $0, %6\n"                 /*  %6 = fillen */
		"  movb $0, %11\n"                /* %11 = dofade */

		fillen=0
		dofade=0

		"  xorl %%eax, %%eax\n"

		eax=0
		edi=chan
		fillen=0
		dofade=0

		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelnostereo\n"
		"    addl $4, %%eax\n"
		"mixrPlayChannelnostereo:\n"
		
		if (stereo)
			eax+=4;

		"  testb MIXRQ_INTERPOLATE, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelnointr\n"
		"    addl $2, %%eax\n"

		if (chan->status&MIXRQ_INTERPOLATE)
			eax+=2;

		"mixrPlayChannelnointr:\n"
		"  testb MIXRQ_PLAY16BIT, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"  jz mixrPlayChannelpsetrtn\n"
		"    incl %%eax\n"
		"mixrPlayChannelpsetrtn:\n"

		if (chan->status&MIXRQ_PLAY16BIT)
			eax++;

		"  shll $5, %%eax\n"

		eax<<=5;   /* eax *= sizeof(void *)+3(8 chooices per chan */

		/* a rout entry looks like this
		 * func
		".long   playstereo,      stereostepl-4,   stereosteph-4,   stereoramp0-4,   stereoramp1-4,   stereoendp-4,    0,0\n"
			  \ func               \step low       \ step high         \ ramp0       \ ramp1          \ end of buffer*/

		"  addl $routtab, %%eax\n"
		"  movl %%eax, %5\n"              /*  %5 = routeptr*/

		routeptr=routtab+eax>>5

		"mixrPlayChannelbigloop:\n"
mixrPlayChannelbigloop:

		"  movl %2, %%ecx\n"              /*  %2 = len */
		"  movl %13(%%edi), %%ebx\n"      /* %13 = ch->step */
		"  movl %14(%%edi), %%edx\n"      /* %14 = ch->pos */
		"  movw %15(%%edi), %%si\n"       /* %15 = ch->fpos */
		"  movb $0, %9\n"                 /*  %9 = inloop */

		eax=garbage/routeptr
		ebx=ch->step
		ecx=len
		edx=ch->pos
		edi=chan
		 si=ch->fpos
		inloop=0

		"  cmpl $0, %%ebx\n"
		"  je mixrPlayChannelplayecx\n"
		"  jg mixrPlayChannelforward\n"

		if (!chan->step)
			goto mixrPlayChannelplayecx;
		if (chan->step>0)
			goto mixrPlayChannelforward;
mixrPlayChannelbackward:

		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"

		eax=ch->pos
		ebx=-ch->step (now positive)
		ecx=len
		edx=ch->pos
		edi=chan
		 si=chan->fpos

		"    testb MIXRQ_LOOPED, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"

		if (!chan->status&MIXRQ_LOOPED)
			goto mixrPlayChannelmaxplaylen

		"    cmpi %16(%%edi), %%edx\n"    /* %16 = ch->loopstart */
		"    jb mixrPlayChannelmaxplaylen\n"

		if (ch->pos < ch->loopstart)
			goto mixrPlayChannelmaxplaylen;

		"    subl %16(%%edi), %%eax\n"    /* %16 = ch->loopstart */

		eax=ch->pos - ch->loopstart

		"    movb $1, %9\n"               /*  %9 = inloop */

		inloop = 1
		"    jmp mixrPlayChannelmaxplaylen\n"

		goto mixrPlayChannelmaxplaylen;


		"mixrPlayChannelforward:\n"
mixrPlayChannelforward:

		"    movl %18(%%edi), %%eax\n"    /* %18 = length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"

		eax = chan->length - ch->pos - (!ch->fpos before it was negated)
		ebx = ch->step
		ecx = len
		edx = ch->pos
		 si = ~ch->fpos
		edi = chan

		"    testb MIXRQ_LOOPED, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"    jz mixrPlayChannelmaxplaylen\n"
		if (!chan->status&MIXRQ_LOOPED)
			goto mixrPlayChannelmaxplaylen;
		"    cmpl %17(%%edi), %%edx\n"    /* %17 = ch->loopend */
		"    jae mixrPlayChannelmaxplaylen\n"
		if (ch->pos>=ch->loopend)
			goto mixrPlayChannelmaxplaylen
		"    subl %18(%%edi), %%eax\n"    /* %18 = ch->length */
		"    addl %17(%%edi), %%eax\n"    /* %17 = ch->loopend*/
		"    movb $1, %9\n"               /*  %9 = inloop */
		eax -= ch->length
		eax += ch->loopend
		inloop=1

		"mixrPlayChannelmaxplaylen:\n"
mixrPlayChannelmaxplaylen:

		"  xorl %%edx, %%edx\n"
		eax = length stuff            (length)
		/* if we play reverse with no loop, we can play upto pos samples */
                      ch->pos                  (reverse play)
		/* if we play reverse with loop, but no loop reached, we can play upto pos samples */
                      ch->pos - ch->loopstart  (reverse play, if pos was after loopstart forward motion )
		/* if we play forward with no loop, we can play upto length - pos samples */
		      ch->length - ch->pos - CF (forward play)
		/* if we play forward with loop, and we are inside the loop, we can play until the loopend...   loopend - pos*/
	              ch->length - ch->pos - CF - ch->length + chan->loopend  (forward play, if pos was after loop end)
		      ch->loopend - ch->pos - CF (simplyfied above)
		ebx = abs(ch->step)           (mystep)
		ecx = len
		edx = 0
		edi=ch
		 si=ch->fpos or -ch->fpos     (myfpos)
		"  shld $16, %%eax, %%edx\n"
		edx=length stuff >> 16
		eax=length stuff << 16
		"  shll $16, %%esi\n"
		esi = myfpos << 16
		"  shld $16, %%esi, %%eax\n"
		eax=length stuff & 0xffff0000 | myfpos
		esi=0
		"  addl %%ebx, %%eax\n"
		eax=(length stuff & 0xffff0000 | myfpos) + mystep
		"  adcl $0, %%edx\n"
		if (overflow)
			edx++
		"  subl $1, %%eax\n"
		eax-=1
		"  sbbl $0, %%edx\n"
		if (overflow)
			edx--;
		"  cmpl %%ebx, %%edx\n"
		"  jae mixrPlayChannelplayecx\n"

		eax=(length stuff & 0xffff0000 | myfpos) + mystep
		ebx=abs(ch->step)               (mystep)
		ecx=len
		edx=length stuff >> 16 + overflowstuff - overflowstuff2
		esi=0 ?

		iif (edx>=ebx)    (targetlengde>=step, goto playecx.. else the div bellow will overflow.. we can play over 2^32 of samples with the current rate)
			goto mixrPlayChannelplayecx
		"  divl %%ebx\n"
		eax = edx:eax / ebx
		edx = edx:eax % ebx
		"  cmpl %%eax, %%ecx\n"
		"  jb mixrPlayChannelplayecx\n"
		if (ecx<eax)       ( we can sample more data than the buffer can fit?)
			goto mixrPlayChannelplayecx
		"    movl %%eax, %%ecx\n"
		ecx = eax     (fill len with how many samples we can produce)
		"    cmpb $0, %9\n"               /*  %9 = inloop */
		"    jnz mixrPlayChannelplayecx\n"

		if (inloop) /* if we are going to loop, play the eax length */
			goto mixrPlayChannelplayecx

		"      andb NOT_MIXRQ_PLAYING, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"      movb $1, %11\n"            /* %11 = dofade */
		"      movl %2, %%eax\n"          /*  %2 = len */
		"      subl %%ecx, %%eax\n"
		"      addl %%eax, %6\n"          /*  %6 = filllen */
		"      movl %%ecx, %2\n"          /*  %2 = len */

		ch->status&=~MIXRQ_PLAYING /* mask away playing */
		eax=length (len var)
		eax-=ecx
		fillen+=eax
		(len var) = ecx

		"mixrPlayChannelplayecx:\n"
			eax = junk
			ebx = junk
			ecx =    samples to do
			edx = junk
			esi = junk
			edi =    chan


		"  movb $0, %10\n"                /* %10 = ramploop */
		"  movl $0, %7\n"                 /*  %7 = ramping[0] */
		"  movl $0, %8\n"                 /*  %8 = ramping[1] */

		ramploop=0
		ramping[0]=0
		ramping[1]=0

		"  cmpl $0, %%ecx\n"
		"  je mixrPlayChannelnoplay\n"
		if (!ecx)     (mylen==0)
			goto mixrPlayChannelnoplay

		"  movl %21(%%edi), %%edx\n"      /* %21 = ch->dstvols[0] */
		"  subl %19(%%edi), %%edx\n"      /* %19 = ch->curvols[0] */
		"  je mixrPlayChannelnoramp0\n"
		"  jl mixrPlayChannelramp0down\n"
		"    movl $1, %7\n"               /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp0\n"
		"mixrPlayChannelramp0down:\n"
		"    negl %%edx\n"
		"    movl $-1, %7\n"              /*  %7 = ramping[0] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp0\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"    movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp0:\n"

		"  movl %22(%%edi), %%edx\n"      /* %22 = ch->dstvols[1] */
		"  subl %20(%%edi), %%edx\n"      /* %20 = ch->curvols[1] */
		"  je mixrPlayChannelnoramp1\n"
		"  jl mixrPlayChannelramp1down\n"
		"    movl $1, %8\n"               /*  %8 = ramping[4] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"      jmp mixrPlayChannelnoramp1\n"
		"mixrPlayChannelramp1down:\n"
		"    negl %%edx\n"
		"    movl $-1, %8\n"              /*  %8 = ramping[1] */
		"    cmpl %%edx, %%ecx\n"
		"    jbe mixrPlayChannelnoramp1\n"
		"      movb $1, %10\n"            /* %10 = ramploop */
		"      movl %%edx, %%ecx\n"
		"mixrPlayChannelnoramp1:\n"

		"  movl %5, %%edx\n"              /*  %5 = routptr */
		"  cmpl $0, %7\n"                 /*  %7 = ramping[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %8\n"                 /*  %8 = ramping[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %19(%%edi)\n"         /* %19 = ch->curvols[0] */
		"  jne mixrPlayChannelnotquiet\n"
		"  cmpl $0, %20(%%edi)\n"         /* %20 = ch->curvols[1] */
		"  jne mixrPlayChannelnotquiet\n"
		"    movl $routq, %%edx\n"

		"mixrPlayChannelnotquiet:\n"
		/* ecx = mylen */
		/* edx = router */

		"  movl 4(%%edx), %%ebx\n"
		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  shll $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		routeptr[1]=ch->step<<16

		"  movl 8(%%edx), %%ebx\n"
		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  sarl $16, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		routeptr[2]=ch->step>>16  /* the correct according to the assembler would be to div would be to typecast to int32_t and divide by 65536 */

		"  movl 12(%%edx), %%ebx\n"
		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		routeptr[3]=ramping[0]<<8

		"  movl 16(%%edx), %%ebx\n"
		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  shll $8, %%eax\n"
		"  movl %%eax, (%%ebx)\n"
		routeptr[4]=ramping[1]<<8
	
		"  movl 20(%%edx), %%ebx\n"
		"  leal (,%%ecx,4), %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm1\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm1:\n"
		"  addl %0, %%eax\n"              /*  %0 = buf */
		"  movl %%eax, (%%ebx)\n"
		routeptr[5]=buf[mylen<<stereo]    /* end of buffer in the current run */

		"  pushl %%ecx\n"
		"  movl (%%edx), %%eax\n"
	
		"  movl %19(%%edi), %%ebx\n"      /* %19 = ch->curvols[0] */
		"  shll $8, %%ebx\n"
		ebx=curvols[0]<<8
		"  movl %20(%%edi), %%ecx\n"      /* %20 = ch->curvols[1] */
		"  shll $8, %%ecx\n"
		ecx=curvols[1]<<8
		"  movw %15(%%edi), %%dx\n"       /* %15 = ch->fpos */
		"  shll $16, %%edx\n"
		edx=((uint32_t)ch->fpos)<<16
		"  movl %14(%%edi), %%esi\n"      /* %14 = ch->chpos */
		"  addl %23(%%edi), %%esi\n"      /* %23 = ch->samp */
		esi=ch->chpos+ch->samp
		"  movl %0, %%edi\n"              /*  %0 = buf */
		edi=buf

		"  call *%%eax\n"
		eax=junk
		ebx=curvols[0]<<8
		ecx=curvols[1]<<8
		edx=((uint32_t)ch->fpos)<<16
		esi=ch->chpos+ch->samp
		edi=buf
		+ ch->step<<16
		+ ch->step>>16
		+ ramping[0]
		+ ramping[1]
		+ buf[mylen<<stereo]
		rout()

		"  popl %%ecx\n"
		"  movl %3, %%edi\n"              /*  %3 = chan */
		ecx=mylen
		edi=chan

		"mixrPlayChannelnoplay:\n"
		"  movl %%ecx, %%eax\n"
		"  shll $2, %%eax\n"
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  je mixrPlayChannelm2\n"
		"    shll $1, %%eax\n"
		"mixrPlayChannelm2:\n"
		eax=mylen<<(2+stereo)  /*sizeof(uint32_t)*/
	
		"  addl %%eax, %0\n"              /*  %0 = buf */
		"  subl %%ecx, %2\n"              /*  %2 = len */
		buf+=mylen<<stereo  /* buf is uint32_t *, sizeo it should size itself */
		len-=mylen

		"  movl %13(%%edi), %%eax\n"      /* %13 = ch->step */
		"  imul %%ecx\n"
		(int64_t) edx:eax=ch->step*mylen /* ch->step? is signed, so the mul should be signed */
		"  shld $16, %%eax, %%edx\n"
		edx = edx:eax<<16

		"  addw %%ax, %15(%%edi)\n"       /* %15 = ch->fpos */
		ch->fpos+=ax
		"  adcl %%edx, %14(%%edi)\n"      /* %14 = ch->pos */
		ch->pos+=edx + overflow from above

		"  movl %7, %%eax\n"              /*  %7 = ramping[0] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %19(%%edi)\n"      /* %19 = ch->curvols[0] */
		ch->curvols[0]+=mylen*ramping[0]

		"  movl %8, %%eax\n"              /*  %8 = ramping[1] */
		"  imul %%ecx, %%eax\n"
		"  addl %%eax, %20(%%edi)\n"      /* %20 = ch->curvols[1] */
		ch->curvols[1]+=mylen*ramping[1]

		"  cmpb $0, %10\n"                /* %10 = ramploop */
		"  jnz mixrPlayChannelbigloop\n"
		if (ramploop)
			goto mixrPlayChannelbigloop;

		"  cmpb $0, %9\n"                 /*  %9 = inloop */
		"  jz mixrPlayChannelfill\n"
		if (!inloop)
			goto mixrPlayChannelfill;	

		"  movl %14(%%edi), %%eax\n"      /* %14 = ch->pos */
		eax=ch->pos
		ecx=mylen
		edi=chan

		"  cmpl $0, %13(%%edi)\n"         /* %13 = ch->step */
		"  jge mixrPlayChannelforward2\n"
		if (ch->step>=0)
			goto mixrPlayChannelforward2;
		"    cmpl %16(%%edi), %%eax\n"    /* %16 = ch->loopstart */
		"    jge mixrPlayChannelexit\n"
		if (ch->pos>=ch->loopstart)
			return
		"    testb MIXRQ_PINGPONGLOOP, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelpong\n"
		if (ch->status&MIXRQ_PINGPONGLOOP)
			goto mixrPlayChannelpong;
		"      addl %24(%%edi), %%eax\n"  /* %24 = ch->replen */
		eax+=ch->replen     (eax=ch->pos+ch->replen)
		"      jmp mixrPlayChannelloopiflen\n"
		goto mixrPlayChannelloopiflen;
		"mixrPlayChannelpong:\n"
mixrPlayChannelpong:
		"      negl %13(%%edi)\n"         /* %13 = ch->step */
		ch->step=-ch->step;
		"      negw %15(%%edi)\n"         /* %15 = ch->fpos */
		ch->fpos=-ch->fpos;
		"      adcl $0, %%eax\n"
		eax+=!!(ch->fpos)
		"      negl %%eax\n"
		eax-=eax
		"      addl %16(%%edi), %%eax\n"  /* %16 = ch->loopstart */
		eax+=ch->loopstart
		"      addl %16(%%edi), %%eax\n"  /* %16 = ch->loopstart */
		eax+=ch->loopstart /* since we have 16 bit samples ? */
		"      jmp mixrPlayChannelloopiflen\n"
		goto mixrPlayChannelloopiflen;
		"mixrPlayChannelforward2:\n"
		eax=ch->pos
		"    cmpl %17(%%edi), %%eax\n"    /* %17 = ch->loopend */
		"    jb mixrPlayChannelexit\n"
		if (ch->pos<ch->loopend)
			return;
		"    testb MIXRQ_PINGPONGLOOP, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"    jnz mixrPlayChannelping\n"
		if (ch->status&MIXRQ_PINGPONGLOOP)
			goto mixrPlayChannelping
		"      subl %24(%%edi), %%eax\n"  /* %24 = ch->replen */
		eax-=ch->replen;
		"      jmp mixrPlayChannelloopiflen\n"
		goto mixrPlayChannelloopiflen;
		"mixrPlayChannelping:\n"
		"      negl %13(%%edi)\n"         /* %13 = ch->step */
		"      negw %15(%%edi)\n"         /* %15 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %17(%%edi), %%eax\n"  /* %17 = ch->loopend */
		"      addl %17(%%edi), %%eax\n"  /* %17 = ch->loopend */
		ch->step=-ch->step;
		ch->fstep=-ch->fstep;
		eax+=carry
		eax=-eax
		eax+=ch->loopend;
		eax+=ch->loopend;
		"mixrPlayChannelloopiflen:\n"

		"  movl %%eax, %14(%%edi)\n"      /* %14 = ch->pos */
		ch->pos=eax
		"  cmpl $0, %2\n"                 /*  %2 = len */
		"  jne mixrPlayChannelbigloop\n"
		if (len)
			goto mixrPlayChannelbigloop;
		"  jmp mixrPlayChannelexit\n"
		return;

		"mixrPlayChannelfill:\n"

		"  cmpl $0, %6\n"                 /*  %6 = filllen */
		"  je mixrPlayChannelfadechk\n"
		if (!filllen)
			goto mixrPlayChannelfadechk;
		"  movl %18(%%edi), %%eax\n"      /* %18 = ch->length */
		eax=ch->length
		"  movl %%eax, %14(%%edi)\n"      /* %14 = ch->pos */
		ch->pos=eax
		"  addl %23(%%edi), %%eax\n"      /* %23 = ch->samp */
		eax+=ch->samp
		"  movl %19(%%edi), %%ebx\n"      /* %19 = ch->curvols[0] */
		"  movl %20(%%edi), %%ecx\n"      /* %20 = ch->curvols[1] */
		"  shll $8, %%ebx\n"
		"  shll $8, %%ecx\n"
		eax=ch->length+ch->samp
		ebx=ch->curvols[0]<<8
		ecx=ch->curvols[1]<<8
		"  testb MIXRQ_PLAY16BIT, %12(%%edi)\n"
		                                  /* %12 = ch->status */
		"  jnz mixrPlayChannelfill16\n"
		if (ch->status&MIXRQ_PLAY16BIT)
			goto mixrPlayChannelfill16;
		"    movb (%%eax), %%bl\n"
		bl=*eax
		"    jmp mixrPlayChannelfilldo\n"
		goto mixrPlayChannelfilldo
		"mixrPlayChannelfill16:\n"
		"    movb 1(%%eax, %%eax), %%bl\n"
mixrPlayChannelfill16:
		bl=(eax+eax)>>8
		"mixrPlayChannelfilldo:\n"
mixrPlayChannelfilldo:
		"  movb %%bl, %%cl\n"
		cl=bl
		"  movl 1234(,%%ebx,4), %%ebx\n"
		"mixrPlayChannelvoltab1:\n"
		ebx=mixrPlayChannelvoltab1[ebx]          mixrFadeChannelvoltab works
		"  movl 1234(,%%ecx,4), %%ecx\n"
		"mixrPlayChannelvoltab2:\n"
		ecx=mixrPlayChannelvoltab1[ecx]          mixrFadeChannelvoltab works
		"  movl %6, %%eax\n"              /*  %6 = filllen */
		/* we are here */
	
		eax=fillen
		"  movl %0, %%edi\n"              /*  %0 = buf */
		edi=buf
		"  cmpl $0, %4\n"                 /*  %4 = stereo */
		"  jne mixrPlayChannelfillstereo\n"
		if (stereo)
			goto mixrPlayChannelfillstereo;
		"mixrPlayChannelfillmono:\n"
mixrPlayChannelfillmono:
		"    addl %%ebx,(%%edi)\n"
		"    addl $4, %%edi\n"
		"  decl %%eax\n"
		(*(edi++))+=ebx
		eax--
		"  jnz mixrPlayChannelfillmono\n"
		if (eax)
			goto mixrPlayChannelfillmono
		"  jmp mixrPlayChannelfade\n"
		goto mixrPlayChannelfade
		"mixrPlayChannelfillstereo:\n"
mixrPlayChannelfillstereo:
		"    addl %%ebx, (%%edi)\n"
		"    addl %%ecx, 4(%%edi)\n"
		"    addl $8, %%edi\n"
		(*(edi++))+=ebx
		(*(edi++))+=ecx
		"  decl %%eax\n"
		eax--
		"  jnz mixrPlayChannelfillstereo\n"
		if (eax)
			goto mixrPlayChannelfillstereo
		"  jmp mixrPlayChannelfade\n"
		goto mixrPlayChannelfade

		"mixrPlayChannelfadechk:\n"
		"  cmpb $0, %11\n"                /* %11 = dofade */
		"  je mixrPlayChannelexit\n"
mixrPlayChannelfadechk:
		if (!dofade)
			return
		"mixrPlayChannelfade:\n"
mixrPlayChannelfade:
		"  movl %3, %%edi\n"              /* %3 = chan */
		"  movl %1, %%esi\n"              /* %1 = fadebuf */
		"  call mixrFadeChannel_\n"
		"  jmp mixrPlayChannelexit\n"
		edi=chan
		esi=fadebuf
		call mixrFadeChannel_
		return

	mixrPlayChannelvoltab1-4=eax (vol)
	mixrPlayChannelvoltab2-4=eax (vol)
		"setupplay:\n"
		"  movl %%eax, (mixrPlayChannelvoltab1-4)\n"
		"  movl %%eax, (mixrPlayChannelvoltab2-4)\n"
		"  ret\n"

		"mixrPlayChannelexit:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "m" (buf),                                  /*   0  */
		  "m" (fadebuf),                              /*   1  */
		  "m" (len),                                  /*   2  */
		  "m" (chan),                                 /*   3  */
		  "m" (stereo),                               /*   4  */
		  "m" (routptr),                              /*   5  */
		  "m" (filllen),                              /*   6  */
		  "m" (ramping[0]),                           /*   7  */
		  "m" (ramping[1]),                           /*   8  */
		  "m" (inloop),                               /*   9  */
		  "m" (ramploop),                             /*  10  */
		  "m" (dofade),                               /*  11  */
		  "m" (((struct channel *)NULL)->status),     /*  12  */
		  "m" (((struct channel *)NULL)->step),       /*  13  */
		  "m" (((struct channel *)NULL)->pos),        /*  14  */
		  "m" (((struct channel *)NULL)->fpos),       /*  15  */
		  "m" (((struct channel *)NULL)->loopstart),  /*  16  */
		  "m" (((struct channel *)NULL)->loopend),    /*  17  */
		  "m" (((struct channel *)NULL)->length),     /*  18  */
		  "m" (((struct channel *)NULL)->curvols[0]), /*  19  */
		  "m" (((struct channel *)NULL)->curvols[1]), /*  20  */
		  "m" (((struct channel *)NULL)->dstvols[0]), /*  21  */
		  "m" (((struct channel *)NULL)->dstvols[1]), /*  22  */
		  "m" (((struct channel *)NULL)->samp),       /*  23  */
		  "m" (((struct channel *)NULL)->replen)      /*  24  */
#ifdef __PIC__
		: "memory", "eax", "ecx", "edx", "edi", "esi"
#else
		: "memory", "eax", "ebx", "ecx", "edx", "edi", "esi"
#endif
	);
	#endif
}
#endif

void mixrFade(int32_t *buf, int32_t *fade, int len, int stereo)
{
	int32_t samp0 = fade[0];
	int32_t samp0_save;
	int32_t samp1 = fade[1];
	int32_t samp1_save;
	
	if (!stereo)
	{
		do
		{
			*(buf++)=samp0;

			samp0_save=samp0;
			samp0<<=7; /* remove bottom bits */
			samp0-=samp0_save;
			samp0>>=7;
		} while (--len);
	} else {
		do
		{
			*(buf++)=samp0;
			*(buf++)=samp1;

			samp0_save=samp0;
			samp0<<=7; /* remove bottom bits */
			samp0-=samp0_save;
			samp0>>=7;

			samp1_save=samp1;
			samp1<<=7; /* remove bottom bits */
			samp1-=samp1_save;
			samp1>>=7;
		} while (--len);
	}
	fade[0]=samp0;
	fade[1]=samp1;
}

void mixrClip(void *dst, int32_t *src, int len, void *tab, int32_t max, int b16)
{
	if (!b16)
	{
		uint8_t *_dst=dst;
		const uint16_t (*amptab)[256] = tab;
		const uint16_t *mixrClipamp1 = amptab[0];
		const uint16_t *mixrClipamp2 = amptab[1];
		const uint16_t *mixrClipamp3 = amptab[2];
		const int32_t mixrClipmax=max;
		const int32_t mixrClipmin=-max;
		const uint8_t mixrClipminv =
			(mixrClipamp1[mixrClipmin&0xff]+
			mixrClipamp2[(mixrClipmin&0xff00)>>8]+
			mixrClipamp3[(mixrClipmin&0xff0000)>>16])>>8;
		const uint8_t mixrClipmaxv =
			(mixrClipamp1[mixrClipmax&0xff]+
			mixrClipamp2[(mixrClipmax&0xff00)>>8]+
			mixrClipamp3[(mixrClipmax&0xff0000)>>16])>>8;
		while (len)
		{
			if (*src<mixrClipmin)
			{
				*_dst=mixrClipminv;
			} else if (*src>mixrClipmax)
			{
				*_dst=mixrClipmaxv;
			} else {
				*_dst=
					(mixrClipamp1[*src&0xff]+
					mixrClipamp2[(*src&0xff00)>>8]+
					mixrClipamp3[(*src&0xff0000)>>16])>>8;
			}
			src++;
			_dst++;
			len--;
		}
	} else {
		uint16_t *_dst=dst;
		const uint16_t (*amptab)[256] = (void *)tab;
		const uint16_t *mixrClipamp1 = amptab[0];
		const uint16_t *mixrClipamp2 = amptab[1];
		const uint16_t *mixrClipamp3 = amptab[2];
		const int32_t mixrClipmax=max;
		const int32_t mixrClipmin=-max;
		const uint16_t mixrClipminv =
			mixrClipamp1[(mixrClipmin&0xff)]+
			mixrClipamp2[(mixrClipmin&0xff00)>>8]+
			mixrClipamp3[(mixrClipmin&0xff0000)>>16];
		const uint16_t mixrClipmaxv =
			mixrClipamp1[(mixrClipmax&0xff)]+
			mixrClipamp2[(mixrClipmax&0xff00)>>8]+
			mixrClipamp3[(mixrClipmax&0xff0000)>>16];

		while (len)
		{
			if (*src<mixrClipmin)
			{
				*_dst=mixrClipminv;
			} else if (*src>mixrClipmax)
			{
				*_dst=mixrClipmaxv;
			} else {
				*_dst=
					mixrClipamp1[*src&0xff]+
					mixrClipamp2[(*src&0xff00)>>8]+
					mixrClipamp3[(*src&0xff0000)>>16];
			}
			src++;
			_dst++;
			len--;
		}
	}
}
#endif
