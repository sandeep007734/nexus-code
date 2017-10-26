/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#ifdef KERNEL
#if !defined(UKERNEL)
#include "h/types.h"
#if !defined(AFS_LINUX26_ENV)
#include "h/param.h"
#endif
#ifdef	AFS_AUX_ENV
#include "h/mmu.h"
#include "h/seg.h"
#include "h/sysmacros.h"
#include "h/signal.h"
#include "h/errno.h"
#endif
#include "h/time.h"
#if defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_LINUX20_ENV)
#include "h/errno.h"
#else
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV)
#include "h/kernel.h"
#endif
#endif
#if	defined(AFS_SUN56_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_FBSD_ENV) || defined(AFS_DARWIN80_ENV)
#include "afs/sysincludes.h"
#endif
#if !defined(AFS_SGI64_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_OBSD48_ENV)
#include "h/user.h"
#endif /* AFS_SGI64_ENV */
#include "h/uio.h"
#ifdef	AFS_OSF_ENV
#include <sys/mount.h>
#include <sys/vnode.h>
#include <ufs/inode.h>
#endif
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_HPUX110_ENV)
#include "h/mbuf.h"
#endif
#ifndef AFS_LINUX20_ENV
#include "netinet/in.h"
#endif

/* afs_buffer.c */
/* These are needed because afs_prototypes.h is not included here */
struct dcache;
extern void *DRead(struct dcache *adc, int page);
extern void *DNew(struct dcache *adc, int page);

#else /* !defined(UKERNEL) */
#include "afs/stds.h"
#include "afs/sysincludes.h"

/* afs_buffer.c */
/* These are needed because afs_prototypes.h is not included here */
extern void *DRead(afs_int32 *fid, int page);
extern void *DNew(afs_int32 *fid, int page);

#endif /* !defined(UKERNEL) */
#include "afs/afs_osi.h"

#include "afs/dir.h"

#ifdef AFS_LINUX20_ENV
#include "h/string.h"
#endif

/* generic renaming */
#define	NameBlobs	afs_dir_NameBlobs
#define	GetBlob		afs_dir_GetBlob
#define GetVerifiedBlob afs_dir_GetVerifiedBlob
#define	Create		afs_dir_Create
#define	Length		afs_dir_Length
#define	Delete		afs_dir_Delete
#define	MakeDir		afs_dir_MakeDir
#define	Lookup		afs_dir_Lookup
#define	LookupOffset	afs_dir_LookupOffset
#define	EnumerateDir	afs_dir_EnumerateDir
#define	IsEmpty		afs_dir_IsEmpty
#define InverseLookup   afs_dir_InverseLookup
#define ChangeFid	afs_dir_ChangeFid

#else /* KERNEL */

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# include <sys/types.h>
# include <errno.h>
# include "dir.h"
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <string.h>
#endif /* KERNEL */

afs_int32 DErrno;

/* Local static prototypes */
static struct DirEntry *FindItem(void *dir, char *ename,
				 unsigned short **previtem);


/* Find out how many entries are required to store a name. */
int
NameBlobs(char *name)
{
    int i;
    i = strlen(name) + 1;
    return 1 + ((i + 15) >> 5);
}

/* Create an entry in a file.  Dir is a file representation, while entry is
 * a string name. */
int
Create(void *dir, char *entry, void *voidfid)
{
    afs_int32 *vfid = (afs_int32 *) voidfid;
    int blobs, firstelt;
    int i;
    struct DirEntry *ep;
    unsigned short *pp = NULL;
    struct DirHeader *dhp;

    /* check name quality */
    if (*entry == 0)
	return EINVAL;
    /* First check if file already exists. */
    ep = FindItem(dir, entry, &pp);
    if (ep) {
	DRelease(ep, 0);
	DRelease(pp, 0);
	return EEXIST;
    }
    blobs = NameBlobs(entry);	/* number of entries required */
    firstelt = FindBlobs(dir, blobs);
    if (firstelt < 0)
	return EFBIG;		/* directory is full */
    /* First, we fill in the directory entry. */
    ep = GetBlob(dir, firstelt);
    if (ep == 0)
	return EIO;
    ep->flag = FFIRST;
    ep->fid.vnode = htonl(vfid[1]);
    ep->fid.vunique = htonl(vfid[2]);
    strcpy(ep->name, entry);
    /* Now we just have to thread it on the hash table list. */
    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp) {
	DRelease(ep, 1);
	return EIO;
    }
    i = DirHash(entry);
    ep->next = dhp->hashTable[i];
    dhp->hashTable[i] = htons(firstelt);
    DRelease(dhp, 1);
    DRelease(ep, 1);
    return 0;
}

