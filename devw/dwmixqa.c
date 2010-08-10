/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * assembler routines for Quality Mixer
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
 *  -kb980717 Tammo Hinrichs <opencp@gmx.net>
 *    -some pentium optimization on inner loops
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -used CS addressing here and there for more optimal cache usage
 *  -ss04????
 */

#include "config.h"
#include <stdio.h>
#include "types.h"
#include "dwmix.h"
#include "dwmixqa.h"

#ifdef I386_ASM

#ifndef NULL
#define NULL (void *)0
#endif

void remap_range2_start(void){}

void noexternal_dwmixq(void)
{
	__asm__ __volatile__
	(
		"playquiet:\n"
		"  ret\n"
	);

/* esi = current source
 * edi = current destination
 *
 * esi:ebx = 64 bit source index
 * ebp:edx = 64 bit pitch
 * ecx = number of target samples
 * ----------------------------------
 * ecx = target buffer end marker
 * ebx,eax = temp buffers
 */

	__asm__ __volatile__
	(
		"playmono:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  xorb %bl, %bl\n"
		"playmono_lp:\n"
		"    movb (%esi), %bh\n"
		"    addl $2, %edi\n"
		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    movw %bx, -2(%edi)\n"
		"  cmpl %ecx, %edi\n"
		"  jb playmono_lp\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmono16:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  xorb %bl, %bl\n"
		"playmono16_lp:\n"
		"  movw (%esi, %esi), %bx\n"
		"  addl $2, %edi\n"
		"  addl %edx, %ebx\n"
		"  adcl %ebp, %esi\n"
		"  movw %bx, -2(%edi)\n"
		"  cmpl %ecx, %edi\n"
		"  jb playmono16_lp\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi_endp-4)\n"
		"  xorl %eax, %eax\n"
		"  movl %ebx, %ecx\n"

		"playmonoi_lp:\n"
		"  shrl $19, %ecx\n"
		"  addl $2, %edi\n"
		"  movl %ecx, %eax\n"
		"  movb 0(%esi), %cl\n"
		"  movb 1(%esi), %al\n"
		"  addl %edx, %ebx\n"
		"  movw %es:1234(,%ecx,4), %bx\n"
		"    playmonoi_intr1:\n"
		"  adcl %ebp, %esi\n"
		"  addw %es:1234(, %eax, 4), %bx\n"
		"    playmonoi_intr2:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi_endp:\n"
		"  jb playmonoi_lp\n"
		"  ret\n"

		"setupmonoi:\n"
		"  movl %ebx, (playmonoi_intr1-4)\n"
		"  addl $2, %ebx\n"
		"  movl %ebx, (playmonoi_intr2-4)\n"
		"  subl $2, %ebx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi16:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi16_endp-4)\n"
		"  movl %eax, %eax\n" /* pentium pipeline */
		"  movl %ebx, %ecx\n"

		"playmonoi16_lp:\n"
		"    shrl $19, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"
		"    movb 1(%esi,%esi), %cl\n"
		"    movb 3(%esi,%esi), %al\n"
		"    movw %es:1234(,%ecx,4), %bx\n"
		"      playmonoi16_intr1:\n"
		"    movb (%esi, %esi), %cl\n"
		"    addw 1234(,%eax,4), %bx\n"
		"      playmonoi16_intr2:\n"
		"    movb 2(%esi, %esi), %al\n"
		"    addw %es:1234(,%ecx,4), %bx\n"
		"      playmonoi16_intr3:\n"
		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw 1234(,%eax,4), %bx\n"
		"      playmonoi16_intr4:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi16_endp:\n"
		"  jb playmonoi16_lp\n"
		"  ret\n"

		"setupmonoi16:\n"
		"  movl %ebx, (playmonoi16_intr1-4)\n"
		"  addl $2, %ebx\n"
		"  movl %ebx, (playmonoi16_intr2-4)\n"
		"  addl $32768, %ebx\n" /* 4*32*256 */
		"  movl %ebx, (playmonoi16_intr4-4)\n"
		"  subl $2, %ebx\n"
		"  movl %ebx, (playmonoi16_intr3-4)\n"
		"  subl $32768, %ebx\n" /* 4*32*256 - 2 */
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi2:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi2_endp-4)\n"
		"  movl %eax, %eax\n" /* pipeline */
		"  movl %ebx, %ecx\n"

		"playmonoi2_lp:\n"
		"    shrl $20, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"
		"    movb 0(%esi), %cl\n"
		"    movb 1(%esi), %al\n"

		"    movw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi2_intr1:\n"
		"    movb 2(%esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi2_intr2:\n"

		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi2_intr3:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi2_endp:\n"
		"  jb playmonoi2_lp\n"

		"  ret\n"

		"setupmonoi2:\n"
		"  movl %ecx, (playmonoi2_intr1-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi2_intr2-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi2_intr3-4)\n"
		"  subl $4, %ecx\n"
		"  ret\n"
	);

	__asm__ __volatile__
	(
		"playmonoi216:\n"
		"  leal (%edi, %ecx, 2), %ecx\n"
		"  movl %ecx, (playmonoi216_endp-4)\n"
		"  movl %ebx, %ecx\n"
		"  movl %eax, %eax\n" /* pipeline*/

		"playmonoi216_lp:\n"
		"    shrl $20, %ecx\n"
		"    addl $2, %edi\n"
		"    movl %ecx, %eax\n"

		"    movb 1(%esi, %esi), %cl\n"
		"    movb 3(%esi, %esi), %al\n"

		"    movw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr1:\n"

		"    movb 5(%esi, %esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr2:\n"

		"    movb 0(%esi,%esi), %al\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr3:\n"

		"    movb 2(%esi,%esi), %cl\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr4:\n"

		"    movb 4(%esi, %esi), %al\n"
		"    addw %es:1234(,%ecx,8), %bx\n"
		"      playmonoi216_intr5:\n"

		"    addl %edx, %ebx\n"
		"    adcl %ebp, %esi\n"
		"    addw 1234(,%eax,8), %bx\n"
		"      playmonoi216_intr6:\n"

		"    movw %bx, -2(%edi)\n"
		"    movl %ebx, %ecx\n"
		"  cmpl $1234, %edi\n"
		"    playmonoi216_endp:\n"
		"  jb playmonoi216_lp\n"
		"  ret\n"

		"setupmonoi216:\n"
		"  movl %ecx, (playmonoi216_intr1-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr2-4)\n"
		"  addl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr3-4)\n"
		"  addl $32768, %ecx\n" /* 8*16*256 */
		"  movl %ecx, (playmonoi216_intr6-4)\n"
		"  subl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr5-4)\n"
		"  subl $2, %ecx\n"
		"  movl %ecx, (playmonoi216_intr4-4)\n"
		"  subl $32768, %ecx\n" /* 8*16*256 */
		"  ret\n"
	);
}


