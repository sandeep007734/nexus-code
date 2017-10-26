/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * login [ name ]
 * login -r hostname (for rlogind)
 * login -h hostname (for telnetd, etc.)
 */
#include <afsconfig.h>
#include <afs/param.h>


#if !defined(AFS_SUN_ENV) && !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#include <sys/param.h>

#define quota(a,b,c,d) 0

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <afs/afsutil.h>
#include <sgtty.h>
#include <utmp.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <lastlog.h>
#include <errno.h>
#include <ttyent.h>


#include <syslog.h>

#include <grp.h>

static gid_t tty_gid(int default_gid);
static void getloginname(struct utmp *up);

#define TTYGRPNAME	"tty"	/* name of group to own ttys */
#define TTYGID(gid)	tty_gid(gid)	/* gid that owns all ttys */

#define	SCMPN(a, b)	strncmp(a, b, sizeof(a))
#define	SCPYN(a, b)	strncpy(a, b, sizeof(a))

#define NMAX	sizeof(utmp.ut_name)
#define HMAX	sizeof(utmp.ut_host)

#define	FALSE	0
#define	TRUE	-1

char nolog[] = "/etc/nologin";
char qlog[] = ".hushlogin";
char maildir[30] = "/usr/spool/mail/";
char lastlog[] = "/usr/adm/lastlog";
struct passwd nouser;
struct sgttyb ttyb;
struct utmp utmp;
char minusnam[16] = "-";
char *envinit[] = { 0 };	/* now set by setenv calls */

/*
 * This bounds the time given to login.  We initialize it here
 * so it can be patched on machines where it's too small.
 */
int timeout = 60;

char term[64];

struct passwd *pwd;
char *strcat(), *malloc(), *realloc();
static void timedout(void);
static void showmotd(void);
static void doremoteterm(char *term, struct sgttyb *tp);
static void setenv(char *var, char *value, int clobber);
char *ttyname();
char *crypt();
char *getpass();
char *stypeof();
extern char **environ;


struct tchars tc = {
    CINTR, CQUIT, CSTART, CSTOP, CEOT, CBRK
};
struct ltchars ltc = {
    CSUSP, CDSUSP, CRPRNT, CFLUSH, CWERASE, CLNEXT
};

struct winsize win = { 0, 0, 0, 0 };

int rflag;
int usererr = -1;
char rusername[NMAX + 1], lusername[NMAX + 1];
char rpassword[NMAX + 1];
char name[NMAX + 1];
char *rhost;

int
osi_audit(void)
{
    /* this sucks but it works for now.
     */
    return 0;
}