int
Length(void *dir)
{
    int i, ctr;
    struct DirHeader *dhp;
    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp)
	return 0;
    if (dhp->header.pgcount != 0)
	ctr = ntohs(dhp->header.pgcount);
    else {
	/* old style, count the pages */
	ctr = 0;
	for (i = 0; i < MAXPAGES; i++)
	    if (dhp->alloMap[i] != EPP)
		ctr++;
    }
    DRelease(dhp, 0);
    return ctr * AFS_PAGESIZE;
}

int
Delete(void *dir, char *entry)
{
    /* Delete an entry from a directory, including update of all free entry descriptors. */
    int nitems, index;
    struct DirEntry *firstitem;
    unsigned short *previtem;
    firstitem = FindItem(dir, entry, &previtem);
    if (firstitem == 0)
	return ENOENT;
    *previtem = firstitem->next;
    DRelease(previtem, 1);
    index = DVOffset(firstitem) / 32;
    nitems = NameBlobs(firstitem->name);
    DRelease(firstitem, 0);
    FreeBlobs(dir, index, nitems);
    return 0;
}

int
FindBlobs(void *dir, int nblobs)
{
    /* Find a bunch of contiguous entries; at least nblobs in a row. */
    int i, j, k;
    int failed = 0;
    struct DirHeader *dhp;
    struct PageHeader *pp;
    int pgcount;

    dhp = (struct DirHeader *)DRead(dir, 0);	/* read the dir header in first. */
    if (!dhp)
	return -1;
    for (i = 0; i < BIGMAXPAGES; i++) {
	if (i >= MAXPAGES || dhp->alloMap[i] >= nblobs) {
	    /* if page could contain enough entries */
	    /* If there are EPP free entries, then the page is not even allocated. */
	    if (i >= MAXPAGES) {
		/* this pages exists past the end of the old-style dir */
		pgcount = ntohs(dhp->header.pgcount);
		if (pgcount == 0) {
		    pgcount = MAXPAGES;
		    dhp->header.pgcount = htons(pgcount);
		}
		if (i > pgcount - 1) {
		    /* this page is bigger than last allocated page */
		    AddPage(dir, i);
		    dhp->header.pgcount = htons(i + 1);
		}
	    } else if (dhp->alloMap[i] == EPP) {
		/* Add the page to the directory. */
		AddPage(dir, i);
		dhp->alloMap[i] = EPP - 1;
		dhp->header.pgcount = htons(i + 1);
	    }
	    pp = (struct PageHeader *)DRead(dir, i);	/* read the page in. */
	    if (!pp) {
		DRelease(dhp, 1);
		break;
	    }
	    for (j = 0; j <= EPP - nblobs; j++) {
		failed = 0;
		for (k = 0; k < nblobs; k++)
		    if ((pp->freebitmap[(j + k) >> 3] >> ((j + k) & 7)) & 1) {
			failed = 1;
			break;
		    }
		if (!failed)
		    break;
		failed = 1;
	    }
	    if (!failed) {
		/* Here we have the first index in j.  We update the allocation maps
		 * and free up any resources we've got allocated. */
		if (i < MAXPAGES)
		    dhp->alloMap[i] -= nblobs;
		DRelease(dhp, 1);
		for (k = 0; k < nblobs; k++)
		    pp->freebitmap[(j + k) >> 3] |= 1 << ((j + k) & 7);
		DRelease(pp, 1);
		return j + i * EPP;
	    }
	    DRelease(pp, 0);	/* This dir page is unchanged. */
	}
    }
    /* If we make it here, the directory is full. */
    DRelease(dhp, 1);
    return -1;
}