#ifndef __PIC__
static void *playrout;
#endif

void mixqPlayChannel(int16_t *buf, uint32_t len, struct channel *ch, int quiet)
{
	int inloop;
	uint32_t filllen;
#ifdef __PIC__
	void *playrout=NULL;
#endif
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
#endif
		"  movl $0, %5\n"                 /*  %5 = fillen */

		"  movl %2, %%edi\n"              /*  %2 = ch */
		"  cmpb $0, %3\n"                 /*  %3 = quiet */
		"  jne mixqPlayChannel_pquiet\n"

		"  testb %16, %6(%%edi)\n"        /* %16 = MIXQ_INTERPOLATE
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intr\n"
		"    movl $playmono, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmono16, %%eax\n"
		"  jmp mixqPlayChannel_intrfini\n"
		"mixqPlayChannel_intr:\n"

		"  testb %18, %6(%%edi)\n"        /* %18 = MIXQ_INTERPOLATEMAX
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intrmax\n"
		"    movl $playmonoi, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi16, %%eax\n"
		"  jmp mixqPlayChannel_intrfini\n"
		"mixqPlayChannel_intrmax:\n"

		"    movl $playmonoi2, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi216, %%eax\n"
		" mixqPlayChannel_intrfini:\n"

		"  movl %%eax, %7\n"              /*  %7 = playrout */
		"  jmp mixqPlayChannel_bigloop\n"

		"mixqPlayChannel_pquiet:\n"

		"  movl $playquiet, %7\n"         /*  %7 = playrout */

		"mixqPlayChannel_bigloop:\n"

		"  movl %1, %%ecx\n"              /*  %1 = len */
		"  movl %8(%%edi), %%ebx\n"       /*  %8 = ch->step */
		"  movl %9(%%edi), %%edx\n"       /*  %9 = ch->pos */
		"  movw %10(%%edi), %%si\n"       /* %10 = ch->fpos */
		"  movb $0, %4\n"                 /*  %4 = inloop */
		"  cmpl $0, %%ebx\n"
		"  je mixqPlayChannel_playecx\n"
		"  jg mixqPlayChannel_forward\n"
		"    negl %%ebx\n"
		"    movl %%edx, %%eax\n"
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		"    cmpl %11(%%edi), %%edx\n"    /* %11 = ch->loopstart */
		"    jb mixqPlayChannel_maxplaylen\n"
		"    subl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		"    movb $1, %4\n"               /*  %4 = inloop */
		"    jmp mixqPlayChannel_maxplaylen\n"
		"mixqPlayChannel_forward:\n"

		"    movl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		"    negw %%si\n"
		"    sbbl %%edx, %%eax\n"
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		"    cmpl %12(%%edi), %%edx\n"    /* %12 = ch->loopend */
		"    jae mixqPlayChannel_maxplaylen\n"
		"    subl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		"    addl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */
		"    movb $1, %4\n"               /*  %4 = inloop */


		"mixqPlayChannel_maxplaylen:\n"

		"  xorl %%edx, %%edx\n"
		"  shld $16, %%eax, %%edx\n"
		"  shll $16, %%esi\n"
		"  shld $16, %%esi, %%eax\n"
		"  addl %%ebx, %%eax\n"
		"  adcl $0, %%edx\n"
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		"  cmpl %%ebx, %%edx\n"
		"  jae mixqPlayChannel_playecx\n"
		"  divl %%ebx\n"
		"  cmpl %%eax, %%ecx\n"
		"  jb mixqPlayChannel_playecx\n"
		"    movl %%eax, %%ecx\n"
		"    cmpb $0, %4\n"               /*  %4 = inloop */
		"    jnz mixqPlayChannel_playecx\n"
		"      andb %20, %6(%%edi)\n"     /* %20 = MIXQ_PLAYING^255
		                                   *  %6 = ch->status */
		"      movl %1, %%eax\n"          /*  %1 = len */
		"      subl %%ecx, %%eax\n"
		"      addl %%eax, %5\n"          /*  %5 = filllen */
		"      movl %%ecx, %1\n"          /*  %1 = len */

		"mixqPlayChannel_playecx:\n"

		"  pushl %%ebp\n"
		"  pushl %%edi\n"
		"  pushl %%ecx\n"

#ifdef __PIC__
		/* We are going to kill ebp, so this is needed, since playrout is now a local variable */
		"  movl  %7, %%eax\n " /* step 1 */
		"  pushl %%eax\n"      /* step 2 */
#endif
		"  movw %10(%%edi), %%bx\n"       /* %10 = ch->fpos */
		"  shll $16, %%ebx\n"
		"  movl %0, %%eax\n"              /*  %0 = buf */

		"  movl %8(%%edi), %%edx\n"       /*  %8 = ch->step */
		"  shll $16, %%edx\n"

		"  movl %9(%%edi), %%esi\n"       /*  %9 = ch->pos */
		"  movl %8(%%edi), %%ebp\n"       /*  %8 = ch->step */
		"  sarl $16, %%ebp\n"
		"  addl %15(%%edi), %%esi\n"      /* %15 = ch->samp */
		"  movl %%eax, %%edi\n"

#ifdef __PIC__
		"  popl %%eax\n"  /* step 3 */
		"  call *%%eax\n" /* step 4 */
#else
		"  call *%7\n"                    /*  %7 = playrout */
#endif
		"  popl %%ecx\n"
		"  popl %%edi\n"
		"  popl %%ebp\n"

		"  addl %%ecx, %0\n"              /*  %0 = buf */
		"  addl %%ecx, %0\n"              /*  %0 = buf */
		"  subl %%ecx, %1\n"              /*  %1 = len */

		"  movl %8(%%edi), %%eax\n"       /*  %8 = ch->step */
		"  imul %%ecx\n"
		"  shld $16, %%eax, %%edx\n"
		"  addw %%ax, %10(%%edi)\n"       /* %10 = ch->fpos */
		"  adcl %%edx, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  movl %9(%%edi), %%eax\n"       /*  %9 = ch->pos */

		"  cmpb $0, %4\n"                 /*  %4 = inloop */
		"  jz mixqPlayChannel_fill\n"

		"  cmpl $0, %8(%%edi)\n"          /*  %8 = ch->step */
		"  jge mixqPlayChannel_forward2\n"
		"    cmpl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		"    jge mixqPlayChannel_exit\n"

		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_pong\n"
		"      addl %13(%%edi), %%eax\n"  /* %13 = ch->replen */
		"      jmp mixqPlayChannel_loopiflen\n"
		"    mixqPlayChannel_pong:\n"

		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fpos */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		"      jmp mixqPlayChannel_loopiflen\n"
		"mixqPlayChannel_forward2:\n"

		"    cmpl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */

		"    jb mixqPlayChannel_exit\n"

		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_ping\n"

		"      subl %13(%%edi), %%eax\n"  /* %13 = ch->replen */

		"      jmp mixqPlayChannel_loopiflen\n"

		"    mixqPlayChannel_ping:\n"

		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fstep */
		"      adcl $0, %%eax\n"
		"      negl %%eax\n"
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */

		"mixqPlayChannel_loopiflen:\n"

		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  cmpl $0, %1\n"                 /*  %1 = len */
		"  jne mixqPlayChannel_bigloop\n"

		"mixqPlayChannel_fill:\n"

		"  cmpl $0, %5\n"                 /*  %5 = filllen */
		"  je mixqPlayChannel_exit\n"
		"  movl %14(%%edi), %%eax\n"      /* %14 = ch->length */
		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  addl %15(%%edi), %%eax\n"      /* %15 = ch->samp */
		"  testb %17, %6(%%edi)\n"        /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_fill16\n"
		"    movb (%%eax), %%ah\n"
		"    movb $0, %%al\n"
		"    jmp mixqPlayChannel_filldo\n"
		"mixqPlayChannel_fill16:\n"
		"    movw (%%eax, %%eax), %%ax\n"
		"mixqPlayChannel_filldo:\n"
		"  movl %5, %%ecx\n"              /*  %5 = filllen */
		"  movl %0, %%edi\n"              /*  %0 = buf */
		"  rep stosw\n"
		"mixqPlayChannel_exit:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		:
		: "m" (buf),                                 /*   0  */
		  "m" (len),                                 /*   1  */
		  "m" (ch),                                  /*   2  */
		  "m" (quiet),                               /*   3  */
		  "m" (inloop),                              /*   4  */
		  "m" (filllen),                             /*   5  */
		  "m" (((struct channel *)NULL)->status),    /*   6  */
		  "m" (playrout),                            /*   7  */
		  "m" (((struct channel *)NULL)->step),      /*   8  */
		  "m" (((struct channel *)NULL)->pos),       /*   9  */
	      	  "m" (((struct channel *)NULL)->fpos),      /*  10  */
		  "m" (((struct channel *)NULL)->loopstart), /*  11  */
		  "m" (((struct channel *)NULL)->loopend),   /*  12  */
		  "m" (((struct channel *)NULL)->replen),    /*  13  */
		  "m" (((struct channel *)NULL)->length),    /*  14  */
		  "m" (((struct channel *)NULL)->samp),      /*  15  */
		  "n" (MIXQ_INTERPOLATE),                    /*  16  */
		  "n" (MIXQ_PLAY16BIT),                      /*  17  */
		  "n" (MIXQ_INTERPOLATEMAX),                 /*  18  */
		  "n" (MIXQ_LOOPED),                         /*  19  */
		  "n" (255-MIXQ_PLAYING),                    /*  20  */
		  "n" (MIXQ_PINGPONGLOOP)                    /*  21  */
		:
#ifndef __PIC__
		"ebx",
#endif
		"memory", "eax", "ecx", "edx", "edi", "esi"
	);
}

