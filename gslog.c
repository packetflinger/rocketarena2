#include "g_local.h"
#include "net_compat.h"


extern cvar_t	*logfile;
extern cvar_t	*netlog;

FILE		*StdLogFile;

static fd_set	global_fds;


/* gamex86.dll 0x2001b120-0x2001b230 (shape-matched(ratio=0.59)+size-corrected) */
/* gamei386.so 0x00054144-0x00054248 */
struct sockaddr_in net_name_to_address (char *name)
{
	struct sockaddr_in	sin;
	struct hostent		*hp;
	char			*s, *portstr;
	int			port;
	unsigned long		a;

	memset (&sin, 0, sizeof(sin));

	s = strdup (name);
	strtok (s, ":");
	portstr = strtok (NULL, "");

	if (portstr)
	{
		port = atoi (portstr);
		if (port <= 0 || port >= 65536)
		{
			fprintf (stderr, "net_name_to_address: %s: invalid port number\n", portstr);
			exit (1);
		}
		sin.sin_port = port;
	}
	else
		sin.sin_port = 0;

	a = inet_addr (s);
	if (a == INADDR_NONE)
	{
		hp = gethostbyname (s);
		if (hp)
			memcpy (&sin.sin_addr, hp->h_addr_list[0], sizeof(sin.sin_addr));
		else
		{
#ifdef _WIN32
			fprintf (stderr, "%s: %d", s, WSAGetLastError ());
#else
			fprintf (stderr, "%s: %s", s, "net_name_to_addr");
#endif
			exit (1);
		}
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons (sin.sin_port);
	free (s);

	return sin;
}

/* gamex86.dll 0x2001b230-0x2001b270 (manual-confirmed) */
/* gamei386.so 0x00054248-0x00054286 */
void net_send (int sock, char *buf, int len)
{
	int	r;

	r = send (sock, buf, len, 0);
	if (r != len)
	{
		perror ("send");
		if (errno)
			exit (1);
	}
}

/* gamex86.dll 0x2001b270-0x2001b2c0 (manual-confirmed) */
/* gamei386.so 0x00054288-0x000542c5 */
int net_open_socket (void)
{
	int	sock;

	sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
#ifdef _WIN32
		printf ("WSA %d\n", WSAGetLastError ());
#else
		perror ("socket");
#endif
		exit (1);
	}

	FD_SET (sock, &global_fds);

	return sock;
}

/* gamex86.dll 0x2001b2c0-0x2001b340 (padded) */
/* gamei386.so 0x000542c8-0x00054304 */
void net_close_socket (int sock)
{
	if (sock)
	{
		if (close (sock) < 0)
		{
			perror ("close");
			exit (1);
		}
	}

	FD_CLR (sock, &global_fds);
}

