/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * These routines perform encryption and decryption using the DES
 * private key algorithm, or else a subset of it-- fewer inner loops.
 * (AUTH_DES_ITER defaults to 16, may be less.)
 *
 * Under U.S. law, this software may not be exported outside the US
 * without license from the U.S. Commerce department.
 *
 * The key schedule is passed as an arg, as well as the cleartext or
 * ciphertext.
 *
 * All registers labeled imply Vax using the Ultrix or 4.2bsd
 * compiler.
 *
 *
 *	NOTE:  bit and byte numbering:
 *			DES algorithm is defined in terms of bits of L
 *			followed by bits of R.

 *		bit 0  ==> lsb of L
 *		bit 63 ==> msb of R
 *
 * Always work in register pairs, FROM L1,R1 TO L2,R2 to make
 * bookkeeping easier.
 *
 * originally written by Steve Miller, MIT Project Athena
 */
#include <afsconfig.h>
#include <afs/param.h>

#include "mit-cpyright.h"


#ifndef KERNEL
#include <stdio.h>
#include <string.h>
#endif
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#endif /* AFS_PTHREAD_ENV */
#include "des.h"
#include "des_internal.h"
#include "s_table.h"
#ifdef BIG
#include "p_table.h"
#endif

#include "des_prototypes.h"

#ifdef AFS_HPUX_ENV
/* When compiling with '-g -O', the HP-UX compiler can get confused with
 * including ip.c and fp.c below. The result is that the compiler spins on
 * the CPU and never compiles the file. As a workaround, we can avoid this if
 * R1 and L1 are declared 'register' (I'm guessing this has to do with the
 * fact that they are used in ip.c/fp.c, and declaring them register avoids
 * creating debugging information, and it's that debugging information that
 * the compiler is flipping out over since they're in multiple .c files). So
 * declare them as 'register' on HP-UX; only do it on HP-UX to minimize the
 * scope of such a change, so we don't impact other platforms. */
# define HPUX_REGISTER register
#else
# define HPUX_REGISTER
#endif

#define XPRT_DES

#ifdef DEBUG
#define DBG_PRINT(s) if (des_debug & 2) \
    des_debug_print(s,i,L1&0xffff,(L1>>16)&0xffff, \
		R1&0xffff,(R1>>16)&0xffff)
#else
#define DBG_PRINT(s)
#endif

/* encrypt == 0  ==> decrypt, else encrypt */

