/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>
#include <afs/com_err.h>
#include <afs/afs_consts.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <errno.h>
#include <assert.h>

#include <osi.h>
#include <afsint.h>
#include <WINNT\afsreg.h>

#include "fs_utils.h"
#include "cmd.h"

#define MAXNAME 100
#define MAXINSIZE 1300    /* pioctl complains if data is larger than this */

static char space[AFS_PIOCTL_MAXSIZE];
static char tspace[1024];

#ifndef WIN32
static struct ubik_client *uclient;
#endif /* not WIN32 */


static char pn[] = "symlink";
static int rxInitDone = 0;

void Die();

#if 0
foldcmp (a, b)
    char *a;
    char *b; {
    char t, u;
    while (1) {
        t = *a++;
        u = *b++;
        if (t >= 'A' && t <= 'Z') t += 0x20;
        if (u >= 'A' && u <= 'Z') u += 0x20;
        if (t != u) return 1;
        if (t == 0) return 0;
    }
}
#endif

/* this function returns TRUE (1) if the file is in AFS, otherwise false (0) */
static int InAFS(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl_utf8(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code) {
	if ((errno == EINVAL) || (errno == ENOENT))
            return 0;
    }
    return 1;
}

static int
IsFreelanceRoot(char *apath)
{
    struct ViceIoctl blob;
    afs_int32 code;

    blob.in_size = 0;
    blob.out_size = AFS_PIOCTL_MAXSIZE;
    blob.out = space;

    code = pioctl_utf8(apath, VIOC_FILE_CELL_NAME, &blob, 1);
    if (code == 0)
        return !strnicmp("Freelance.Local.Root",space,blob.out_size);
    return 1;   /* assume it is because it is more restrictive that way */
}

static const char * NetbiosName(void)
{
    static char buffer[1024] = "AFS";
    HKEY  parmKey;
    DWORD code;
    DWORD dummyLen;
    DWORD enabled = 0;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(buffer);
        code = RegQueryValueEx(parmKey, "NetbiosName", NULL, NULL,
			       buffer, &dummyLen);
        RegCloseKey (parmKey);
    } else {
	strcpy(buffer, "AFS");
    }
    return buffer;
}

#define AFSCLIENT_ADMIN_GROUPNAME "AFS Client Admins"