void mixqAmplifyChannel(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(

#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannel_ploop:\n"
		"    movl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannel_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannel_voltab2:\n"
		"    movb 1(%%esi), %%bl\n"
		"    movswl  %%ax, %%eax\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannel_ploop\n"
		"  jmp mixqAmplifyChannel_done\n"

		"setupampchan:\n"
		"  movl %%eax, (mixqAmplifyChannel_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannel_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"
		"mixqAmplifyChannel_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif
	);
}

void mixqAmplifyChannelUp(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannelUp_ploop:\n"
		"    movl 1234(%%ebx,%%ebx), %%eax\n"
		"      mixqAmplifyChannelUp_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelUp_voltab2:\n"
		"    addl $512, %%ebx\n"
		"    movswl %%ax, %%eax\n"
		"    movb 1(%%esi), %%bl\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannelUp_ploop\n"
		"  jmp mixqAmplifyChannelUp_done\n"

		"setupampchanup:\n"
		"  movl %%eax, (mixqAmplifyChannelUp_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannelUp_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"

		"mixqAmplifyChannelUp_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif
	);
}

void mixqAmplifyChannelDown(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	int d0, d1, d2, d3;
	__asm__ __volatile__
	(
#ifdef __PIC__
		"pushl %%ebx\n"
		"movl %%eax, %%ebx\n"
#endif
		"  shll $9, %%ebx\n"
		"  movb 1(%%esi), %%bl\n"

		"mixqAmplifyChannelDown_ploop:\n"
		"    movl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelDown_voltab1:\n"
		"    movb (%%esi), %%bl\n"
		"    addl $2, %%esi\n"
		"    addl 1234(%%ebx, %%ebx), %%eax\n"
		"      mixqAmplifyChannelDown_voltab2:\n"
		"    subl $512, %%ebx\n"
		"    movswl %%ax, %%eax\n"
		"    movb 1(%%esi), %%bl\n"
		"    addl %%eax, (%%edi)\n"
		"    addl %%edx, %%edi\n"
		"  decl %%ecx\n"
		"  jnz mixqAmplifyChannelDown_ploop\n"
		"  jmp mixqAmplifyChannelDown_done\n"

		"setupampchandown:\n"
		"  movl %%eax, (mixqAmplifyChannelDown_voltab1-4)\n"
		"  addl $512, %%eax\n"
		"  movl %%eax, (mixqAmplifyChannelDown_voltab2-4)\n"
		"  subl $512, %%eax\n"
		"  ret\n"

		"mixqAmplifyChannelDown_done:\n"
#ifdef __PIC__
		"popl %%ebx\n"
#endif
		: "=&S" (d0),
		  "=&D" (d1),
#ifdef __PIC__
		  "=&a" (d2),
#else
		  "=&b" (d2),
#endif
		  "=&c" (d3)
		: "0" (src),
		  "1" (buf),
		  "2" (vol),
		  "3" (len),
		  "d" (step)
		: "memory"
#ifndef __PIC__
		, "eax"
#endif

	);
}