#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    char *namep;
    int pflag = 0, hflag = 0, t, f, c;
    int invalid, quietlog;
    FILE *nlfd;
    char *ttyn, *tty;
    int ldisc = 0, zero = 0, i;
    char **envnew;

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
    signal(SIGALRM, timedout);
    alarm(timeout);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    setpriority(PRIO_PROCESS, 0, 0);
    quota(Q_SETUID, 0, 0, 0);

    /* create a dummy user */
    memset(&nouser, 0, sizeof(nouser));
    nouser.pw_name = "";
    nouser.pw_passwd = "*";
    nouser.pw_dir = nouser.pw_shell = "";
    nouser.pw_uid = nouser.pw_gid = -1;

    /*
     * -p is used by getty to tell login not to destroy the environment
     * -r is used by rlogind to cause the autologin protocol;
     * -h is used by other servers to pass the name of the
     * remote host to login so that it may be placed in utmp and wtmp
     */
    while (argc > 1) {
	if (strcmp(argv[1], "-r") == 0) {
	    if (rflag || hflag) {
		printf("Only one of -r and -h allowed\n");
		exit(1);
	    }
	    rflag = 1;
	    usererr = doremotelogin(argv[2]);
	    SCPYN(utmp.ut_host, argv[2]);
	    argc -= 2;
	    argv += 2;
	    continue;
	}
	if (strcmp(argv[1], "-h") == 0 && getuid() == 0) {
	    if (rflag || hflag) {
		printf("Only one of -r and -h allowed\n");
		exit(1);
	    }
	    hflag = 1;
	    SCPYN(utmp.ut_host, argv[2]);
	    argc -= 2;
	    argv += 2;
	    continue;
	}
	if (strcmp(argv[1], "-p") == 0) {
	    argc--;
	    argv++;
	    pflag = 1;
	    continue;
	}
	break;
    }
    ioctl(0, TIOCLSET, &zero);
    ioctl(0, TIOCNXCL, 0);
    ioctl(0, FIONBIO, &zero);
    ioctl(0, FIOASYNC, &zero);
    ioctl(0, TIOCGETP, &ttyb);
    /*
     * If talking to an rlogin process,
     * propagate the terminal type and
     * baud rate across the network.
     */
    if (rflag)
	doremoteterm(term, &ttyb);
    ttyb.sg_erase = CERASE;
    ttyb.sg_kill = CKILL;
    ioctl(0, TIOCSLTC, &ltc);
    ioctl(0, TIOCSETC, &tc);
    ioctl(0, TIOCSETP, &ttyb);
    for (t = getdtablesize(); t > 2; t--)
	close(t);
    ttyn = ttyname(0);
    if (ttyn == (char *)0 || *ttyn == '\0')
	ttyn = "/dev/tty??";
    tty = strrchr(ttyn, '/');
    if (tty == NULL)
	tty = ttyn;
    else
	tty++;
    openlog("login", LOG_ODELAY, LOG_AUTH);
    t = 0;
    invalid = FALSE;
    do {
	ldisc = 0;
	ioctl(0, TIOCSETD, &ldisc);
	SCPYN(utmp.ut_name, "");
	/*
	 * Name specified, take it.
	 */
	if (argc > 1) {
	    SCPYN(utmp.ut_name, argv[1]);
	    argc = 0;
	}
	/*
	 * If remote login take given name,
	 * otherwise prompt user for something.
	 */
	if (rflag && !invalid)
	    SCPYN(utmp.ut_name, lusername);
	else
	    getloginname(&utmp);
	invalid = FALSE;
	if (!strcmp(pwd->pw_shell, "/bin/csh")) {
	    ldisc = NTTYDISC;
	    ioctl(0, TIOCSETD, &ldisc);
	}
	/*
	 * If no remote login authentication and
	 * a password exists for this user, prompt
	 * for one and verify it.
	 */
	if (usererr == -1 && *pwd->pw_passwd != '\0') {
#ifdef KAUTH
	    char password[BUFSIZ];
	    char *reason;
	    if (ka_UserReadPassword
		("Password:", password, sizeof(password), &reason)) {
		printf("Unable to login because %s,\n", reason);
		invalid = TRUE;
	    } else
		if (ka_UserAuthenticate
		    (pwd->pw_name, /*inst */ 0, /*realm */ 0, password,
		     /*sepag */ 1, &reason)) {
		printf("Unable to authenticate to AFS because %s.\n", reason);
		printf("  proceeding with local authentication... \n");
		/* try local login */
		namep = crypt(password, pwd->pw_passwd);
		if (strcmp(namep, pwd->pw_passwd))
		    invalid = TRUE;
	    }
#else
	    char *pp;
	    setpriority(PRIO_PROCESS, 0, -4);
	    pp = getpass("Password:");
	    namep = crypt(pp, pwd->pw_passwd);
	    setpriority(PRIO_PROCESS, 0, 0);
	    if (strcmp(namep, pwd->pw_passwd))
		invalid = TRUE;
#endif
	}
	/*
	 * If user not super-user, check for logins disabled.
	 */
	if (pwd->pw_uid != 0 && (nlfd = fopen(nolog, "r")) != 0) {
	    while ((c = getc(nlfd)) != EOF)
		putchar(c);
	    fflush(stdout);
	    sleep(5);
	    exit(0);
	}
	/*
	 * If valid so far and root is logging in,
	 * see if root logins on this terminal are permitted.
	 */
	if (!invalid && pwd->pw_uid == 0 && !rootterm(tty)) {
	    if (utmp.ut_host[0])
		syslog(LOG_CRIT, "ROOT LOGIN REFUSED ON %s FROM %.*s", tty,
		       HMAX, utmp.ut_host);
	    else
		syslog(LOG_CRIT, "ROOT LOGIN REFUSED ON %s", tty);
	    invalid = TRUE;
	}
	if (invalid) {
	    printf("Login incorrect\n");
	    if (++t >= 5) {
		if (utmp.ut_host[0])
		    syslog(LOG_CRIT,
			   "REPEATED LOGIN FAILURES ON %s FROM %.*s, %.*s",
			   tty, HMAX, utmp.ut_host, NMAX, utmp.ut_name);
		else
		    syslog(LOG_CRIT, "REPEATED LOGIN FAILURES ON %s, %.*s",
			   tty, NMAX, utmp.ut_name);
		ioctl(0, TIOCHPCL, NULL);
		close(0), close(1), close(2);
		sleep(10);
		exit(1);
	    }
	}
	if (*pwd->pw_shell == '\0')
	    pwd->pw_shell = "/bin/sh";
	if (chdir(pwd->pw_dir) < 0 && !invalid) {
	    if (chdir("/") < 0) {
		printf("No directory!\n");
		invalid = TRUE;
	    } else {
		printf("No directory! %s\n", "Logging in with home=/");
		pwd->pw_dir = "/";
	    }
	}
	/*
	 * Remote login invalid must have been because
	 * of a restriction of some sort, no extra chances.
	 */
	if (!usererr && invalid)
	    exit(1);
    } while (invalid);