static BOOL IsAdmin (void)
{
    static BOOL fAdmin = FALSE;
    static BOOL fTested = FALSE;

    if (!fTested)
    {
        /* Obtain the SID for the AFS client admin group.  If the group does
         * not exist, then assume we have AFS client admin privileges.
         */
        PSID psidAdmin = NULL;
        DWORD dwSize, dwSize2;
        char pszAdminGroup[ MAX_COMPUTERNAME_LENGTH + sizeof(AFSCLIENT_ADMIN_GROUPNAME) + 2 ];
        char *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;

        dwSize = sizeof(pszAdminGroup);

        if (!GetComputerName(pszAdminGroup, &dwSize)) {
            /* Can't get computer name.  We return false in this case.
               Retain fAdmin and fTested. This shouldn't happen.*/
            return FALSE;
        }

        dwSize = 0;
        dwSize2 = 0;

        strcat(pszAdminGroup,"\\");
        strcat(pszAdminGroup, AFSCLIENT_ADMIN_GROUPNAME);

        LookupAccountName(NULL, pszAdminGroup, NULL, &dwSize, NULL, &dwSize2, &snu);
        /* that should always fail. */

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            /* if we can't find the group, then we allow the operation */
            fAdmin = TRUE;
            return TRUE;
        }

        if (dwSize == 0 || dwSize2 == 0) {
            /* Paranoia */
            fAdmin = TRUE;
            return TRUE;
        }

        psidAdmin = (PSID)malloc(dwSize); memset(psidAdmin,0,dwSize);
        assert(psidAdmin);
        pszRefDomain = (char *)malloc(dwSize2);
        assert(pszRefDomain);

        if (!LookupAccountName(NULL, pszAdminGroup, psidAdmin, &dwSize, pszRefDomain, &dwSize2, &snu)) {
            /* We can't lookup the group now even though we looked it up earlier.
               Could this happen? */
            fAdmin = TRUE;
        } else {
            /* Then open our current ProcessToken */
            HANDLE hToken;

            if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {

                if (!CheckTokenMembership(hToken, psidAdmin, &fAdmin)) {
                    /* We'll have to allocate a chunk of memory to store the list of
                     * groups to which this user belongs; find out how much memory
                     * we'll need.
                     */
                    DWORD dwSize = 0;
                    PTOKEN_GROUPS pGroups;

                    GetTokenInformation (hToken, TokenGroups, NULL, dwSize, &dwSize);

                    pGroups = (PTOKEN_GROUPS)malloc(dwSize);
                    assert(pGroups);

                    /* Allocate that buffer, and read in the list of groups. */
                    if (GetTokenInformation (hToken, TokenGroups, pGroups, dwSize, &dwSize))
                    {
                        /* Look through the list of group SIDs and see if any of them
                         * matches the AFS Client Admin group SID.
                         */
                        size_t iGroup = 0;
                        for (; (!fAdmin) && (iGroup < pGroups->GroupCount); ++iGroup)
                        {
                            if (EqualSid (psidAdmin, pGroups->Groups[ iGroup ].Sid)) {
                                fAdmin = TRUE;
                            }
                        }
                    }

                    if (pGroups)
                        free(pGroups);
                }

                /* if do not have permission because we were not explicitly listed
                 * in the Admin Client Group let's see if we are the SYSTEM account
                 */
                if (!fAdmin) {
                    PTOKEN_USER pTokenUser;
                    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_NT_AUTHORITY;
                    PSID pSidLocalSystem = 0;
                    DWORD gle;

                    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);

                    pTokenUser = (PTOKEN_USER)malloc(dwSize);
                    assert(pTokenUser);

                    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize))
                        gle = GetLastError();

                    if (AllocateAndInitializeSid( &SIDAuth, 1,
                                                  SECURITY_LOCAL_SYSTEM_RID,
                                                  0, 0, 0, 0, 0, 0, 0,
                                                  &pSidLocalSystem))
                    {
                        if (EqualSid(pTokenUser->User.Sid, pSidLocalSystem)) {
                            fAdmin = TRUE;
                        }

                        FreeSid(pSidLocalSystem);
                    }

                    if ( pTokenUser )
                        free(pTokenUser);
                }
            }
        }

        free(psidAdmin);
        free(pszRefDomain);

        fTested = TRUE;
    }

    return fAdmin;
}

/* return a static pointer to a buffer */
static char *Parent(char *apath)
{
    char *tp;
    strcpy(tspace, apath);
    tp = strrchr(tspace, '\\');
    if (tp) {
        if (tp - tspace > 2 &&
            tspace[1] == ':' &&
            &tspace[2] == tp)
	    *(tp+1) = 0;	/* lv trailing slash so Parent("k:\foo") is "k:\" not "k:" */
        else
            *tp = 0;
    }
    else {
	fs_ExtractDriveLetter(apath, tspace);
    	strcat(tspace, ".");
    }
    return tspace;
}


static ListLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    int error;
    struct cmd_item *ti;
    char orig_name[1024];		/*Original name, may be modified*/
    char true_name[1024];		/*``True'' dirname (e.g., symlink target)*/
    char parent_dir[1024];		/*Parent directory of true name*/
    char *last_component;	/*Last component of true name*/
#ifndef WIN32
    struct stat statbuff;		/*Buffer for status info*/
#endif /* not WIN32 */
#ifndef WIN32
    int link_chars_read;		/*Num chars read in readlink()*/
#endif /* not WIN32 */
    int	thru_symlink;			/*Did we get to a mount point via a symlink?*/

    error = 0;
    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	thru_symlink = 0;
#ifdef WIN32
	strcpy(orig_name, ti->data);
#else /* not WIN32 */
	sprintf(orig_name, "%s%s",
		(ti->data[0] == '/') ? "" : "./",
		ti->data);
#endif /* not WIN32 */