void mixqSetupAddresses(int16_t (*voltab)[2][256], int16_t (*intrtab1)[32][256][2], int16_t (*intrtab2)[16][256][4])
{
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
		"movl  %%edx, %%ebx\n"
#endif
		"  call setupampchan\n"
		"  call setupampchanup\n"
		"  call setupampchandown\n"
		"  call setupmonoi\n"
		"  call setupmonoi16\n"
		"  call setupmonoi2\n"
		"  call setupmonoi216\n"
#ifdef __PIC__
	 	"popl %%ebx\n"
#endif
		:
		: "a" (voltab),
#ifdef __PIC__
		  "d" (intrtab1),
#else
		  "b" (intrtab1),
#endif
		  "c" (intrtab2)
	);
}

void remap_range2_stop(void){}

#else

static int16_t (*myvoltab)[2][256];
static int16_t (*myinterpoltabq)[32][256][2];
static int16_t (*myinterpoltabq2)[16][256][4];

void mixqSetupAddresses(int16_t (*voltab)[2][256], int16_t (*interpoltabq)[32][256][2], int16_t (*interpoltabq2)[16][256][4])
{
	myvoltab=voltab;
	myinterpoltabq=interpoltabq;
	myinterpoltabq2=interpoltabq2;
/*{
	__asm__ __volatile__
	(
#ifdef __PIC__
	 	"pushl %%ebx\n"
		"movl  %%edx, %%ebx\n"
#endif
		"  call setupampchan\n"
		"  call setupampchanup\n"
		"  call setupampchandown\n"
#ifdef __PIC__
	 	"popl %%ebx\n"
#endif
		:
		: "a" (voltab),
#ifdef __PIC__
		  "d" (interpoltabq),
#else
		  "b" (interpoltabq),
#endif
		  "c" (interpoltabq2)
	);
}*/
}

static void playquiet(int16_t *buf, uint32_t len, struct channel *chan)
{
}

static void playmono(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;
	while (len)
	{
		*(buf++)=chan->realsamp.bit8[pos]<<8;
		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;
	}
}

static void playmono16(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;
	while (len)
	{
		*(buf++)=chan->realsamp.bit16[pos];
		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;
	}
}

static void playmonoi(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;
	while (len)
	{
		uint8_t _fpos = fpos>>11;
		*(buf++)= myinterpoltabq[0][_fpos][(uint8_t)chan->realsamp.bit8[pos]][0]
		          +
		          myinterpoltabq[0][_fpos][(uint8_t)chan->realsamp.bit8[pos+1]][1];
		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;
	}
}

static void playmonoi16(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;
	while (len)
	{
		uint8_t _fpos = fpos>>11;


		*(buf++)= myinterpoltabq[0][_fpos][(uint8_t)(chan->realsamp.bit16[pos]>>8)][0]
		          +
		          myinterpoltabq[0][_fpos][(uint8_t)(chan->realsamp.bit16[pos+1]>>8)][1]
			  +
		          myinterpoltabq[1][_fpos][(uint8_t)(chan->realsamp.bit16[pos]&0xff)][0]
		          +
		          myinterpoltabq[1][_fpos][(uint8_t)(chan->realsamp.bit16[pos+1]&0xff)][1];
		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;
	}
}