/* gamex86.dll 0x2001b340-0x2001b380 (padded) */
/* gamei386.so 0x00054304-0x00054343 */
void net_connect_socket (int sock, struct sockaddr_in *addr, unsigned short port)
{
	addr->sin_port = htons (port);

	if (connect (sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
	{
		perror ("connect");
		exit (1);
	}
}

/* gamex86.dll 0x2001b380-0x2001b400 (bracketed) */
/* gamei386.so 0x00054344-0x0005445a */
void GSSendLine (char *line)
{
	struct sockaddr_in	addr;
	int			sock;
	unsigned short		port;

	addr = net_name_to_address (netlog->string);
	port = ntohs (addr.sin_port);

	sock = net_open_socket ();
	net_connect_socket (sock, &addr, port);
	net_send (sock, line, strlen (line) + 1);
	net_close_socket (sock);
}

/* gamex86.dll 0x2001b400-0x2001b4d0 (padded+majority+collision-resolved) */
/* gamei386.so 0x0005445c-0x000544ca */
void GSOpenLog (void)
{
	cvar_t	*gamedir, *logname;
	char	path[80];

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
#ifdef _WIN32
	strcat (path, "\\");
#else
	strcat (path, "/");
#endif
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");
}

/* gamex86.dll 0x2001b4d0-0x2001b4e0 (bracketed) */
/* gamei386.so 0x000544cc-0x000544db */
void GSCloseLog (void)
{
	fclose (StdLogFile);
}

/* gamex86.dll 0x2001b4e0-0x2001b520 (bracketed) */
/* gamei386.so 0x000544dc-0x000545ab */
void GSLogShutdown (void)
{
	if (logfile->value != 2)
		return;

	GSOpenLog ();

	fprintf (StdLogFile, "\t\tGameEnd\t\t\t%d\n", (int)level.time);

	GSCloseLog ();
}

/* gamex86.dll 0x2001b520-0x2001b570 (bracketed) */
/* gamei386.so 0x000545ac-0x00054669 */
void GSLogStartup (void)
{
	if (logfile->value != 2)
		return;

	GSOpenLog ();

	fprintf (StdLogFile, "\t\tStdLog\t1.22\n");
	fprintf (StdLogFile, "\t\tPatchName\tRocket Arena 2 %s\n", "v2.25");

	GSCloseLog ();
}

/* gamex86.dll 0x2001b570-0x2001b5c0 (bracketed) */
/* gamei386.so 0x0005466c-0x0005474d */
void GSLogNewmap (void)
{
	if (logfile->value != 2)
		return;

	GSOpenLog ();

	fprintf (StdLogFile, "\t\tMAP\t%s\n", level.level_name);
	fprintf (StdLogFile, "\t\tGameStart\t\t\t%d\n", (int)level.time);

	GSCloseLog ();
}

/* gamex86.dll 0x2001b5c0-0x2001b5f0 (bracketed) */
/* gamei386.so 0x00054750-0x0005477c */
void GSdodeathlog (char *line)
{
	fprintf (StdLogFile, "%s", line);

	if (netlog->string[0])
		GSSendLine (line);
}

/* gamex86.dll 0x2001b5f0-0x2001b890 (padded+size) */
/* gamei386.so 0x0005477c-0x000549d8 */
void GSLogDeath (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	char	line[1000];
	gitem_t	*weap;
	char	*weapname;

	if (logfile->value != 2)
		return;

	GSOpenLog ();

	if (attacker == self)
	{
		if (attacker->client->pers.weapon)
		{
			if (!strcmp (self->client->pers.weapon->classname, "weapon_grenadelauncher") ||
			    !strcmp (self->client->pers.weapon->classname, "weapon_rocketlauncher") ||
			    !strcmp (self->client->pers.weapon->classname, "weapon_bfg"))
			{
				Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t%s\t-1\t%d\t%d\n",
					self->client->pers.netname, self->client->pers.weapon->pickup_name,
					(int)level.time, self->client->ping);
				GSdodeathlog (line);
			}
			else
			{
				Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
					self->client->pers.netname, (int)level.time, self->client->ping);
				GSdodeathlog (line);
			}
		}
		else
		{
			Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
				self->client->pers.netname, (int)level.time, self->client->ping);
			GSdodeathlog (line);
		}

		GSCloseLog ();
		return;
	}

	if (attacker && attacker->client)
	{
		weap = attacker->client->pers.weapon;
		weapname = weap ? weap->pickup_name : "BFG10K";

		Com_sprintf (line, sizeof(line), "%s\t%s\tKill\t%s\t1\t%d\t%d\n",
			attacker->client->pers.netname, self->client->pers.netname,
			weapname, (int)level.time, attacker->client->ping);
		GSdodeathlog (line);
		GSCloseLog ();
		return;
	}

	Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
		self->client->pers.netname, (int)level.time, self->client->ping);
	GSdodeathlog (line);
	GSCloseLog ();
}

/* gamex86.dll 0x2001b890-0x2001b8e0 (call-propagated) */
/* gamei386.so 0x000549d8-0x00054ab6 */
void GSLogEnter (edict_t *ent)
{
	if (logfile->value != 2)
		return;

	GSOpenLog ();

	fprintf (StdLogFile, "\t\tPlayerConnect\t%s\t\t%d\n",
		ent->client->pers.netname, (int)level.time);

	GSCloseLog ();
}

/* gamex86.dll 0x2001b8e0-0x2001b930 (call-propagated) */
/* gamei386.so 0x00054ab8-0x00054b96 */
void GSLogExit (edict_t *ent)
{
	if (logfile->value != 2)
		return;

	GSOpenLog ();

	fprintf (StdLogFile, "\t\tPlayerLeft\t%s\t\t%d\n",
		ent->client->pers.netname, (int)level.time);

	GSCloseLog ();
}

#ifdef _WIN32

/* gamex86.dll 0x2001b930-0x2001b984 (manual-confirmed) */
/* gamei386.so: no symbol -- not compiled into the Unix build */
int NetShutdown (int mode)
{
	WSADATA	wsaData;

	if (mode == 1)
	{
		if (WSAStartup (MAKEWORD (1, 1), &wsaData) != 0)
		{
			gi.dprintf ("WS Error: %d\n", WSAGetLastError ());
			return 0;
		}
	}
	else if (mode == 0)
	{
		WSACleanup ();
	}

	return 1;
}

#endif