#ifndef WIN32
	if (lstat(orig_name, &statbuff) < 0) {
	    /* if lstat fails, we should still try the pioctl, since it
		may work (for example, lstat will fail, but pioctl will
		    work if the volume of offline (returning ENODEV). */
	    statbuff.st_mode = S_IFDIR; /* lie like pros */
	}

	/*
	 * The lstat succeeded.  If the given file is a symlink, substitute
	 * the file name with the link name.
	 */
	if ((statbuff.st_mode & S_IFMT) == S_IFLNK) {
	    thru_symlink = 1;
	    /*
	     * Read name of resolved file.
	     */
	    link_chars_read = readlink(orig_name, true_name, 1024);
	    if (link_chars_read <= 0) {
		fprintf(stderr,"%s: Can't read target name for '%s' symbolic link!\n",
		       pn, orig_name);
		exit(1);
	    }

	    /*
	     * Add a trailing null to what was read, bump the length.
	     */
	    true_name[link_chars_read++] = 0;

	    /*
	     * If the symlink is an absolute pathname, we're fine.  Otherwise, we
	     * have to create a full pathname using the original name and the
	     * relative symlink name.  Find the rightmost slash in the original
	     * name (we know there is one) and splice in the symlink value.
	     */
	    if (true_name[0] != '\\') {
		last_component = (char *) strrchr(orig_name, '\\');
		strcpy(++last_component, true_name);
		strcpy(true_name, orig_name);
	    }
	}
	else
	    strcpy(true_name, orig_name);
#else	/* WIN32 */
	strcpy(true_name, orig_name);
#endif /* WIN32 */

	/*
	 * Find rightmost slash, if any.
	 */
	last_component = (char *) strrchr(true_name, '\\');
	if (!last_component)
	    last_component = (char *) strrchr(true_name, '/');
	if (last_component) {
	    /*
	     * Found it.  Designate everything before it as the parent directory,
	     * everything after it as the final component.
	     */
	    strncpy(parent_dir, true_name, last_component - true_name + 1);
	    parent_dir[last_component - true_name + 1] = 0;
	    last_component++;   /*Skip the slash*/

#ifdef WIN32
	    if (!InAFS(parent_dir)) {
		const char * nbname = NetbiosName();
		int len = (int)strlen(nbname);

		if (parent_dir[0] == '\\' && parent_dir[1] == '\\' &&
		    parent_dir[len+2] == '\\' &&
		    parent_dir[len+3] == '\0' &&
		    !strnicmp(nbname,&parent_dir[2],len))
		{
		    sprintf(parent_dir,"\\\\%s\\all\\", nbname);
		}
	    }
#endif
	}
	else {
	    /*
	     * No slash appears in the given file name.  Set parent_dir to the current
	     * directory, and the last component as the given name.
	     */
	    fs_ExtractDriveLetter(true_name, parent_dir);
	    strcat(parent_dir, ".");
	    last_component = true_name;
            fs_StripDriveLetter(true_name, true_name, sizeof(true_name));
	}

	if (strcmp(last_component, ".") == 0 || strcmp(last_component, "..") == 0) {
	    fprintf(stderr,"%s: you may not use '.' or '..' as the last component\n", pn);
	    fprintf(stderr,"%s: of a name in the 'symlink list' command.\n", pn);
	    error = 1;
	    continue;
	}
	blob.in = last_component;
	blob.in_size = (long)strlen(last_component)+1;
	blob.out_size = AFS_PIOCTL_MAXSIZE;
	blob.out = space;
	memset(space, 0, AFS_PIOCTL_MAXSIZE);

	code = pioctl_utf8(parent_dir, VIOC_LISTSYMLINK, &blob, 1);

	if (code == 0)
	    printf("'%s' is a %ssymlink to '%.*s'\n",
		   ti->data,
		   (thru_symlink ? "symbolic link, leading to a " : ""),
		   blob.out_size,
                   space);

	else {
	    error = 1;
	    if (errno == EINVAL)
		fprintf(stderr,"'%s' is not a symlink.\n",
		       ti->data);
	    else {
		Die(errno, (ti->data ? ti->data : parent_dir));
	    }
	}
    }
    return error;
}

static MakeLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;
    struct ViceIoctl blob;
    char * parent;
    char path[1024] = "";

    strcpy(path, as->parms[0].items->data);
    parent = Parent(path);

    if (!InAFS(parent)) {
#ifdef WIN32
	const char * nbname = NetbiosName();
	int len = (int)strlen(nbname);

	if (parent[0] == '\\' && parent[1] == '\\' &&
	    (parent[len+2] == '\\' && parent[len+3] == '\0' || parent[len+2] == '\0') &&
	    !strnicmp(nbname,&parent[2],len))
	{
	    sprintf(path,"%s%sall%s", parent, parent[len+2]?"":"\\",
                    &as->parms[0].items->data[strlen(parent)]);
	    parent = Parent(path);
	    if (!InAFS(parent)) {
		fprintf(stderr,"%s: symlinks must be created within the AFS file system\n", pn);
		return 1;
	    }
	} else
#endif
	{
	    fprintf(stderr,"%s: symlinks must be created within the AFS file system\n", pn);
	    return 1;
	}
    }

#ifdef WIN32
    if ( IsFreelanceRoot(parent) && !IsAdmin() ) {
	fprintf(stderr,"%s: Only AFS Client Administrators may alter the root.afs volume\n", pn);
	return 1;
    }

    fprintf(stderr, "Creating symlink [%s] to [%s]\n", path, as->parms[1].items->data);

    /* create symlink with a special pioctl for Windows NT, since it doesn't
     * have a symlink system call.
     */
    blob.out_size = 0;
    blob.in_size = 1 + (long)strlen(as->parms[1].items->data);
    blob.in = as->parms[1].items->data;
    blob.out = NULL;
    code = pioctl_utf8(path, VIOC_SYMLINK, &blob, 0);
#else /* not WIN32 */
    code = symlink(as->parms[1].items->data, path);
#endif /* not WIN32 */
    if (code) {
	Die(errno, as->parms[0].items->data);
	return 1;
    }
    return 0;
}

/*
 * Delete AFS symlinks.  Variables are used as follows:
 *       tbuffer: Set to point to the null-terminated directory name of the
 *	    symlink (or ``.'' if none is provided)
 *      tp: Set to point to the actual name of the symlink to nuke.
 */
static RemoveLinkCmd(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code=0;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    char tbuffer[1024];
    char lsbuffer[1024];
    char *tp;

    for(ti=as->parms[0].items; ti; ti=ti->next) {
	/* once per file */
	tp = (char *) strrchr(ti->data, '\\');
	if (!tp)
	    tp = (char *) strrchr(ti->data, '/');
	if (tp) {
	    strncpy(tbuffer, ti->data, code=(afs_int32)(tp-ti->data+1));  /* the dir name */
            tbuffer[code] = 0;
	    tp++;   /* skip the slash */

#ifdef WIN32
	    if (!InAFS(tbuffer)) {
		const char * nbname = NetbiosName();
		int len = (int)strlen(nbname);

		if (tbuffer[0] == '\\' && tbuffer[1] == '\\' &&
		     tbuffer[len+2] == '\\' &&
		     tbuffer[len+3] == '\0' &&
		     !strnicmp(nbname,&tbuffer[2],len))
		{
		    sprintf(tbuffer,"\\\\%s\\all\\", nbname);
		}
	    }
#endif
	}
	else {
	    fs_ExtractDriveLetter(ti->data, tbuffer);
	    strcat(tbuffer, ".");
	    tp = ti->data;
            fs_StripDriveLetter(tp, tp, 0);
	}
	blob.in = tp;
	blob.in_size = (int)strlen(tp)+1;
	blob.out = lsbuffer;
	blob.out_size = sizeof(lsbuffer);
	code = pioctl_utf8(tbuffer, VIOC_LISTSYMLINK, &blob, 0);
	if (code) {
	    if (errno == EINVAL)
		fprintf(stderr,"symlink: '%s' is not a symlink.\n", ti->data);
	    else {
		Die(errno, ti->data);
	    }
	    continue;	/* don't bother trying */
	}

        if ( IsFreelanceRoot(tbuffer) && !IsAdmin() ) {
            fprintf(stderr,"symlink: Only AFS Client Administrators may alter the root.afs volume\n");
            code = 1;
            continue;   /* skip */
        }

	blob.out_size = 0;
	blob.in = tp;
	blob.in_size = (long)strlen(tp)+1;
	code = pioctl_utf8(tbuffer, VIOC_DELSYMLINK, &blob, 0);
	if (code) {
	    Die(errno, ti->data);
	}
    }
    return code;
}

static void
FreeUtf8CmdLine(int argc, char ** argv)
{
    int i;
    for (i=0; i < argc; i++) {
        if (argv[i])
            free(argv[i]);
    }
    free(argv);
}