void
AddPage(void *dir, int pageno)
{				/* Add a page to a directory. */
    int i;
    struct PageHeader *pp;

    pp = (struct PageHeader *)DNew(dir, pageno);	/* Get a new buffer labelled dir,pageno */
    pp->tag = htons(1234);
    if (pageno > 0)
	pp->pgcount = 0;
    pp->freecount = EPP - 1;	/* The first dude is already allocated */
    pp->freebitmap[0] = 0x01;
    for (i = 1; i < EPP / 8; i++)	/* It's a constant */
	pp->freebitmap[i] = 0;
    DRelease(pp, 1);
}

/* Free a whole bunch of directory entries. */

void
FreeBlobs(void *dir, int firstblob, int nblobs)
{
    int i;
    int page;
    struct DirHeader *dhp;
    struct PageHeader *pp;
    page = firstblob / EPP;
    firstblob -= EPP * page;	/* convert to page-relative entry */
    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp)
	return;
    if (page < MAXPAGES)
	dhp->alloMap[page] += nblobs;
    DRelease(dhp, 1);
    pp = (struct PageHeader *)DRead(dir, page);
    if (pp)
	for (i = 0; i < nblobs; i++)
	    pp->freebitmap[(firstblob + i) >> 3] &=
		~(1 << ((firstblob + i) & 7));
    DRelease(pp, 1);
}

/*
 * Format an empty directory properly.  Note that the first 13 entries in a
 * directory header page are allocated, 1 to the page header, 4 to the
 * allocation map and 8 to the hash table.
 */
int
MakeDir(void *dir, afs_int32 * me, afs_int32 * parent)
{
    int i;
    struct DirHeader *dhp;
    dhp = (struct DirHeader *)DNew(dir, 0);
    dhp->header.pgcount = htons(1);
    dhp->header.tag = htons(1234);
    dhp->header.freecount = (EPP - DHE - 1);
    dhp->header.freebitmap[0] = 0xff;
    dhp->header.freebitmap[1] = 0x1f;
    for (i = 2; i < EPP / 8; i++)
	dhp->header.freebitmap[i] = 0;
    dhp->alloMap[0] = (EPP - DHE - 1);
    for (i = 1; i < MAXPAGES; i++)
	dhp->alloMap[i] = EPP;
    for (i = 0; i < NHASHENT; i++)
	dhp->hashTable[i] = 0;
    DRelease(dhp, 1);
    Create(dir, ".", me);
    Create(dir, "..", parent);	/* Virtue is its own .. */
    return 0;
}

/* Look up a file name in directory. */

int
Lookup(void *dir, char *entry, void *voidfid)
{
    afs_int32 *fid = (afs_int32 *) voidfid;
    struct DirEntry *firstitem;
    unsigned short *previtem;

    firstitem = FindItem(dir, entry, &previtem);
    if (firstitem == 0)
	return ENOENT;
    DRelease(previtem, 0);
    fid[1] = ntohl(firstitem->fid.vnode);
    fid[2] = ntohl(firstitem->fid.vunique);
    DRelease(firstitem, 0);
    return 0;
}

/* Look up a file name in directory. */

int
LookupOffset(void *dir, char *entry, void *voidfid, long *offsetp)
{
    afs_int32 *fid = (afs_int32 *) voidfid;
    struct DirEntry *firstitem;
    unsigned short *previtem;

    firstitem = FindItem(dir, entry, &previtem);
    if (firstitem == 0)
	return ENOENT;
    DRelease(previtem, 0);
    fid[1] = ntohl(firstitem->fid.vnode);
    fid[2] = ntohl(firstitem->fid.vunique);
    if (offsetp)
	*offsetp = DVOffset(firstitem);
    DRelease(firstitem, 0);
    return 0;
}

int
EnumerateDir(void *dir, int (*hookproc) (void *dir, char *name,
				         afs_int32 vnode, afs_int32 unique),
	     void *hook)
{
    /* Enumerate the contents of a directory.
     * Break when hook function returns non 0.
     */
    int i;
    int num;
    struct DirHeader *dhp;
    struct DirEntry *ep;
    int code = 0;
    int elements;

    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp)
	return EIO;		/* first page should be there */

    for (i = 0; i < NHASHENT; i++) {
	/* For each hash chain, enumerate everyone on the list. */
	num = ntohs(dhp->hashTable[i]);
	elements = 0;
	while (num != 0 && elements < BIGMAXPAGES * EPP) {
	    elements++;

	    /* Walk down the hash table list. */
	    code = GetVerifiedBlob(dir, num, &ep);
	    if (code)
		goto out;

	    if (!ep)
		break;

	    num = ntohs(ep->next);
	    code = (*hookproc) (hook, ep->name, ntohl(ep->fid.vnode),
			 ntohl(ep->fid.vunique));
	    DRelease(ep, 0);
	    if (code)
		goto out;
	}
    }

