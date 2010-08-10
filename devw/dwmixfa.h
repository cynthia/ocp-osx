#define MIXF_MIXBUFLEN 4096
#define MIXF_MAXCHAN 255

//#define MIXF_PLAYSTEREO 0     Floating point mixer doesn't support stereo samples, only mono to stereo output
#define MIXF_INTERPOLATE 2
#define MIXF_INTERPOLATEQ 4
#define MIXF_FILTER 8
#define MIXF_QUIET 16
#define MIXF_LOOPED 32

#define MIXF_PLAYING 256
#define MIXF_MUTE 512

#define MIXF_UNSIGNED 1
#define MIXF_16BITOUT 2

#define MIXF_VOLRAMP  256
#define MIXF_DECLICK  512

extern void mixer (void);
/*#pragma aux mixer "*" parm[] modify [eax ebx ecx edx esi edi ebp];*/

extern void prepare_mixer (void);
/*#pragma aux prepare_mixer "*" parm[] modify [];*/

extern void getchanvol (int n, int len);
/*#pragma aux getchanvol "*" parm [eax][ecx] modify [];*/

extern float * tempbuf;         /* ptr to 32 bit temp-buffer */
extern void  * outbuf;          /* ptr to 16 bit mono-buffer */
extern uint32_t nsamples;        /* # of samples to generate */
extern uint32_t nvoices;         /* # of voices */

extern uint32_t freqf[];
extern uint32_t freqw[];

extern float     *smpposw[];
extern uint32_t smpposf[];

extern float    *loopend[];
extern uint32_t looplen[];
extern float   volleft[];
extern float   volright[];
extern float   rampleft[];
extern float   rampright[];

extern float   ffreq[];
extern float   freso[];

extern float   fl1[];
extern float   fb1[];

extern uint32_t voiceflags[];

extern float   fadeleft,faderight;

extern int     isstereo;
extern int     outfmt;
extern float   voll, volr;

extern float   ct0[],ct1[],ct2[],ct3[];

extern struct mixfpostprocregstruct *postprocs;

extern uint32_t samprate;

#ifdef I386_ASM

extern void start_dwmixfa(void);   /* these two are used to calculate memory remapping (self modifying code) */
extern void stop_dwmixfa(void);

#endif