static char **
MakeUtf8Cmdline(int argc, const wchar_t **wargv)
{
    char ** argv;
    int i;

    argv = calloc(argc, sizeof(argv[0]));
    if (argv == NULL)
        return NULL;

    for (i=0; i < argc; i++) {
        int s;

        s = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, FALSE);
        if (s == 0 ||
            (argv[i] = calloc(s+1, sizeof(char))) == NULL) {
            break;
        }

        s = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], s+1, NULL, FALSE);
        if (s == 0) {
            break;
        }
    }

    if (i < argc) {
        FreeUtf8CmdLine(argc, argv);
        return NULL;
    }

    return argv;
}

static    struct ViceIoctl gblob;
static int debug = 0;

int wmain(int argc, wchar_t **wargv)
{
    afs_int32 code;
    struct cmd_syndesc *ts;
    char ** argv;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

#ifdef WIN32
    WSADATA WSAjunk;
    WSAStartup(0x0101, &WSAjunk);
#endif /* WIN32 */

    /* try to find volume location information */

    argv = MakeUtf8Cmdline(argc, wargv);

    osi_Init();

    ts = cmd_CreateSyntax("list", ListLinkCmd, NULL, "list symlink");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");

    ts = cmd_CreateSyntax("make", MakeLinkCmd, NULL, "make symlink");
    cmd_AddParm(ts, "-name", CMD_SINGLE, 0, "name");
    cmd_AddParm(ts, "-to", CMD_SINGLE, 0, "target");

    ts = cmd_CreateSyntax("remove", RemoveLinkCmd, NULL, "remove symlink");
    cmd_AddParm(ts, "-name", CMD_LIST, 0, "name");
    cmd_CreateAlias(ts, "rm");

    code = cmd_Dispatch(argc, argv);

#ifndef WIN32
    if (rxInitDone) rx_Finalize();
#endif /* not WIN32 */

    FreeUtf8CmdLine(argc, argv);

    return code;
}

void Die(code, filename)
    int   code;
    char *filename;
{ /*Die*/

    if (code == EINVAL) {
	if (filename)
	    fprintf(stderr,"%s: Invalid argument; it is possible that %s is not in AFS.\n", pn, filename);
	else fprintf(stderr,"%s: Invalid argument.\n", pn);
    }
    else if (code == ENOENT) {
	if (filename) fprintf(stderr,"%s: File '%s' doesn't exist.\n", pn, filename);
	else fprintf(stderr,"%s: no such file returned.\n", pn);
    }
    else if (code == EEXIST) {
	if (filename) fprintf(stderr,"%s: File '%s' already exists.\n", pn, filename);
	else fprintf(stderr,"%s: the specified file already exists.\n", pn);
    }
    else if (code == EROFS)  fprintf(stderr,"%s: You can not change a backup or readonly volume\n", pn);
    else if (code == EACCES || code == EPERM) {
	if (filename) fprintf(stderr,"%s: You don't have the required access rights on '%s'\n", pn, filename);
	else fprintf(stderr,"%s: You do not have the required rights to do this operation\n", pn);
    }
    else if (code == ENODEV) {
	fprintf(stderr,"%s: AFS service may not have started.\n", pn);
    }
    else if (code == ESRCH) {
	fprintf(stderr,"%s: Cell name not recognized.\n", pn);
    }
    else if (code == EPIPE) {
	fprintf(stderr,"%s: Volume name or ID not recognized.\n", pn);
    }
    else if (code == EFBIG) {
	fprintf(stderr,"%s: Cache size too large.\n", pn);
    }
    else if (code == ETIMEDOUT) {
	if (filename)
	    fprintf(stderr,"%s:'%s': Connection timed out", pn, filename);
	else
	    fprintf(stderr,"%s: Connection timed out", pn);
    }
    else {
	if (filename) fprintf(stderr,"%s:'%s'", pn, filename);
	else fprintf(stderr,"%s", pn);
#ifdef WIN32
	fprintf(stderr, ": code 0x%x\n", code);
#else /* not WIN32 */
	fprintf(stderr,": %s\n", afs_error_message(code));
#endif /* not WIN32 */
    }
} /*Die*/