/* committed to login turn off timeout */
    alarm(0);

    if (quota(Q_SETUID, pwd->pw_uid, 0, 0) < 0 && errno != EINVAL) {
	if (errno == EUSERS)
	    printf("%s.\n%s.\n", "Too many users logged on already",
		   "Try again later");
	else if (errno == EPROCLIM)
	    printf("You have too many processes running.\n");
	else
	    perror("quota (Q_SETUID)");
	sleep(5);
	exit(0);
    }
    time(&utmp.ut_time);
    t = ttyslot();
    if (t > 0 && (f = open("/etc/utmp", O_WRONLY)) >= 0) {
	lseek(f, (afs_int32) (t * sizeof(utmp)), 0);
	SCPYN(utmp.ut_line, tty);
	write(f, (char *)&utmp, sizeof(utmp));
	close(f);
    }
    if ((f = open("/usr/adm/wtmp", O_WRONLY | O_APPEND)) >= 0) {
	write(f, (char *)&utmp, sizeof(utmp));
	close(f);
    }
    quietlog = access(qlog, F_OK) == 0;
    if ((f = open(lastlog, O_RDWR)) >= 0) {
	struct lastlog ll;

	lseek(f, (afs_int32) pwd->pw_uid * sizeof(struct lastlog), 0);
	if (read(f, (char *)&ll, sizeof ll) == sizeof ll && ll.ll_time != 0
	    && !quietlog) {
	    printf("Last login: %.*s ", 24 - 5, (char *)ctime(&ll.ll_time));
	    if (*ll.ll_host != '\0')
		printf("from %.*s\n", sizeof(ll.ll_host), ll.ll_host);
	    else
		printf("on %.*s\n", sizeof(ll.ll_line), ll.ll_line);
	}
	lseek(f, (afs_int32) pwd->pw_uid * sizeof(struct lastlog), 0);
	time(&ll.ll_time);
	SCPYN(ll.ll_line, tty);
	SCPYN(ll.ll_host, utmp.ut_host);
	write(f, (char *)&ll, sizeof ll);
	close(f);
    }
    chown(ttyn, pwd->pw_uid, TTYGID(pwd->pw_gid));
    if (!hflag && !rflag)	/* XXX */
	ioctl(0, TIOCSWINSZ, &win);
    chmod(ttyn, 0620);
    setgid(pwd->pw_gid);
    strncpy(name, utmp.ut_name, NMAX);
    name[NMAX] = '\0';
    initgroups(name, pwd->pw_gid);
    quota(Q_DOWARN, pwd->pw_uid, (dev_t) - 1, 0);
    setuid(pwd->pw_uid);
    /* destroy environment unless user has asked to preserve it */
    if (!pflag)
	environ = envinit;

    /* set up environment, this time without destruction */
    /* copy the environment before setenving */
    i = 0;
    while (environ[i] != NULL)
	i++;
    envnew = (char **)malloc(sizeof(char *) * (i + 1));
    for (; i >= 0; i--)
	envnew[i] = environ[i];
    environ = envnew;

    setenv("HOME=", pwd->pw_dir, 1);
    setenv("SHELL=", pwd->pw_shell, 1);
    if (term[0] == '\0')
	strncpy(term, stypeof(tty), sizeof(term));
    setenv("TERM=", term, 0);
    setenv("USER=", pwd->pw_name, 1);
    setenv("PATH=", ":/usr/ucb:/bin:/usr/bin", 0);

    if ((namep = strrchr(pwd->pw_shell, '/')) == NULL)
	namep = pwd->pw_shell;
    else
	namep++;
    strcat(minusnam, namep);
    if (tty[sizeof("tty") - 1] == 'd')
	syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);
    if (pwd->pw_uid == 0)
	if (utmp.ut_host[0])
	    syslog(LOG_NOTICE, "ROOT LOGIN %s FROM %.*s", tty, HMAX,
		   utmp.ut_host);
	else
	    syslog(LOG_NOTICE, "ROOT LOGIN %s", tty);
    if (!quietlog) {
	struct stat st;

	showmotd();
	strcat(maildir, pwd->pw_name);
	if (stat(maildir, &st) == 0 && st.st_size != 0)
	    printf("You have %smail.\n",
		   (st.st_mtime > st.st_atime) ? "new " : "");
    }
    signal(SIGALRM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_IGN);
    execlp(pwd->pw_shell, minusnam, 0);
    perror(pwd->pw_shell);
    printf("No shell\n");
    exit(0);
}

