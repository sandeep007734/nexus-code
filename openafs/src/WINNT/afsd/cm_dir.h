/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_DIR_H
#define OPENAFS_WINNT_AFSD_CM_DIR_H 1

/* These data structures are derived from src/dir/dir.h and should not
 * be changed as they describe AFS3 wire protocol.
 *
 */

#define CM_DIR_PAGESIZE		2048		/* bytes per page */
#define CM_DIR_NHASHENT		128		/* entries in the dir hash tbl */
#define CM_DIR_MAXPAGES		128		/* max pages in a dir */
#define CM_DIR_BIGMAXPAGES	1023		/* new big max pages */
#define CM_DIR_EPP		64		/* dir entries per page */
#define CM_DIR_LEPP		6		/* log above */
#define CM_DIR_CHUNKSIZE	32		/* bytes per dir entry chunk */

/* When this next field changs, it is crucial to modify MakeDir, since the latter is
 * responsible for marking these entries as allocated.  Also change
 * the salvager.
 */
#define CM_DIR_DHE		12		/* entries in a dir header above a pages
						 * header alone.
						 */

#define CM_DIR_FFIRST           1
#define CM_DIR_FNEXT            2

typedef struct cm_dirFid {
	/* A file identifier. */
	afs_int32 vnode;	/* file's vnode slot */
	afs_int32 unique;	/* the slot incarnation number */
} cm_dirFid_t;

typedef struct cm_pageHeader {
	/* A page header entry. */
	unsigned short pgcount;	/* number of pages, or 0 if old-style */
	unsigned short tag;		/* '1234' in network byte order */
	char freeCount;	/* unused, info in dirHeader structure */
	char freeBitmap[CM_DIR_EPP/8];
	char padding[CM_DIR_CHUNKSIZE-(5+CM_DIR_EPP/8)];/* pad to one 32-byte entry */
} cm_pageHeader_t;

/* a total of 13 32-byte entries, 1 for the header that in all pages, and
 * 12 more special ones for the entries in a the first page.
 */
typedef struct cm_dirHeader {
	/* A directory header object. */
	cm_pageHeader_t header;
	char alloMap[CM_DIR_MAXPAGES];    /* one byte per 2K page */
	unsigned short hashTable[CM_DIR_NHASHENT];
} cm_dirHeader_t;

/* this represents a directory entry.  We use strlen to find out how
 * many bytes are really in the dir entry; it is always a multiple of
 * 32.
 */
typedef struct cm_dirEntry {
	/* A directory entry */
	char flag;	/* this must be FFIRST (1) */
	char length;	/* currently unused */
	unsigned short next;
	cm_dirFid_t fid;
	char name[1];	/* the real length is determined with strlen() */
} cm_dirEntry_t;

#ifdef UNUSED
typedef struct cm_dirXEntry {
	/* A directory extension entry. */
	char name[32];
} cm_dirXEntry_t;

typedef struct cm_dirPage0 {
	/* A page in a directory. */
	cm_dirHeader_t header;
	cm_dirEntry_t entry[1];
} cm_dirPage0_t;

typedef struct cm_dirPage1 {
	/* A page in a directory. */
	cm_pageHeader_t header;
	cm_dirEntry_t entry[1];
} cm_dirPage1_t;
#endif /* UNUSED */

#define CM_DIROP_MAXBUFFERS 8

typedef struct cm_dirOpBuffer {
    int        flags;
    cm_buf_t * bufferp;
    int        refcount;
} cm_dirOpBuffer_t;

#define CM_DIROPBUFF_INUSE      0x1

/* lock types */
#define CM_DIRLOCK_NONE         0x0
#define CM_DIRLOCK_READ         0x1
#define CM_DIRLOCK_WRITE        0x2

/* flags for cm_dirOp operations */
#define CM_DIROP_FLAG_NONE         0x0000
#define CM_DIROP_FLAG_NOBUILDTREE  0x0001

/* Used for managing transactional directory operations.  Each
   instance should only be used by one thread. */
typedef struct cm_dirOp {
    int           lockType;
    cm_scache_t * scp;
    cm_user_t *   userp;
    cm_req_t      req;

    osi_hyper_t   length;       /* scp->length at the time
                                   cm_BeginDirOp() was called.*/
    osi_hyper_t   newLength;    /* adjusted scp->length */
    afs_uint64    dataVersion;  /* scp->dataVersion when
                                   cm_BeginDirOp() was called.*/
    afs_uint64    newDataVersion; /* scp->dataVersion when
                                     cm_CheckDirOpForSingleChange()
                                     was called. */

    afs_uint64    dirtyBufCount;

    afs_uint64    nBuffers;     /* number of buffers below */
    cm_dirOpBuffer_t buffers[CM_DIROP_MAXBUFFERS];
} cm_dirOp_t;

extern long
cm_BeginDirOp(cm_scache_t * scp, cm_user_t * userp, cm_req_t * reqp,
              afs_uint32 lockType, afs_uint32 flags, cm_dirOp_t * op);

extern int
cm_CheckDirOpForSingleChange(cm_dirOp_t * op);

extern long
cm_EndDirOp(cm_dirOp_t * op);

extern long
cm_NameEntries(char *namep, size_t *lenp);

extern long
cm_DirCreateEntry(cm_dirOp_t * op, char *entry, cm_fid_t * cfid);

extern int
cm_DirLength(cm_dirOp_t * op);

extern int
cm_DirDeleteEntry(cm_dirOp_t * op, char *entry);

extern int
cm_DirMakeDir(cm_dirOp_t * op, cm_fid_t * me, cm_fid_t * parent);

extern int
cm_DirLookup(cm_dirOp_t * op, char *entry, cm_fid_t * cfid);

extern int
cm_DirLookupOffset(cm_dirOp_t * op, char *entry, cm_fid_t *cfid, osi_hyper_t *offsetp);

extern int
cm_DirApply(cm_dirOp_t * op, int (*hookproc) (void *, char *, long, long), void *hook);

extern int
cm_DirIsEmpty(cm_dirOp_t * op);

extern int
cm_DirHash(char *string);

/* Directory entry lists */
typedef struct cm_dirEntryList {
    struct cm_dirEntryList * nextp;
    char   name[1];
} cm_dirEntryList_t;

extern void
cm_DirEntryListAdd(char * namep, cm_dirEntryList_t ** list);

extern void
cm_DirEntryListFree(cm_dirEntryList_t ** list);

extern void
cm_DirDumpStats(void);

extern int
cm_MemDumpDirStats(FILE *outputFile, char *cookie, int lock);

extern afs_uint64 dir_enums;

#endif /*  OPENAFS_WINNT_AFSD_CM_DIR_H */