out:
    DRelease(dhp, 0);
    return 0;
}

int
IsEmpty(void *dir)
{
    /* Enumerate the contents of a directory. */
    int i;
    int num;
    struct DirHeader *dhp;
    struct DirEntry *ep;
    int code;
    int elements;

    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp)
	return 0;
    for (i = 0; i < NHASHENT; i++) {
	/* For each hash chain, enumerate everyone on the list. */
	num = ntohs(dhp->hashTable[i]);
	elements = 0;
	while (num != 0 && elements < BIGMAXPAGES * EPP) {
	    elements++;
	    /* Walk down the hash table list. */
	    code = GetVerifiedBlob(dir, num, &ep);
	    if (code)
		goto out;

	    if (!ep)
		break;

	    if (strcmp(ep->name, "..") && strcmp(ep->name, ".")) {
		DRelease(ep, 0);
		DRelease(dhp, 0);
		return 1;
	    }
	    num = ntohs(ep->next);
	    DRelease(ep, 0);
	}
    }

out:
    DRelease(dhp, 0);
    return 0;
}

/* Return a pointer to an entry, given its number. Also return the maximum
 * size of the entry, which is determined by its position within the directory
 * page.
 */

static struct DirEntry *
GetBlobWithLimit(void *dir, afs_int32 blobno, afs_size_t *maxlen)
{
    char *page;
    afs_size_t pos;

    *maxlen = 0;

    page = DRead(dir, blobno >> LEPP);
    if (!page)
	return NULL;

    pos = 32 * (blobno & (EPP - 1));

    *maxlen = AFS_PAGESIZE - pos - 1;

    return (struct DirEntry *)(page + pos);
}

/* Given an entries number, return a pointer to that entry */
struct DirEntry *
GetBlob(void *dir, afs_int32 blobno)
{
    afs_size_t maxlen = 0;
    return GetBlobWithLimit(dir, blobno, &maxlen);
}

/* Return an entry, having verified that the name held within the entry
 * doesn't overflow off the end of the directory page it is contained
 * within
 */

int
GetVerifiedBlob(void *file, afs_int32 blobno, struct DirEntry **outdir)
{
    struct DirEntry *dir;
    afs_size_t maxlen;
    char *cp;

    *outdir = NULL;

    DErrno = 0;
    dir = GetBlobWithLimit(file, blobno, &maxlen);
    if (!dir)
	return DErrno;

    /* A blob is only valid if the name within it is NULL terminated before
     * the end of the blob's containing page */
    for (cp = dir->name; *cp != '\0' && cp < ((char *)dir) + maxlen; cp++);

    if (*cp != '\0') {
	DRelease(dir, 0);
	return EIO;
    }

    *outdir = dir;
    return 0;
}

int
DirHash(char *string)
{
    /* Hash a string to a number between 0 and NHASHENT. */
    unsigned char tc;
    unsigned int hval;
    int tval;
    hval = 0;
    while ((tc = (*string++))) {
	hval *= 173;
	hval += tc;
    }
    tval = hval & (NHASHENT - 1);
    if (tval == 0)
	return tval;
    else if (hval >= 1<<31)
	tval = NHASHENT - tval;
    return tval;
}


/* Find a directory entry, given its name.  This entry returns a pointer
 * to a locked buffer, and a pointer to a locked buffer (in previtem)
 * referencing the found item (to aid the delete code).  If no entry is
 * found, however, no items are left locked, and a null pointer is
 * returned instead. */