static void
getloginname(struct utmp *up)
{
    char *namep;
    int c;

    while (up->ut_name[0] == '\0') {
	namep = up->ut_name;
	printf("login: ");
	while ((c = getchar()) != '\n') {
	    if (c == ' ')
		c = '_';
	    if (c == EOF)
		exit(0);
	    if (namep < up->ut_name + NMAX)
		*namep++ = (char)c;
	}
    }
    strncpy(lusername, up->ut_name, NMAX);
    lusername[NMAX] = 0;
    if ((pwd = getpwnam(lusername)) == NULL)
	pwd = &nouser;
}

static void
timedout(void)
{

    printf("Login timed out after %d seconds\n", timeout);
    exit(0);
}

int stopmotd;
static void
catch(void)
{

    signal(SIGINT, SIG_IGN);
    stopmotd++;
}

int
rootterm(char *tty)
{
    struct ttyent *t;

    if ((t = getttynam(tty)) != NULL) {
	if (t->ty_status & TTY_SECURE)
	    return (1);
    }
    return (0);
}

static void
showmotd(void)
{
    FILE *mf;
    int c;

    signal(SIGINT, catch);
    if ((mf = fopen("/etc/motd", "r")) != NULL) {
	while ((c = getc(mf)) != EOF && stopmotd == 0)
	    putchar(c);
	fclose(mf);
    }
    signal(SIGINT, SIG_IGN);
}

#undef	UNKNOWN
#define UNKNOWN "su"

char *
stypeof(char *ttyid)
{
    struct ttyent *t;

    if (ttyid == NULL || (t = getttynam(ttyid)) == NULL)
	return (UNKNOWN);
    return (t->ty_type);
}

int
doremotelogin(char *host)
{
    getstr(rusername, sizeof(rusername), "remuser");
    getstr(lusername, sizeof(lusername), "locuser");
    getstr(term, sizeof(term), "Terminal type");
    if (getuid()) {
	pwd = &nouser;
	return (-1);
    }
    pwd = getpwnam(lusername);
    if (pwd == NULL) {
	pwd = &nouser;
	return (-1);
    }
    return (ruserok(host, (pwd->pw_uid == 0), rusername, lusername));
}

int
getstr(char *buf, int cnt, char *err)
{
    char c;

    do {
	if (read(0, &c, 1) != 1)
	    exit(1);
	if (--cnt < 0) {
	    printf("%s too long\r\n", err);
	    exit(1);
	}
	*buf++ = c;
    } while (c != 0);
}

char *speeds[] = { "0", "50", "75", "110", "134", "150", "200", "300",
    "600", "1200", "1800", "2400", "4800", "9600", "19200", "38400"
};

#define	NSPEEDS	(sizeof (speeds) / sizeof (speeds[0]))

static void
doremoteterm(char *term, struct sgttyb *tp)
{
    char *cp = strchr(term, '/'), **cpp;
    char *speed;

    if (cp) {
	*cp++ = '\0';
	speed = cp;
	cp = strchr(speed, '/');
	if (cp)
	    *cp++ = '\0';
	for (cpp = speeds; cpp < &speeds[NSPEEDS]; cpp++)
	    if (strcmp(*cpp, speed) == 0) {
		tp->sg_ispeed = tp->sg_ospeed = cpp - speeds;
		break;
	    }
    }
    tp->sg_flags = ECHO | CRMOD | ANYP | XTABS;
}

/*
 * Set the value of var to be arg in the Unix 4.2 BSD environment env.
 * Var should end with '='.
 * (bindings are of the form "var=value")
 * This procedure assumes the memory for the first level of environ
 * was allocated using malloc.
 */
static void
setenv(char *var, char *value, int clobber)
{
    extern char **environ;
    int index = 0;
    int varlen = strlen(var);
    int vallen = strlen(value);

    for (index = 0; environ[index] != NULL; index++) {
	if (strncmp(environ[index], var, varlen) == 0) {
	    /* found it */
	    if (!clobber)
		return;
	    environ[index] = malloc(varlen + vallen + 1);
	    strcpy(environ[index], var);
	    strcat(environ[index], value);
	    return;
	}
    }
    environ = (char **)realloc(environ, sizeof(char *) * (index + 2));
    if (environ == NULL) {
	fprintf(stderr, "login: malloc out of memory\n");
	exit(1);
    }
    environ[index] = malloc(varlen + vallen + 1);
    strcpy(environ[index], var);
    strcat(environ[index], value);
    environ[++index] = NULL;
}

static gid_t
tty_gid(int default_gid)
{
    struct group *getgrnam(), *gr;
    gid_t gid = default_gid;

    gr = getgrnam(TTYGRPNAME);
    if (gr != NULL)
	gid = gr->gr_gid;

    endgrent();

    return (gid);
}
#else

#include "AFS_component_version_number.c"

main()
{
}
#endif /*!defined(AFS_SUN_ENV) && !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) */