static void playmonoi2(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;

	while (len)
	{
		uint8_t _fpos = fpos>>12;

		*(buf++)= myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit8[pos])][0]
		          +
		          myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit8[pos+1])][1]
		          +
		          myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit8[pos+2])][2];
		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;

	}
}

static void playmonoi216(int16_t *buf, uint32_t len, struct channel *chan)
{
	uint32_t fpos=chan->fpos;
	uint32_t fadd=chan->step&0xffff;
	uint32_t posadd=(int16_t)(chan->step>>16);
	uint32_t pos=chan->pos;
	while (len)
	{
		uint8_t _fpos = fpos>>12;


		*(buf++)= myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit16[pos]>>8)][0]
		          +
		          myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit16[pos+1]>>8)][1]
		          +
		          myinterpoltabq2[0][_fpos][(uint8_t)(chan->realsamp.bit16[pos+2]>>8)][2]
			  +
		          myinterpoltabq2[1][_fpos][(uint8_t)(chan->realsamp.bit16[pos]&0xff)][0]
		          +
		          myinterpoltabq2[1][_fpos][(uint8_t)(chan->realsamp.bit16[pos+1]&0xff)][1]
		          +
		          myinterpoltabq2[1][_fpos][(uint8_t)(chan->realsamp.bit16[pos+2]&0xff)][2];

		fpos+=fadd;
		if (fpos&0xffff0000)
		{
			pos++;
			fpos&=0x0000ffff;
		}
		pos+=posadd;
		len--;
	}
}