static struct DirEntry *
FindItem(void *dir, char *ename, unsigned short **previtem)
{
    int i;
    struct DirHeader *dhp;
    unsigned short *lp;
    struct DirEntry *tp;
    int elements;

    i = DirHash(ename);
    dhp = (struct DirHeader *)DRead(dir, 0);
    if (!dhp)
	return 0;
    if (dhp->hashTable[i] == 0) {
	/* no such entry */
	DRelease(dhp, 0);
	return 0;
    }

    GetVerifiedBlob(dir, (u_short) ntohs(dhp->hashTable[i]), &tp);
    if (tp == NULL) {
	DRelease(dhp, 0);
	return NULL;
    }

    lp = &(dhp->hashTable[i]);
    elements = 0;
    /* Detect circular hash chains. Absolute max size of a directory */
    while (elements < BIGMAXPAGES * EPP) {
	elements++;

	/* Look at each hash conflict entry. */
	if (!strcmp(ename, tp->name)) {
	    /* Found our entry. */
	    *previtem = lp;
	    return tp;
	}

	DRelease(lp, 0);
	lp = &(tp->next);
	if (tp->next == 0)
	    break;

	GetVerifiedBlob(dir, (u_short) ntohs(tp->next), &tp);
	if (tp == NULL)
	    break;
    }
    DRelease(lp, 0);
    return NULL;
}

static struct DirEntry *
FindFid (void *dir, afs_uint32 vnode, afs_uint32 unique)
{
    /* Find a directory entry, given the vnode and uniquifier of a object.
     * This entry returns a pointer to a locked buffer.  If no entry is found,
     * however, no items are left locked, and a null pointer is returned
     * instead.
     */
    int i;
    struct DirHeader *dhp;
    unsigned short *lp;
    struct DirEntry *tp;
    int elements;

    dhp = (struct DirHeader *) DRead(dir,0);
    if (!dhp) return 0;
    for (i=0; i<NHASHENT; i++) {
	if (dhp->hashTable[i] != 0) {
	    GetVerifiedBlob(dir,(u_short)ntohs(dhp->hashTable[i]), &tp);
	    if (!tp) { /* should not happen */
		DRelease(dhp, 0);
		return 0;
	    }
	    elements = 0;
	    while(tp && elements < BIGMAXPAGES * EPP) {
		elements++;
		if (vnode == ntohl(tp->fid.vnode)
		    && unique == ntohl(tp->fid.vunique)) {
		    DRelease(dhp, 0);
		    return tp;
		}
		lp = &(tp->next);
		if (tp->next == 0)
		    break;
		GetVerifiedBlob(dir,(u_short)ntohs(tp->next), &tp);
		DRelease(lp, 0);
	    }
	    DRelease(lp, 0);
	}
    }
    DRelease(dhp, 0);
    return NULL;
}

int
InverseLookup (void *dir, afs_uint32 vnode, afs_uint32 unique, char *name,
	       afs_uint32 length)
{
    /* Look for the name pointing to given vnode and unique in a directory */
    struct DirEntry *entry;
    int code = 0;

    entry = FindFid(dir, vnode, unique);
    if (!entry)
	return ENOENT;
    if (strlen(entry->name) >= length)
	code = E2BIG;
    else
	strcpy(name, entry->name);
    DRelease(entry, 0);
    return code;
}

/*!
 * Change an entry fid.
 *
 * \param dir
 * \param entry The entry name.
 * \param old_fid The old find in MKFid format (host order).
 * It can be omitted if you don't need a safety check...
 * \param new_fid The new find in MKFid format (host order).
 */
int ChangeFid(void *dir,
		char *entry,
		afs_uint32 *old_fid,
		afs_uint32 *new_fid)
{
    struct DirEntry *firstitem;
    unsigned short *previtem;
    struct MKFid *fid_old = (struct MKFid *) old_fid;
    struct MKFid *fid_new = (struct MKFid *) new_fid;

    /* Find entry. */
    firstitem = FindItem(dir, entry, &previtem);
    if (firstitem == 0) {
	return ENOENT;
    }
    DRelease(previtem, 1);
    /* Replace fid. */
    if (!old_fid ||
    	((htonl(fid_old->vnode) == firstitem->fid.vnode) &&
    	(htonl(fid_old->vunique) == firstitem->fid.vunique))) {

	firstitem->fid.vnode = htonl(fid_new->vnode);
	firstitem->fid.vunique = htonl(fid_new->vunique);
    }

    DRelease(firstitem, 1);

    return 0;
}
