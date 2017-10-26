/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * Quadratic Congruential Manipulation Dectection Code
 *
 * ref: "Message Authentication"
 *		R.R. Jueneman, S. M. Matyas, C.H. Meyer
 *		IEEE Communications Magazine,
 *		Sept 1985 Vol 23 No 9 p 29-40
 *
 * This routine, part of the Athena DES library built for the Kerberos
 * authentication system, calculates a manipulation detection code for
 * a message.  It is a much faster alternative to the DES-checksum
 * method. No guarantees are offered for its security.	Refer to the
 * paper noted above for more information
 *
 * Implementation for 4.2bsd
 * by S.P. Miller	Project Athena/MIT
 */

/*
 * Algorithm (per paper):
 *		define:
 *		message to be composed of n m-bit blocks X1,...,Xn
 *		optional secret seed S in block X1
 *		MDC in block Xn+1
 *		prime modulus N
 *		accumulator Z
 *		initial (secret) value of accumulator C
 *		N, C, and S are known at both ends
 *		C and , optionally, S, are hidden from the end users
 *		then
 *			(read array references as subscripts over time)
 *			Z[0] = c;
 *			for i = 1...n
 *				Z[i] = (Z[i+1] + X[i])**2 modulo N
 *			X[n+1] = Z[n] = MDC
 *
 *		Then pick
 *			N = 2**31 -1
 *			m = 16
 *			iterate 4 times over plaintext, also use Zn
 *			from iteration j as seed for iteration j+1,
 *			total MDC is then a 128 bit array of the four
 *			Zn;
 *
 *			return the last Zn and optionally, all
 *			four as output args.
 *
 * Modifications:
 *	To inhibit brute force searches of the seed space, this
 *	implementation is modified to have
 *	Z	= 64 bit accumulator
 *	C	= 64 bit C seed
 *	N	= 2**63 - 1
 *  S	= S seed is not implemented here
 *	arithmetic is not quite real double integer precision, since we
 *	cant get at the carry or high order results from multiply,
 *	but nontheless is 64 bit arithmetic.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>


#include "mit-cpyright.h"

/* System include files */
#ifndef KERNEL
#include <stdio.h>
#endif
#include <errno.h>

/* Application include files */
#include "des.h"
#include "des_internal.h"
#include "des_prototypes.h"

/* Definitions for byte swapping */

#ifdef LSBFIRST
#define vaxtohl(x) (*((afs_uint32 *)(x)))
#define vaxtohs(x) (*((unsigned short *)(x)))
#else
#define vaxtohl(x) four_bytes_vax_to_nets((char *)(x))
#define vaxtohs(x) two_bytes_vax_to_nets((char *)(x))
#endif

/*** Routines ***************************************************** */

#ifdef MSBFIRST

static unsigned short
two_bytes_vax_to_nets(char *p)
{
    union {
	char pieces[2];
	unsigned short result;
    } short_conv;

    short_conv.pieces[0] = p[1];
    short_conv.pieces[1] = p[0];
    return (short_conv.result);
}

static afs_uint32
four_bytes_vax_to_nets(char *p)
{
    union {
	char pieces[4];
	afs_uint32 result;
    } long_conv;

    long_conv.pieces[0] = p[3];
    long_conv.pieces[1] = p[2];
    long_conv.pieces[2] = p[1];
    long_conv.pieces[3] = p[0];
    return (long_conv.result);
}

#endif

/*
    des_cblock *c_seed;		* secret seed, 8 bytes *
    unsigned char *in;		* input block *
    afs_uint32 *out;		* optional longer output *
    int out_count;		* number of iterations *
    afs_int32 length;		* original length in bytes *
*/

afs_uint32
des_quad_cksum(unsigned char *in, afs_uint32 * out, afs_int32 length,
	       int out_count, des_cblock * c_seed)
{

    /*
     * this routine both returns the low order of the final (last in
     * time) 32bits of the checksum, and if "out" is not a null
     * pointer, a longer version, up to entire 32 bytes of the
     * checksum is written unto the address pointed to.
     */

    afs_uint32 z;
    afs_uint32 z2;
    afs_uint32 x;
    afs_uint32 x2;
    unsigned char *p;
    afs_int32 len;
    int i;

    /* use all 8 bytes of seed */

    z = vaxtohl(c_seed);
    z2 = vaxtohl((char *)c_seed + 4);
    if (out == NULL)
	out_count = 1;		/* default */

    /* This is repeated n times!! */
    for (i = 1; i <= 4 && i <= out_count; i++) {
	len = length;
	p = in;
	while (len) {
	    if (len > 1) {
		x = (z + vaxtohs(p));
		p += 2;
		len -= 2;
	    } else {
		x = (z + *(char *)p++);
		len = 0;
	    }
	    x2 = z2;
	    z = ((x * x) + (x2 * x2)) % 0x7fffffff;
	    z2 = (x * (x2 + 83653421)) % 0x7fffffff;	/* modulo */
	    if (des_debug & 8)
		printf("%ld %ld\n", afs_printable_int32_ld(z),
		       afs_printable_int32_ld(z2));
	}

	if (out != NULL) {
	    *out++ = z;
	    *out++ = z2;
	}
    }
    /* return final z value as 32 bit version of checksum */
    return z;
}