void mixqPlayChannel(int16_t *buf, uint32_t len, struct channel *chan, int quiet)
{
	uint32_t fillen=0;
	uint32_t mylen;
	int inloop;

	void (*playrout)(int16_t *buf, uint32_t len, struct channel *chan);

	if (quiet)
	{
		playrout=playquiet;
	} else {
		if (chan->status&MIXQ_INTERPOLATE)
		{
			if (chan->status&MIXQ_INTERPOLATEMAX)
			{
				if (chan->status&MIXQ_PLAY16BIT)
				{
					playrout=playmonoi216;
				} else {
					playrout=playmonoi2;
				}
			} else {
				if (chan->status&MIXQ_PLAY16BIT)
				{
					playrout=playmonoi16;
				} else {
					playrout=playmonoi;
				}
			}
		} else {
			if (chan->status&MIXQ_PLAY16BIT)
			{
				playrout=playmono16;
			} else {
				playrout=playmono;
			}
		}
	}

mixqPlayChannel_bigloop:
	inloop=0;
	mylen=len;

	if (chan->step)
	{
		uint32_t mystep; /* ebx */
		uint16_t myfpos;
		uint32_t mypos; /* samples left before end or fold */

		if (chan->step<0)
		{
			mypos=chan->pos;
			mystep=-chan->step;
			myfpos=chan->fpos;
			if (chan->status&MIXQ_LOOPED)
			{				
				if (chan->pos >= chan->loopstart)
				{					
					mypos -= chan->loopstart;		
					inloop = 1;
				}
			}
		} else {
		mixqPlayChannel_forward:
			mystep=chan->step;
			mypos=chan->length - chan->pos - (!chan->fpos);
			myfpos=-chan->fpos;
			if (chan->status&MIXQ_LOOPED)
			{
				if (chan->pos < chan->loopend)
				{
					mypos -=  chan->length - chan->loopend;
					inloop = 1;
				}
			}
		}
		/* mypos should now be the end of the loop envelope */
	mixqPlayChannel_maxplaylen:
		{
			uint64_t tmppos;
/*			fprintf(stderr, "Samples available to use: %d/0x%08x\n", mypos, mypos); */
			tmppos=((((uint64_t)mypos)<<16)|myfpos)+((uint32_t)mystep)-1;
/*			fprintf(stderr, "Samples available to use hidef: %lld/0x%012llx\n", tmppos, tmppos);*/
			if ((tmppos>>32)<mystep)
			{
				/* mypos = samples until barrier */
				/* tmppos = mypos.myfpos + mystep-1 = 32.16 sample pos after route? */
				uint32_t tmplen;
				tmplen=tmppos/mystep;
/*				fprintf(stderr, "Samples at current playrate would be output into %d samples\n", tmplen); */
				/* tmplen = samples of output data can this generate */
				if (mylen>=tmplen)
				{
					/* output data requested is more than can be generated */
/*					fprintf(stderr, "world wants more data than we can provide, limit output\n");*/
					mylen=tmplen;
					/* data requested is now equal to what can be generated */
					if (!inloop)
					{
/*						fprintf(stderr, "We are not in loop, quit sample\n");*/
						/* no loop.. we hit EOF */
						chan->status&=~MIXQ_PLAYING;
						fillen+=(len-tmplen);
						len=mylen;
					}
				}
			}
		}
	}
mixqPlayChannel_playecx:
	playrout(buf, mylen, chan);
	buf+=mylen;
	len-=mylen;
	{
		int64_t tmp64;
		int32_t tmp32;
		tmp64=((int64_t)chan->step)*mylen;
		tmp32=(tmp64&0xffff)+(uint16_t)chan->fpos;

		chan->fpos=tmp32&0xffff;
		chan->pos+=(tmp64>>16)+(tmp32>0xffff);
	}

	if (inloop)
	{
		int32_t mypos = chan->pos; /* eax */
		if (chan->step<0)
		{
			if (mypos>=(int32_t)chan->loopstart)
				return;
			if (!(chan->status&MIXQ_PINGPONGLOOP))
			{
				mypos+=chan->replen;
			} else {
			mixqPlayChannel_pong:
				chan->step=-chan->step;
				if ((chan->fpos=-chan->fpos))
					mypos++;
				mypos=chan->loopstart+chan->loopstart-mypos;
			}
		} else {
		mixqPlayChannel_forward2:
			if (mypos<chan->loopend)
				return;
			if (!(chan->status&MIXQ_PINGPONGLOOP))
			{
				mypos-=chan->replen;
			} else {
			mixqPlayChannel_ping:
				chan->step=-chan->step;
				if ((chan->fpos=-chan->fpos))
					mypos++;
				mypos=chan->loopend+chan->loopend-mypos;
			}
		}
mixqPlayChannel_loopiflen:
		chan->pos=mypos;
		if (len)
			goto mixqPlayChannel_bigloop;
	}
mixqPlayChannel_fill:
	if (fillen)
	{
		int16_t sample;
		int count;
		chan->pos=chan->length;
		if (!(chan->status&MIXQ_PLAY16BIT))
		{
			sample=chan->realsamp.bit8[chan->pos]<<8;
		} else {
			mixqPlayChannel_fill16:
			sample=chan->realsamp.bit16[chan->pos];
		}
		for (count=0;count<fillen;count++)
			*(buf++)=sample;
	}
#if 0
	int inloop;
	uint32_t filllen;
	void *playrout=NULL;
	__asm__ __volatile__
	(
		"  movl $0, %5\n"                 /*  %5 = fillen */

		"  movl %2, %%edi\n"              /*  %2 = ch */
		edi=chan
		"  cmpb $0, %3\n"                 /*  %3 = quiet */
		"  jne mixqPlayChannel_pquiet\n"
		if (quiet)
			goto mixqPlayChannel_pquiet;

		"  testb %16, %6(%%edi)\n"        /* %16 = MIXQ_INTERPOLATE
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intr\n"
		if (chan->status&MIXQ_INTERPOLATE)
			goto mixqPlayChannel_intr;

		"    movl $playmono, %%eax\n"
		eax=playmono
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		if (!chan->status&MIXQ_PLAY16BIT)
			goto mixqPlayChannel_intrfini
		"      movl $playmono16, %%eax\n"
		eax=playmono16
		"  jmp mixqPlayChannel_intrfini\n"
		goto mixqPlayChannel_intrfini
		"mixqPlayChannel_intr:\n"
mixqPlayChannel_intr:

		"  testb %18, %6(%%edi)\n"        /* %18 = MIXQ_INTERPOLATEMAX
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_intrmax\n"
		"    movl $playmonoi, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi16, %%eax\n"
		"  jmp mixqPlayChannel_intrfini\n"
		"mixqPlayChannel_intrmax:\n"

		"    movl $playmonoi2, %%eax\n"
		"    testb %17, %6(%%edi)\n"      /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_intrfini\n"
		"      movl $playmonoi216, %%eax\n"
		" mixqPlayChannel_intrfini:\n"

		"  movl %%eax, %7\n"              /*  %7 = playrout */
		"  jmp mixqPlayChannel_bigloop\n"

		"mixqPlayChannel_pquiet:\n"
		edi=chan  (from outer loop)

		"  movl $playquiet, %7\n"         /*  %7 = playrout */

		"mixqPlayChannel_bigloop:\n"
mixqPlayChannel_bigloop:
		edi=chan  (from quiet)

		"  movl %1, %%ecx\n"              /*  %1 = len */
		"  movl %8(%%edi), %%ebx\n"       /*  %8 = ch->step */
		"  movl %9(%%edi), %%edx\n"       /*  %9 = ch->pos */
		"  movw %10(%%edi), %%si\n"       /* %10 = ch->fpos */
		ecx=len
		ebx=chan->step
		edx=chan->pos
		 si=chan->fpos
		edi=chan
		"  movb $0, %4\n"                 /*  %4 = inloop */
		inloop=0
		"  cmpl $0, %%ebx\n"
		"  je mixqPlayChannel_playecx\n"
		"  jg mixqPlayChannel_forward\n"
		if (!chan->step)
			goto mixqPlayChannel_playecx;
		if (chan->step>0)
			goto mixqPlayChannel_forward;
		"    negl %%ebx\n"
		ebx=-ebx
		"    movl %%edx, %%eax\n"
		eax=edx
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		if (!chan->status&MIXQ_LOOPED)
			goto mixqPlayChannel_maxplaylen;

		"    cmpl %11(%%edi), %%edx\n"    /* %11 = ch->loopstart */
		"    jl mixqPlayChannel_maxplaylen\n"
		if (chan->pos<ch->loopstart)
			goto mixqPlayChannel_maxplaylen;
		"    subl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		eax=chan->pos-chan->loopstart;
		"    movb $1, %4\n"               /*  %4 = inloop */
		inloop=1;
		"    jmp mixqPlayChannel_maxplaylen\n"
		goto mixqPlayChannel_maxplaylen
		"mixqPlayChannel_forward:\n"
mixqPlayChannel_forward:

		"    movl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		eax=ch->length
		"    negw %%si\n"
		si=-si
		"    sbbl %%edx, %%eax\n"
		eax-=edx (and subtract the si carry part aswell)
		"    testb %19, %6(%%edi)\n"      /* %19 = MIXQ_LOOPED
		                                   *  %6 = ch->status */
		"    jz mixqPlayChannel_maxplaylen\n"
		if (!chan->status&MIXQ_LOOPED)
			goto mixqPlayChannel_maxplaylen
		"    cmpl %12(%%edi), %%edx\n"    /* %12 = ch->loopend */
		"    jae mixqPlayChannel_maxplaylen\n"
		if (chan->pos>=chan->loopend)
			goto mixqPlayChannel_maxplaylen
		"    subl %14(%%edi), %%eax\n"    /* %14 = ch->length */
		eax-=edi->length;
		"    addl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */
		eax+=edi->loopend;
		"    movb $1, %4\n"               /*  %4 = inloop */
		inloop=1;


		"mixqPlayChannel_maxplaylen:\n"
		eax==mypos
		ebx==mystep
		 si==myfpos
		edx=chan->pos (but we do not care)
		ecx==mylen==len
		edi=chan
	
		"  xorl %%edx, %%edx\n"
		edx=0
		"  shld $16, %%eax, %%edx\n"
		edx=mypos>>16
		"  shll $16, %%esi\n"
		esi=myfpos<<16
		"  shld $16, %%esi, %%eax\n"
		eax=mypos<<16|myfpos
		"  addl %%ebx, %%eax\n"
		eax=mystep + (mypos<<16|myfpos)
		"  adcl $0, %%edx\n"
		edx+=carry from above (we are using 64bit edx:eax
		edx:eax=((uint64_t)(mypos<<16))|myfpos+((uint64_t)mystep)
		"  subl $1, %%eax\n"
		"  sbbl $0, %%edx\n"
		remove one from edx:eax
		edx:eax=((uint64_t)(mypos<<16))|myfpos+((uint64_t)mystep)-1;	
		"  cmpl %%ebx, %%edx\n"
		"  jae mixqPlayChannel_playecx\n"
		if (targetpos>>32 > myrate)
			goto mixqPlayChannel_playecx; /* division bellow will overflow => exception */
		"  divl %%ebx\n"
		eax=targetpos/mystep
		edx=targetpos%mystep
		"  cmpl %%eax, %%ecx\n"
		"  jb mixqPlayChannel_playecx\n"
		if (mylen<targetpos/mystep)
			goto mixqPlayChannel_playecx;
		"    movl %%eax, %%ecx\n"
		ecx=targetpos/mystep
		"    cmpb $0, %4\n"               /*  %4 = inloop */
		"    jnz mixqPlayChannel_playecx\n"
		if (inloop)
			goto mixqPlayChannel_playecx
		"      andb %20, %6(%%edi)\n"     /* %20 = MIXQ_PLAYING^255
		                                   *  %6 = ch->status */
		chan->status&=~MIXQ_PLAYING;
		"      movl %1, %%eax\n"          /*  %1 = len */
		eax=len
		"      subl %%ecx, %%eax\n"
		eax-=ecx
		"      addl %%eax, %5\n"          /*  %5 = filllen */
		fillen+=eax
		"      movl %%ecx, %1\n"          /*  %1 = len */
		len=ecx

		"mixqPlayChannel_playecx:\n"
mixqPlayChannel_playecx:
		/* we are here */
		"  pushl %%ebp\n"
		"  pushl %%edi\n"
		"  pushl %%ecx\n"

		"  movw %10(%%edi), %%bx\n"       /* %10 = ch->fpos */
		"  shll $16, %%ebx\n"
		ebx=chan->fpos<<16
		"  movl %0, %%eax\n"              /*  %0 = buf */
		eax=buf
		"  movl %8(%%edi), %%edx\n"       /*  %8 = ch->step */
		"  shll $16, %%edx\n"
		edx=chan->step<<16

		"  movl %9(%%edi), %%esi\n"       /*  %9 = ch->pos */
		esi=chan->pos
		"  movl %8(%%edi), %%ebp\n"       /*  %8 = ch->step */
		ebp=chan->step
		"  sarl $16, %%ebp\n"
		ebp=chan->step/65536; /* preserve sign */
		"  addl %15(%%edi), %%esi\n"      /* %15 = ch->samp */
		esi+=ch->samp;
		"  movl %%eax, %%edi\n"
		edi=eax

		"  call *%7\n"                    /*  %7 = playrout */
		"  popl %%ecx\n"
		"  popl %%edi\n"
		"  popl %%ebp\n"

		"  addl %%ecx, %0\n"              /*  %0 = buf */
		"  addl %%ecx, %0\n"              /*  %0 = buf */
		buf+=mylen (sizeof(int16_t)) thing
		"  subl %%ecx, %1\n"              /*  %1 = len */
		len-=mylen
	
		"  movl %8(%%edi), %%eax\n"       /*  %8 = ch->step */
		"  imul %%ecx\n"
		edx:eax=(ch->step*mylen)
		"  shld $16, %%eax, %%edx\n"
		edx=(ch->step*mylen) <<  16 
		"  addw %%ax, %10(%%edi)\n"       /* %10 = ch->fpos */
		"  adcl %%edx, %9(%%edi)\n"       /*  %9 = ch->pos */
		add the 64bit above to chan->fpos and chan->pos;
		"  movl %9(%%edi), %%eax\n"       /*  %9 = ch->pos */
		eax=pos==mypos;
		/* we are here */

		"  cmpb $0, %4\n"                 /*  %4 = inloop */
		"  jz mixqPlayChannel_fill\n"
		if (!inloop)
			goto mixqPlayChannel_fill;

		"  cmpl $0, %8(%%edi)\n"          /*  %8 = ch->step */
		"  jge mixqPlayChannel_forward2\n"
		if (chan->step>=0)
			mixqPlayChannel_forward2;
		"    cmpl %11(%%edi), %%eax\n"    /* %11 = ch->loopstart */
		"    jge mixqPlayChannel_exit\n"
		if (mypos>=ch->loopstart)
			return;
		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_pong\n"
		if (chan->status&MIXQ_PINGPONGLOOP)
			goto mixqPlayChannel_pong
		"      addl %13(%%edi), %%eax\n"  /* %13 = ch->replen */
		mypos+=chan->replen;
		"      jmp mixqPlayChannel_loopiflen\n"
		goto mixqPlayChannel_loopiflen
		"    mixqPlayChannel_pong:\n"

		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fpos */
		"      adcl $0, %%eax\n"
		chan->step=-chan->step
                 if ((chan->fpos=-chan->fpos))
                          mypos++;
		"      negl %%eax\n"
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		"      addl %11(%%edi), %%eax\n"  /* %11 = ch->loopstart */
		mypos=chan->loopstart+chan->loopstart-mypos;
		"      jmp mixqPlayChannel_loopiflen\n"
		goto mixqPlayChannel_loopiflen
		"mixqPlayChannel_forward2:\n"
mixqPlayChannel_forward2:

		"    cmpl %12(%%edi), %%eax\n"    /* %12 = ch->loopend */
		"    jb mixqPlayChannel_exit\n"
		if (mypos<ch->loopend)
			return;
		"    testb %21, %6(%%edi)\n"      /* %21 = MIXQ_PINGPONGLOOP
		                                   *  %6 = ch->status */
		"    jnz mixqPlayChannel_ping\n"
		if (chan->status&MIXQ_PINGPONGLOOP)
			goto mixqPlayChannel_ping;
		"      subl %13(%%edi), %%eax\n"  /* %13 = ch->replen */
		mypos-=chan->replen;
		"      jmp mixqPlayChannel_loopiflen\n"
		goto mixqPlayChannel_loopiflen;
		"    mixqPlayChannel_ping:\n"
mixqPlayChannel_ping:
		"      negl %8(%%edi)\n"          /*  %8 = ch->step */
		"      negw %10(%%edi)\n"         /* %10 = ch->fstep */
		"      adcl $0, %%eax\n"
		chan->step=-chan->step;
		if ((chan->fpos=-chan->fpos))
			mypos++;
		"      negl %%eax\n"
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */
		"      addl %12(%%edi), %%eax\n"  /* %12 = ch->loopend */
		mypos=chan->loopend+chan->loopend-mypos;

		"mixqPlayChannel_loopiflen:\n"
mixqPlayChannel_loopiflen:
		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		"  cmpl $0, %1\n"                 /*  %1 = len */
		"  jne mixqPlayChannel_bigloop\n"
		chan->pos=mypos;
		if (len)
			goto mixqPlayChannel_bigloop;

		"mixqPlayChannel_fill:\n"
mixqPlayChannel_fill:
		"  cmpl $0, %5\n"                 /*  %5 = filllen */
		"  je mixqPlayChannel_exit\n"
		if (!fillen)
			return;
		"  movl %14(%%edi), %%eax\n"      /* %14 = ch->length */
		tmplen==eax
		tmplen=chan->length;
		"  movl %%eax, %9(%%edi)\n"       /*  %9 = ch->pos */
		chan->pos=tmplen;
		"  addl %15(%%edi), %%eax\n"      /* %15 = ch->samp */
		tmplen+=ch->samp
		"  testb %17, %6(%%edi)\n"        /* %17 = MIXQ_PLAY16BIT
		                                   *  %6 = ch->status */
		"  jnz mixqPlayChannel_fill16\n"
		if (chan->status&MIXQ_PLAY16BIT)
			goto mixqPlayChannel_fill16;
		"    movb (%%eax), %%ah\n"
		"    movb $0, %%al\n"
		"    jmp mixqPlayChannel_filldo\n"
		"mixqPlayChannel_fill16:\n"
		"    movw (%%eax, %%eax), %%ax\n"
		"mixqPlayChannel_filldo:\n"
		"  movl %5, %%ecx\n"              /*  %5 = filllen */
		"  movl %0, %%edi\n"              /*  %0 = buf */
		"  rep stosw\n"
		"mixqPlayChannel_exit:\n"
		:
		: "m" (buf),                                 /*   0  */
		  "m" (len),                                 /*   1  */
		  "m" (ch),                                  /*   2  */
		  "m" (quiet),                               /*   3  */
		  "m" (inloop),                              /*   4  */
		  "m" (filllen),                             /*   5  */
		  "m" (((struct channel *)NULL)->status),    /*   6  */
		  "m" (playrout),                            /*   7  */
		  "m" (((struct channel *)NULL)->step),      /*   8  */
		  "m" (((struct channel *)NULL)->pos),       /*   9  */
	      	  "m" (((struct channel *)NULL)->fpos),      /*  10  */
		  "m" (((struct channel *)NULL)->loopstart), /*  11  */
		  "m" (((struct channel *)NULL)->loopend),   /*  12  */
		  "m" (((struct channel *)NULL)->replen),    /*  13  */
		  "m" (((struct channel *)NULL)->length),    /*  14  */
		  "m" (((struct channel *)NULL)->samp),      /*  15  */
		  "n" (MIXQ_INTERPOLATE),                    /*  16  */
		  "n" (MIXQ_PLAY16BIT),                      /*  17  */
		  "n" (MIXQ_INTERPOLATEMAX),                 /*  18  */
		  "n" (MIXQ_LOOPED),                         /*  19  */
		  "n" (255-MIXQ_PLAYING),                    /*  20  */
		  "n" (MIXQ_PINGPONGLOOP)                    /*  21  */
		:
		"memory", "eax", "ecx", "edx", "edi", "esi"
	);
	#endif
}

void mixqAmplifyChannel(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

void mixqAmplifyChannelUp(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		vol++;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

void mixqAmplifyChannelDown(int32_t *buf, int16_t *src, uint32_t len, int32_t vol, uint32_t step)
{
	while (len)
	{
		(*buf) += myvoltab[vol][0][(uint8_t)((*src)>>8)] + myvoltab[vol][1][(uint8_t)((*src)&0xff)];
		src++;
		vol--;
		len--;
		buf+=step/sizeof(uint32_t);
	}
}

#endif
