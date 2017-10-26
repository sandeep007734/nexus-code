/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * This routine computes the DES key schedule given a key.  The
 * permutations and shifts have been done at compile time, resulting
 * in a direct one-step mapping from the input key to the key
 * schedule.
 *
 * Also checks parity and weak keys.
 *
 * Watch out for the subscripts -- most effectively start at 1 instead
 * of at zero.  Maybe some bugs in that area.
 *
 * DON'T change the data types for arrays and such, or it will either
 * break or run slower.  This was optimized for Uvax2.
 *
 * In case the user wants to cache the computed key schedule, it is
 * passed as an arg.  Also implies that caller has explicit control
 * over zeroing both the key schedule and the key.
 *
 * All registers labeled imply Vax using the Ultrix or 4.2bsd compiler.
 *
 * Originally written 6/85 by Steve Miller, MIT Project Athena.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "mit-cpyright.h"
#include "des_internal.h"
#include <stdio.h>

#include "des.h"
#include "key_perm.h"
#include "des_prototypes.h"

typedef char key[64];

/* the following are really void but cc86 doesnt allow it */
static int make_key_sched(key Key, des_key_schedule Schedule);

#ifdef AFS_DUX40_ENV
#pragma weak des_key_sched = afs_des_key_sched
int
afs_des_key_sched(des_cblock k, des_key_schedule schedule)
#else
int
des_key_sched(des_cblock k, des_key_schedule schedule)
#endif
{
    /* better pass 8 bytes, length not checked here */

    int i, j;
    unsigned int temp;	/*  r7 */
    char *p_char;	/* r6 */
    key k_char;
    i = 8;
    p_char = k_char;

#ifdef DEBUG
    if (des_debug)
	fprintf(stderr, "\n\ninput key, left to right = ");
#endif

    if (!des_check_key_parity(k))	/* bad parity --> return -1 */
	return (-1);

    do {
	/* get next input key byte */
#ifdef DEBUG
	if (des_debug)
	    fprintf(stderr, "%02x ", *k & 0xff);
#endif
	temp = (unsigned int)((unsigned char)*k++);
	j = 8;

	do {
#ifndef VAXASM
	    *p_char++ = (int)temp & 01;
	    temp = temp >> 1;
#else
	    asm("bicb3	$-2,r7,(r8)+[r6]");
	    asm("rotl	$-1,r7,r7");
#endif
	} while (--j > 0);
    } while (--i > 0);

#ifdef DEBUG
    if (des_debug) {
	p_char = k_char;
	fprintf(stderr, "\nKey bits, from zero to 63");
	for (i = 0; i <= 7; i++) {
	    fprintf(stderr, "\n\t");
	    for (j = 0; j <= 7; j++)
		fprintf(stderr, "%d ", *p_char++);
	}
    }
#else
#ifdef lint
    p_char = p_char;
#endif
#endif

    /* check against weak keys */
    k -= sizeof(des_cblock);

    if (des_is_weak_key(k))
	return (-2);

    make_key_sched(k_char, schedule);

    /* if key was good, return 0 */
    return 0;
}

static int
make_key_sched(key Key, des_key_schedule Schedule)
{
    /*
     * The key has been converted to an array to make this run faster;
     * on a microvax 2, this routine takes about 3.5ms.  The code and
     * size of the arrays has been played with to get it as fast as
     * possible.
     *
     * Don't change the order of the declarations below without
     * checking the assembler code to make sure that things are still
     * where it expects them.
     */

    /* r10, unroll by AUTH_DES_ITER */
    int iter = AUTH_DES_ITER;
    afs_uint32 *k;	/* r9 */
    int *kp;		/* r8 */
    afs_uint32 temp;	/* r7 */

    kp = (int *)key_perm;
    k = (afs_uint32 *) Schedule;

    do {
	/*
	 * create the Key schedule
	 *
	 * put into lsb first order (lsb is bit 0)
	 */

	/*
	 * On the uvax2, this C code below is as fast as straight
	 * assembler, so just use C code below.
	 */
	temp = 0;
#ifdef LSBFIRST
#define BIT(x)	x
#else
#ifdef notdef
#define BIT(x) rev_swap_bit_pos_0(x)
#else
#define BIT(x)	x
#endif
#endif
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(0));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(1));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(2));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(3));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(4));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(5));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(6));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(7));

	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(8));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(9));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(10));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(11));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(12));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(13));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(14));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(15));

	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(16));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(17));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(18));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(19));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(20));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(21));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(22));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(23));

	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(24));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(25));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(26));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(27));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(28));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(29));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(30));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(31));

	*k++ = temp;
	temp = 0;

	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(0));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(1));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(2));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(3));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(4));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(5));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(6));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(7));

	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(8));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(9));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(10));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(11));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(12));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(13));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(14));
	if ((unsigned)Key[(int)*kp++])
	    temp |= (1 << BIT(15));

	*k++ = temp;

    } while (--iter > 0);

#ifdef DEBUG
    if (des_debug) {
	int i;
	char *n;
	int q;
	fprintf(stderr, "\nKey Schedule, left to right");
	for (i = 0; i < AUTH_DES_ITER; i++) {
	    n = (char *)&Schedule[i];
	    fprintf(stderr, "\n");
	    for (q = 0; q <= 7; q++)
		fprintf(stderr, "%02x ", *n++ & 0xff);
	}
	fprintf(stderr, "\n");
    }
#endif

    return (0);
}