afs_int32
des_ecb_encrypt(void * clear, void * cipher,
		des_key_schedule schedule, int encrypt)
{
    /* better pass 8 bytes, length not checked here */

    HPUX_REGISTER afs_uint32 R1 = 0;
    HPUX_REGISTER afs_uint32 L1 = 0;	/* R1 = r10, L1 = r9 */
    afs_uint32 R2 = 0, L2 = 0;	/* R2 = r8, L2 = r7 */
    afs_int32 i;
    /* one more registers left on VAX, see below P_temp_p */
#ifdef BITS16
    sbox_in_16_a S_in_16_a;
    sbox_in_16_b S_in_16_b;
    sbox_in_16_c S_in_16_c;
    unsigned int *S_in_a_16_p = (unsigned int *)&S_in_16_a;
    unsigned int *S_in_b_16_p = (unsigned int *)&S_in_16_b;
    unsigned int *S_in_c_16_p = (unsigned int *)&S_in_16_c;
#endif
#ifndef BITS32
#ifndef BITS16
#error dunno how to do this machine type, you lose;
#endif
#endif
    volatile afs_uint32 P_temp;
    volatile unsigned char *P_temp_p = (unsigned char *)&P_temp;
#ifdef BITS16
    sbox_out S_out;
    afs_uint32 *S_out_p = (afs_uint32 *) & S_out;
#endif
    afs_uint32 R_save, L_save;
#ifdef DEBUG
    afs_uint32 dbg_tmp[2];
#endif
    /*
     * Use L1,R1 and L2,R2 as two sets of "64-bit" registers always
     * work from L1,R1 input to L2,R2 output; initialize the cleartext
     * into registers.
     */
#ifdef MUSTALIGN
#ifdef DEBUG
    /*
     * If the alignment is wrong, the programmer really screwed up --
     * we aren't even getting the right data type.  His problem.  Keep
     * this code for debugging.
     */
    /* Make sure schedule is ok */
    if ((afs_int32) schedule & 3) {
	fprintf(stderr, "des.c schedule arg pointer not aligned\n");
	abort();
    }
#endif
    if (afs_pointer_to_int(clear) & 3) {
	memcpy((char *)(&L_save), (char *)clear, sizeof(L_save));
	clear=((afs_uint32*)clear)+1;
	memcpy((char *)(&R_save), (char *)clear, sizeof(R_save));
	L1 = L_save;
	R1 = R_save;
    } else
#endif
    {
	if (clear) {
	    L1 = *((afs_uint32 *)clear);
            clear=((afs_uint32*)clear)+1;
	} else
	    L1 = 0;
	if (clear)
	    R1 = *((afs_uint32 *)clear);
	else
	    R1 = 0;
    }

#ifdef DEBUG
    if (des_debug & 2) {
	printf("All values printed from low byte (bit 0)");
	printf(" --> high byte (bit 63)\n");
	i = 0;
	dbg_tmp[0] = L1;
	dbg_tmp[1] = R1;
	printf("iter = %2d  before IP\n\t\tL1 R1 = ", i);
	des_cblock_print_file(dbg_tmp, stdout);
    }

    DBG_PRINT("before IP");
#endif

/*   IP_start:*/

    /* all the Initial Permutation code is in the include file */
#include "ip.c"
    /* reset input to L1,R1 */
    L1 = L2;
    R1 = R2;

    /* iterate through the inner loop */
    for (i = 0; i <= (AUTH_DES_ITER - 1); i++) {

#ifdef DEBUG
	if (des_debug & 2) {
	    dbg_tmp[0] = L1;
	    dbg_tmp[1] = R1;
	    printf("iter = %2d	start loop\n\t\tL1 R1 = ", i);
	    des_cblock_print_file(dbg_tmp, stdout);
	    DBG_PRINT("start loop");
	}
#endif

	R_save = R1;
	L_save = L1;

/*   E_start:*/
	/* apply the E permutation from R1 to L2, R2 */
#ifndef VAXASM
#ifdef SLOW_E
#include "e.c"
#else /* Bill's fast E */
	L2 = (R1 << 1);
	if (R1 & (1 << 31))
	    L2 |= 1 << 0;
	L2 &= 077;
	L2 |= (R1 << 3) & 07700;
	L2 |= (R1 << 5) & 0770000;
	L2 |= (R1 << 7) & 077000000;
	L2 |= (R1 << 9) & 07700000000;
	L2 |= (R1 << 11) & 030000000000;

	/* now from right to right */

	R2 = ((R1 >> 17) & 0176000);
	if (R1 & (1 << 0))
	    R2 |= 1 << 15;

	R2 |= ((R1 >> 21) & 017);
	R2 |= ((R1 >> 19) & 01760);
#endif /* SLOW_E */
#else /* VAXASM */
	/* E operations */
	/* right to left */
	asm("	rotl	$1,r10,r7");
	L2 &= 077;
	L2 |= (R1 << 3) & 07700;
	L2 |= (R1 << 5) & 0770000;
	L2 |= (R1 << 7) & 077000000;
	L2 |= (R1 << 9) & 07700000000;
	L2 |= (R1 << 11) & 030000000000;

	asm("	rotl	$-17,r10,r8");
	R2 &= 0176000;
	asm("	rotl	$-21,r10,r0");
	asm("	bicl2	$-16,r0");
	asm("  bisl2	r0,r8");
	asm("	rotl	$-19,r10,r0");
	asm("	bicl2	$-1009,r0");
	asm("  bisl2	r0,r8");

#endif

	/* reset input to L1,R1 */
	L1 = L2;
	R1 = R2;

#ifdef DEBUG
	if (des_debug & 2) {
	    dbg_tmp[0] = L1;
	    dbg_tmp[1] = R1;
	    DBG_PRINT("after e");
	    printf("iter = %2d	after e\n\t\tL1 R1 = ", i);
	    des_cblock_print_file(dbg_tmp, stdout);
	}
#endif

/*   XOR_start:*/
	/*
	 * XOR with the key schedule, "schedule"
	 *
	 * If this is an encryption operation, use schedule[i],
	 * otherwise use schedule [AUTH_DES_ITER-i-1]
	 *
	 * First XOR left half.
	 */
	if (encrypt) {
	    L1 ^= *(((afs_uint32 *) & schedule[i]) + 0);
	    /* now right half */
	    R1 ^= *(((afs_uint32 *) & schedule[i]) + 1);
	} else {
	    L1 ^= *(((afs_uint32 *) & schedule[AUTH_DES_ITER - i - 1]) + 0);
	    /* now right half */
	    R1 ^= *(((afs_uint32 *) & schedule[AUTH_DES_ITER - i - 1]) + 1);
	}

	/* dont have to reset input to L1, R1 */

#ifdef DEBUG
	if (des_debug & 2) {
	    dbg_tmp[0] = L1;
	    dbg_tmp[1] = R1;
	    DBG_PRINT("after xor");
	    printf("iter = %2d	after xor\n\t\tL1 R1 =", i);
	    des_cblock_print_file(dbg_tmp, stdout);
	}
#endif

/*   S_start:*/
	/* apply the S selection from L1, R1 to R2 */

#ifdef notdef
#include "s.c"
#endif

	/* S operations , cant use registers for bit field stuff */
	/* from S_in to S_out */

#ifdef BITS16
	*S_in_a_16_p = L1 & 0xffff;
	*S_in_b_16_p = (L1 >> 16) & 0xffff;
	*S_in_c_16_p = R1 & 0xffff;
	(*(afs_uint32 *) & S_out) = (unsigned)S_adj[0][S_in_16_a.b0];
	S_out.b1 = (unsigned)S_adj[1][S_in_16_a.b1];
	/* b2 spans two words */
	S_out.b2 = (unsigned)
	    S_adj[2][(unsigned)S_in_16_a.b2 +
		     (((unsigned)S_in_16_b.b2) << 4)];
	S_out.b3 = (unsigned)S_adj[3][S_in_16_b.b3];
	S_out.b4 = (unsigned)S_adj[4][S_in_16_b.b4];
	/* b5 spans both parts */
	S_out.b5 = (unsigned)
	    S_adj[5][(unsigned)S_in_16_b.b5 +
		     (((unsigned)S_in_16_c.b5) << 2)];
	S_out.b6 = (unsigned)S_adj[6][S_in_16_c.b6];
	S_out.b7 = (unsigned)S_adj[7][S_in_16_c.b7];
	R1 = *S_out_p;
#else
	/* is a 32 bit sys */
#ifndef VAXASM
	R2 = (unsigned)S_adj[0][L1 & 077];
	L2 = (unsigned)S_adj[1][(L1 >> 6) & 077];
	R2 |= (L2 << 4);
	L2 = (unsigned)S_adj[2][(L1 >> 12) & 077];
	R2 |= (L2 << 8);
	L2 = (unsigned)S_adj[3][(L1 >> 18) & 077];
	R2 |= (L2 << 12);
	L2 = (unsigned)S_adj[4][(L1 >> 24) & 077];
	R2 |= (L2 << 16);
	/* b5 spans both parts */
	L2 = (unsigned)
	    S_adj[5][(unsigned)((L1 >> 30) & 03) + ((R1 & 017) << 2)];
	R2 |= (L2 << 20);
	L2 = (unsigned)S_adj[6][(R1 >> 4) & 077];
	R2 |= (L2 << 24);
	L2 = (unsigned)S_adj[7][(R1 >> 10) & 077];
	R1 = R2 | (L2 << 28);
	/* reset input to L1, R1 */
#else /* vaxasm */
	/*
	 * this is the c code produced above, with
	 * extzv replaced by rotl
	 */
	asm("bicl3	$-64,r9,r0");
	asm("movzbl	_S_adj[r0],r8");
	asm("rotl	$-6,r9,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+64[r0],r7");
	asm("ashl	$4,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-12,r9,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+128[r0],r7");
	asm("ashl	$8,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-18,r9,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+192[r0],r7");
	asm("ashl	$12,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-24,r9,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+256[r0],r7");
	asm("ashl	$16,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-30,r9,r0");
	asm("bicl2	$-4,r0");
	asm("bicl3	$-16,r10,r1");
	asm("ashl	$2,r1,r1");
	asm("addl2	r1,r0");
	asm("movzbl	_S_adj+320[r0],r7");
	asm("ashl	$20,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-4,r10,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+384[r0],r7");
	asm("ashl	$24,r7,r0");
	asm("bisl2	r0,r8");
	asm("rotl	$-10,r10,r0");
	asm("bicl2	$-64,r0");
	asm("movzbl	_S_adj+448[r0],r7");
	asm("ashl	$28,r7,r0");
	asm("bisl2	r8,r0");
	asm("movl	r0,r10");

#endif /* vaxasm */
#endif

#ifdef DEBUG
	if (des_debug & 2) {
	    dbg_tmp[0] = L1;
	    dbg_tmp[1] = R1;
	    DBG_PRINT("after s");
	    printf("iter = %2d	after s\n\t\tL1 R1 = ", i);
	    des_cblock_print_file(dbg_tmp, stdout);
	}
#endif

/*   P_start:*/
	/* and then the p permutation from R1 into R2 */
#include "p.c"
	/* reset the input to L1, R1 */
	R1 = R2;

#ifdef DEBUG
	if (des_debug & 2) {
	    dbg_tmp[0] = L1;
	    dbg_tmp[1] = R1;
	    DBG_PRINT("after p");
	    printf("iter = %2d	after p\n\t\tL1 R1 = ", i);
	    des_cblock_print_file(dbg_tmp, stdout);
	}
#endif

	/* R1 is the output value from the f() */
	/* move R[iter] to L[iter+1] */
/*   XOR_2_start:*/
	L1 = R_save;
	/* xor with left */
	R1 = L_save ^ R1;
	/* reset the input */
    }

    /* flip left and right before final permutation */
    L2 = R1;			/* flip */
    R2 = L1;
    /* reset the input */
    L1 = L2;
    R1 = R2;

#ifdef DEBUG
    if (des_debug & 2) {
	dbg_tmp[0] = L1;
	dbg_tmp[1] = R1;
	DBG_PRINT("before FP");
	printf("iter = %2d  before FP\n\t\tL1 R1 = ", i);
	des_cblock_print_file(dbg_tmp, stdout);
    }
#endif

/*FP_start:*/
    /* do the final permutation from L1R1 to L2R2 */
    /* all the fp code is in the include file */
#include "fp.c"

    /* copy the output to the ciphertext string;
     * can be same as cleartext
     */

#ifdef MUSTALIGN
    if (afs_pointer_to_int(cipher) & 3) {
	L_save = L2;		/* cant bcopy a reg */
	R_save = R2;
	memcpy((char *)cipher, (char *)&L_save, sizeof(L_save));
	cipher=((afs_uint32*)cipher)+1;
	memcpy((char *)cipher, (char *)&R_save, sizeof(R_save));
    } else
#endif
    {
        *((afs_uint32*)cipher)= L2;
	cipher = ((afs_int32 *)cipher)+1;
	*((afs_uint32 *)cipher) = R2;
    }

#ifdef DEBUG
    if (des_debug & 2) {
	L1 = L2;
	R1 = R2;
	dbg_tmp[0] = L1;
	dbg_tmp[1] = R1;
	DBG_PRINT("done");
	printf("iter = %2d  done\n\t\tL1 R1 = ", i);
	des_cblock_print_file(dbg_tmp, stdout);
    }
#endif

    /* that's it, no errors can be returned */
    return 0;
}
