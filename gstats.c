#include "q_shared.h"
#include "gbucket.h"
#include "hashtable.h"
#include "darray.h"
#include "md5.h"
#include "net_compat.h"
#include "nonport.h"

void	*NewGame (int mode);
void	NewPlayer (void *gamep, int index, char *name);
void	NewTeam (void *gamep, int index, char *name);
void	RemovePlayer (void *gamep, int index);
void	RemoveTeam (void *gamep, int teamnum);
int		GetPlayerIndex (void *gamep, int index);
int		GetTeamIndex (void *gamep, int index);
int		SendGameSnapShot (void *game, char *gamedata, int done);
void	FreeGame (void *game);
int		InitStatsConnection (int port);
void	CloseStatsConnection (void);
qboolean	IsStatsConnected (void);
char	*GetChallenge (void *game);
char	*GenerateAuth (char *cdkey, char *challenge, char *outbuf);

#define STATS_HOST		"gamestats.gamespy.com"
#define STATS_PORT		29920

#define DISK_MAGIC			0x70F33A5F

typedef struct
{
	int			connid;
	int			sesskey;
	int			mode;
	bucketset_t	*serverbucket;
	char		challenge[12];
	array_t		*players;
	array_t		*teams;
	int			nextplayerslot;
	int			nextteamslot;

	unsigned long	starttime;
} statsgame_t;

static int	ServerOpInt (void *gamep, char *key, bucketop_t op, int value, int owner);
static double	ServerOpFloat (void *gamep, char *key, bucketop_t op, double value, int owner);
static char	*ServerOpString (void *gamep, char *key, bucketop_t op, char *value, int owner);
static int	TeamOpInt (void *gamep, char *key, bucketop_t op, int value, int team);
static double	TeamOpFloat (void *gamep, char *key, bucketop_t op, double value, int team);
static char	*TeamOpString (void *gamep, char *key, bucketop_t op, char *value, int team);
static int	PlayerOpInt (void *gamep, char *key, bucketop_t op, int value, int player);
static double	PlayerOpFloat (void *gamep, char *key, bucketop_t op, double value, int player);
static char	*PlayerOpString (void *gamep, char *key, bucketop_t op, char *value, int player);
static char	*CreateBucketSnapShot (bucketset_t *set);
static void	InternalInit (void);
static int	SendChallengeResponse (char *greeting, int port);
static int	DoSend (char *buf, int len);
static void	CheckDiskFile (void);
static void	DiskWrite (char *payload, int len);
static void	xcode_buf (char *buf, int len);
static unsigned long g_crc32 (unsigned char *buf, int len);
static void	create_challenge (int seed, char *outbuf);
static char	*value_for_key (char *s, char *key);
static int	get_sockaddrin (char *host, int port, struct sockaddr_in *addr,
	void *out);

char	gcd_gamename[256] = "";
char	gcd_secret_key[256] = "";

static statsgame_t	*g_statsgame;
static int			connid;
static int			sesskey;
static int			sock = -1;

static char	enc1[16] = "\0ameSpy3D";
static char	enc2[16] = "\0ndustries";
static char	enc3[16] = "\0rojectAphex";
static char	statsfile[16] = "\0stats.dat";
static char	finalstr[10] = "\0final\\";

static char			*enc = enc1;

static qboolean		internal_init = false;

bucketop_t	bucketfuncs[NUMBUCKETOPS] =
{
	BucketSet,
	BucketAdd,
	BucketSub,
	BucketMult,
	BucketDiv,
	BucketConcat,
	BucketAvg
};

bucketopfn_t	bopfuncs[NUMBUCKETOPFNS] =
{
	(bucketopfn_t)ServerOpInt,
	(bucketopfn_t)ServerOpFloat,
	(bucketopfn_t)ServerOpString,
	(bucketopfn_t)TeamOpInt,
	(bucketopfn_t)TeamOpFloat,
	(bucketopfn_t)TeamOpString,
	(bucketopfn_t)PlayerOpInt,
	(bucketopfn_t)PlayerOpFloat,
	(bucketopfn_t)PlayerOpString
};

/* gamex86.dll 0x2001b990-0x2001ba28 (manual-confirmed) */
/* gamei386.so 0x00056550-0x00056605 */
char *GenerateAuth (char *cdkey, char *challenge, char *outbuf)
{
	char	buf[128];

	if (strlen (challenge) + strlen (cdkey) + 20 >= 128)
	{
		strcpy (outbuf, "CD Key or challenge too long");
		return outbuf;
	}

	sprintf (buf, "%s%s", challenge, cdkey);
	MD5Digest ((unsigned char *)buf, strlen (buf), outbuf);
	return outbuf;
}

/* gamex86.dll 0x2001ba30-0x2001bb50 (padded) */
/* gamei386.so 0x00056608-0x00056736 */
int InitStatsConnection (int port)
{
	struct sockaddr_in	addr;

	if (!internal_init)
		InternalInit ();

	SocketStartUp ();
	sesskey = current_time ();

	if (sock != -1)
		CloseStatsConnection ();

	if (!get_sockaddrin (STATS_HOST, STATS_PORT, &addr, NULL))
		return 2;

	sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return 1;

	if (connect (sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		close (sock);
		sock = -1;
		return 3;
	}

	{
		char	greeting[64];
		int		n, result;

		n = recv (sock, greeting, 64, 0);
		if (n <= 0)
		{
			close (sock);
			sock = -1;
			return 5;
		}
		greeting[n] = 0;
		enc = enc1;
		xcode_buf (greeting, n);

		result = SendChallengeResponse (greeting, port);
		if (result)
			return result;

		CheckDiskFile ();
		return 0;
	}
}

/* gamex86.dll 0x2001bb50-0x2001bb70 (shape-matched(ratio=0.80)) */
/* gamei386.so 0x00056738-0x00056756 */
void CloseStatsConnection (void)
{
	if (sock != -1)
		close (sock);

	sock = -1;
}

/* gamex86.dll 0x2001bb70-0x2001bb80 (aligned) */
/* gamei386.so 0x00056758-0x00056768 */
qboolean IsStatsConnected (void)
{
	return sock != -1;
}

/* gamex86.dll 0x2001bb80-0x2001bba0 (padded) */
/* gamei386.so 0x00056768-0x00056786 */
char *GetChallenge (void *gamep)
{
	statsgame_t	*game = (statsgame_t *)gamep;

	if (!game)
		game = g_statsgame;
	if (!game)
		return "NULLGAME";

	return game->challenge;
}

/* gamex86.dll 0x2001bba0-0x2001bd40 (call-propagated-reverse) */
/* gamei386.so 0x00056788-0x00056934 */
void *NewGame (int mode)
{
	statsgame_t	*g;
	char		request[256];

	g = malloc (sizeof(statsgame_t));

	if (!internal_init)
		InternalInit ();

	g->connid = connid;
	g->sesskey = sesskey++;
	g->serverbucket = NULL;
	g->players = NULL;
	g->teams = NULL;
	g->mode = mode;

	if (sock != -1)
	{
		char	livefmt[31];

		memcpy (livefmt, "\x0c\x1c\x0a\x1d\x02\x02\x19$,4\x06\x17>\x1c\x06\x0e\x39\x46\x10\x1d\x03\x0d\x16\x0b;\x17\x16\x36@\x07",
			sizeof(livefmt));
		enc = enc3;
		xcode_buf (livefmt, sizeof(livefmt) - 1);

		if (DoSend (request, sprintf (request, livefmt, g->connid, g->sesskey)) <= 0)
		{
			close (sock);
			sock = -1;
		}

		create_challenge (g->connid ^ 0x38f371e6, g->challenge);
	}

	if (sock == -1)
	{
		char	diskfmt[35];

		memcpy (diskfmt, "\x0c\x1c\x0a\x1d\x02\x02\x19$,4\x16\x1d#\x01\x04\x0f\x1c\x3fQ%,4\x06\x10\x31\x1e\x03\x0f\x0b\x04\x11\x1dU\x0c",
			sizeof(diskfmt));
		enc = enc3;
		xcode_buf (diskfmt, sizeof(diskfmt) - 1);

		DiskWrite (request, sprintf (request, diskfmt, g->sesskey,
			g->sesskey ^ 0x38f371e6));

		g->connid = 0;
		create_challenge (g->sesskey ^ 0x38f371e6, g->challenge);
	}

	if (g)
	{
		if (g->mode)
		{
			g->serverbucket = NewBucketSet ();
			g->players = ArrayNew (sizeof(void *), 32, NULL);
			g->teams = ArrayNew (sizeof(void *), 2, NULL);
			g->nextteamslot = 0;
			g->nextplayerslot = 0;
		}

		g->starttime = current_time ();
	}

	g_statsgame = g;

	return g;
}

/* gamex86.dll 0x2001bd40-0x2001bda0 (call-propagated) */
/* gamei386.so 0x00056934-0x00056992 */
void FreeGame (void *gamep)
{
	statsgame_t	*g = (statsgame_t *)gamep;

	if (!g)
	{
		g = g_statsgame;
		g_statsgame = NULL;
		if (!g)
			return;
	}

	if (g->mode)
	{
		if (g->serverbucket)
			FreeBucketSet (g->serverbucket);
		if (g->players)
			ArrayFree (g->players);
		if (g->teams)
			ArrayFree (g->teams);
	}

	free (g);
}

/* gamex86.dll 0x2001bda0-0x2001befe (call-propagated+corrected) */
/* gamei386.so 0x00056994-0x00056b90 */
int SendGameSnapShot (void *gamep, char *gamedata, int done)
{
	char		*dump;
	char		*payload;
	int			len;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return 5;

	if (((statsgame_t *)gamep)->mode)
		dump = CreateBucketSnapShot (((statsgame_t *)gamep)->serverbucket);
	else
		dump = _strdup (gamedata);

	len = strlen (dump);
	payload = malloc (len + 256);

	while (len--)
		if (dump[len] == '\\')
			dump[len] = 1;

	if (sock != -1)
	{
		char	livefmt[41];

		memcpy (livefmt, "\x0c\x07\x1f\x0e\x02\x02\x19$,4\x16\x1d#\x01\x04\x0f\x1c\x3fQ%,\x0c\x0a\x16\x35.J\x0e\x39\x04\x15,\x15\x0c\x04\x0c\x31.J\x19",
			sizeof(livefmt));
		enc = enc3;
		xcode_buf (livefmt, sizeof(livefmt) - 1);

		len = DoSend (payload, sprintf (payload, livefmt, ((statsgame_t *)gamep)->sesskey, done, dump));
		if (len <= 0)
		{
			close (sock);
			sock = -1;
		}
	}

	if (sock == -1)
	{
		char	diskfmt[56];
		int		n;

		memcpy (diskfmt, "\x0c\x07\x1f\x0e\x02\x02\x19$,4\x16\x1d#\x01\x04\x0f\x1c\x3fQ%,\x0b\x0a\x16>\x1b\x0b\x36@\x07(%\x1f\x06\x00$u\x16\x33\x0d\x04\x0e\x11%\x11\x1c\x04$u\x01\x33\x0e\x09\x3f\x45",
			sizeof(diskfmt));
		enc = enc3;
		xcode_buf (diskfmt, sizeof(diskfmt) - 1);

		n = sprintf (payload, diskfmt, ((statsgame_t *)gamep)->sesskey, ((statsgame_t *)gamep)->connid,
			done, dump);
		DiskWrite (payload, n);
	}

	free (dump);
	free (payload);

	return 0;
}

/* gamex86.dll 0x2001bf00-0x2001bfbe (shape-matched) */
/* gamei386.so 0x00056b90-0x00056c3d */
void NewPlayer (void *gamep, int index, char *name)
{
	int			slot = -1;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return;

	while (index >= ArrayLength (((statsgame_t *)gamep)->players))
		ArrayAppend (((statsgame_t *)gamep)->players, &slot);

	slot = ((statsgame_t *)gamep)->nextplayerslot;
	((statsgame_t *)gamep)->nextplayerslot++;
	ArrayReplaceAt (((statsgame_t *)gamep)->players, &slot, index);

	bopfuncs[BOP_PLAYER_INT] (gamep, "ctime", bucketfuncs[BUCKET_SET],
		(current_time () - ((statsgame_t *)gamep)->starttime) / 1000, index);
	((bucketopstrfn_t)bopfuncs[BOP_PLAYER_STRING]) (gamep, "player",
		bucketfuncs[BUCKET_SET], name, index);
}

/* gamex86.dll 0x2001bfc0-0x2001c010 (call-propagated) */
/* gamei386.so 0x00056c40-0x00056c82 */
void RemovePlayer (void *gamep, int index)
{
	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return;

	bopfuncs[BOP_PLAYER_INT] (gamep, "dtime", bucketfuncs[BUCKET_SET],
		(current_time () - ((statsgame_t *)gamep)->starttime) / 1000, index);
}

/* gamex86.dll 0x2001c010-0x2001c0ce (manual-confirmed) */
/* gamei386.so 0x00056c84-0x00056d31 */
void NewTeam (void *gamep, int index, char *name)
{
	int			slot = -1;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return;

	while (index >= ArrayLength (((statsgame_t *)gamep)->teams))
		ArrayAppend (((statsgame_t *)gamep)->teams, &slot);

	slot = ((statsgame_t *)gamep)->nextteamslot;
	((statsgame_t *)gamep)->nextteamslot++;
	ArrayReplaceAt (((statsgame_t *)gamep)->teams, &slot, index);

	bopfuncs[BOP_TEAM_INT] (gamep, "ctime", bucketfuncs[BUCKET_SET],
		(current_time () - ((statsgame_t *)gamep)->starttime) / 1000, index);
	((bucketopstrfn_t)bopfuncs[BOP_TEAM_STRING]) (gamep, "team", bucketfuncs[BUCKET_SET],
		name, index);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00056d34-0x00056d76 */
void RemoveTeam (void *gamep, int teamnum)
{
	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return;

	bopfuncs[BOP_TEAM_INT] (gamep, "dtime", bucketfuncs[BUCKET_SET],
		(current_time () - ((statsgame_t *)gamep)->starttime) / 1000, teamnum);
}

/* gamex86.dll 0x2001c0d0-0x2001c100 (manual-confirmed) */
/* gamei386.so 0x00056d78-0x00056da6 */
static void InternalInit (void)
{
	internal_init = true;
	enc1[0] = 'G';
	enc2[0] = 'I';
	enc3[0] = 'P';
	statsfile[0] = 'g';
	finalstr[0] = '\\';
}

/* gamex86.dll 0x2001c100-0x2001c280 (manual-confirmed) */
/* gamei386.so 0x00056da8-0x00056f0a */
static int SendChallengeResponse (char *greeting, int port)
{
	static char	challengestr[] = {'\0','h','a','l','l','e','n','g','e','\0'};
	static char	sesskeystr[] = {'\0','e','s','s','k','e','y','\0'};

	char	request[128];
	char	auth[36];
	char	fmt[44];
	char	*challenge;
	char	*sesskeyval;
	int		len;

	memcpy (fmt, "\x0c\x13\x1a\x1e\x0d\x3f(&\x11\x05\x00\x16\x31\x1f\x0a\x36@\x10(3\x15\x1b\x15\x17>\x01\x0a\x36@\x10(1\x1f\x1a\x11$u\x16\x33\x03\x01\x3f\x45",
		sizeof(fmt));

	challengestr[0] = 'c';
	challenge = value_for_key (greeting, challengestr);
	if (!challenge)
	{
		close (sock);
		return 5;
	}

	len = sprintf (request, "%d%s",
		(int)g_crc32 ((unsigned char *)challenge, strlen (challenge)), gcd_secret_key);
	MD5Digest ((unsigned char *)request, len, auth);

	enc = enc3;
	xcode_buf (fmt, sizeof(fmt) - 1);

	if (DoSend (request, sprintf (request, fmt, gcd_gamename, auth, port)) <= 0)
	{
		close (sock);
		return 3;
	}

	len = recv (sock, request, 128, 0);
	if (len <= 0)
	{
		close (sock);
		return 3;
	}
	request[len] = 0;
	enc = enc1;
	xcode_buf (request, len);

	sesskeystr[0] = 's';
	sesskeyval = value_for_key (request, sesskeystr);
	if (!sesskeyval)
	{
		close (sock);
		return 5;
	}

	connid = atoi (sesskeyval);

	return 0;
}

/* gamex86.dll 0x2001c280-0x2001c2e0 (manual-confirmed) */
/* gamei386.so 0x00056f0c-0x00056f4d */
static int DoSend (char *buf, int len)
{
	enc = enc1;
	xcode_buf (buf, len);

	strcpy (buf + len, finalstr);

	return send (sock, buf, len + 7, 0);
}

/* gamex86.dll 0x2001c2e0-0x2001c4d0 (manual-confirmed) */
/* gamei386.so 0x00056f50-0x00057125 */
static void CheckDiskFile (void)
{
	int				len;
	char			mode[3];
	char			*batch;
	FILE			*f;
	unsigned long	crc;
	int				filelen, used;
	char			*payload;

	mode[0] = 'r';
	mode[1] = 'b';
	mode[2] = 0;

	f = fopen (statsfile, mode);
	if (!f)
		return;

	fseek (f, 0, SEEK_END);
	filelen = ftell (f);
	fseek (f, 0, SEEK_SET);

	batch = malloc (filelen + 2);
	batch[0] = 0;
	used = 0;

	while (!feof (f) && !ferror (f))
	{
		if (!fread (&crc, sizeof(crc), 1, f))
			break;

		if (!fread (&len, sizeof(len), 1, f))
			break;

		len ^= DISK_MAGIC;
		payload = malloc (len + 1);

		if (fread (payload, 1, len, f) != len)
			break;
		payload[len] = 0;

		enc = enc2;
		xcode_buf (payload, len);

		if (crc != g_crc32 ((unsigned char *)payload, len))
		{
			free (payload);
			break;
		}

		enc = enc1;
		xcode_buf (payload, len);

		memcpy (batch + used, payload, len);
		used += len;
		memcpy (batch + used, finalstr, 7);
		used += 7;

		free (payload);
	}

	fclose (f);

	len = send (sock, batch, used, 0);
	if (len <= 0)
	{
		close (sock);
		sock = -1;
	}
	else
		remove (statsfile);
}

/* gamex86.dll 0x2001c4d0-0x2001c568 (manual-confirmed) */
/* gamei386.so 0x00057128-0x000571c0 */
static void DiskWrite (char *payload, int len)
{
	FILE			*f;
	int				enclen;
	unsigned long	crc;
	char			mode[3];

	mode[0] = 'a';
	mode[1] = 'b';
	mode[2] = 0;

	f = fopen (statsfile, mode);
	if (!f)
		return;

	crc = g_crc32 ((unsigned char *)payload, len);
	fwrite (&crc, sizeof(crc), 1, f);

	enclen = len ^ DISK_MAGIC;
	fwrite (&enclen, sizeof(enclen), 1, f);

	enc = enc2;
	xcode_buf (payload, len);
	fwrite (payload, 1, len, f);

	fclose (f);
}

/* gamex86.dll 0x2001c570-0x2001c5a4 (manual-confirmed) */
/* gamei386.so 0x000571c0-0x00057283 */
static void xcode_buf (char *buf, int len)
{
	char	*key;
	int		i;

	key = enc;
	for (i = 0; i < len; i++)
	{
		buf[i] ^= *key;
		key++;
		if (!*key)
			key = enc;
	}
}

/* gamex86.dll 0x2001c5b0-0x2001c5d8 (shape-matched) */
/* gamei386.so 0x00057284-0x0005730d */
static unsigned long g_crc32 (unsigned char *buf, int len)
{
	unsigned long	hash;
	int				n;

	hash = 0;
	for (n = 0; n < len; n++)
		hash = hash * 0x9CCF9319UL + (long)(signed char)buf[n];

	return hash;
}

/* gamex86.dll 0x2001c5e0-0x2001c612 (manual-confirmed) */
/* gamei386.so 0x00057310-0x00057348 */
static void create_challenge (int seed, char *outbuf)
{
	char		*p;

	p = outbuf;
	sprintf (p, "%.8x", (unsigned int)seed);

	for ( ; *p; p++)
		*p = *p + 0x11 + (p - outbuf);
}

/* gamex86.dll 0x2001c620-0x2001c720 (manual-confirmed) */
/* gamei386.so 0x00057348-0x00057406 */
static char *value_for_key (char *s, char *key)
{
	static int	valueindex;
	static char	value[2][256];
	char		search[256] = "\\";
	char		*p;
	char		*o;

	valueindex ^= 1;

	strcat (search, key);
	strcat (search, "\\");

	p = strstr (s, search);
	if (!p)
		return NULL;

	p += strlen (search);

	o = value[valueindex];
	while (*p && *p != '\\')
		*o++ = *p++;
	*o = 0;

	return value[valueindex];
}

/* gamex86.dll 0x2001c720-0x2001c7d0 (manual-confirmed) */
/* gamei386.so 0x00057408-0x000574c8 */
static int get_sockaddrin (char *host, int port, struct sockaddr_in *addr, void *out)
{
	void		*result;

	memset (addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons ((unsigned short)port);

	if (!host)
		addr->sin_addr.s_addr = 0;
	else
		addr->sin_addr.s_addr = inet_addr (host);

	if (addr->sin_addr.s_addr == INADDR_NONE && strcmp (host, "255.255.255.255"))
	{
		result = gethostbyname (host);
		if (!result)
			return 0;

		memcpy (&addr->sin_addr, ((struct hostent *)result)->h_addr_list[0],
			sizeof(addr->sin_addr));
	}

	if (out)
		*(void **)out = result;

	return 1;
}

/* gamex86.dll 0x2001c7d0-0x2001c800 (manual-confirmed) */
/* gamei386.so 0x000574c8-0x000574f1 */
int GetTeamIndex (void *gamep, int index)
{
	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return index;

	return *(int *)ArrayNth (((statsgame_t *)gamep)->teams, index);
}

/* gamex86.dll 0x2001c800-0x2001c830 (manual-confirmed) */
/* gamei386.so 0x000574f4-0x0005751d */
int GetPlayerIndex (void *gamep, int index)
{
	if (gamep == NULL)
		gamep = g_statsgame;
	if (gamep == NULL)
		return index;

	return *(int *)ArrayNth (((statsgame_t *)gamep)->players, index);
}

/* gamex86.dll 0x2001c830-0x2001c880 (manual-confirmed) */
/* gamei386.so 0x00057520-0x0005756d */
static int ServerOpInt (void *gamep, char *key, bucketop_t op, int value, int owner)
{
	void		*b;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (!gamep)
		b = &value;
	else
	{
		b = op (((statsgame_t *)gamep)->serverbucket, key, &value);
		if (!b)
			b = BucketNew (((statsgame_t *)gamep)->serverbucket, key, bt_int, &value);
	}

	return *(int *)b;
}

/* gamex86.dll 0x2001c880-0x2001c8d0 (manual-confirmed) */
/* gamei386.so 0x00057570-0x000575bd */
static double ServerOpFloat (void *gamep, char *key, bucketop_t op, double value, int owner)
{
	void		*b;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (!gamep)
		b = &value;
	else
	{
		b = op (((statsgame_t *)gamep)->serverbucket, key, &value);
		if (!b)
			b = BucketNew (((statsgame_t *)gamep)->serverbucket, key, bt_float, &value);
	}

	return *(double *)b;
}

/* gamex86.dll 0x2001c8d0-0x2001c920 (manual-confirmed) */
/* gamei386.so 0x000575c0-0x0005760b */
static char *ServerOpString (void *gamep, char *key, bucketop_t op, char *value, int owner)
{
	char		*b;

	if (gamep == NULL)
		gamep = g_statsgame;
	if (!gamep)
		b = value;
	else
	{
		b = op (((statsgame_t *)gamep)->serverbucket, key, value);
		if (!b)
			b = BucketNew (((statsgame_t *)gamep)->serverbucket, key, bt_string, value);
	}

	return b;
}

/* gamex86.dll 0x2001c920-0x2001c970 (manual-confirmed) */
/* gamei386.so 0x0005760c-0x000576a9 */
static int TeamOpInt (void *gamep, char *key, bucketop_t op, int value, int team)
{
	char	composite[64];

	sprintf (composite, "%s_t%d", key, GetTeamIndex (gamep, team));

	return ServerOpInt (gamep, composite, op, value, team);
}

/* gamex86.dll 0x2001c970-0x2001c9c0 (manual-confirmed) */
/* gamei386.so 0x000576ac-0x0005774d */
static double TeamOpFloat (void *gamep, char *key, bucketop_t op, double value, int team)
{
	char	composite[64];

	sprintf (composite, "%s_t%d", key, GetTeamIndex (gamep, team));

	return ServerOpFloat (gamep, composite, op, value, team);
}

/* gamex86.dll 0x2001c9c0-0x2001ca10 (manual-confirmed) */
/* gamei386.so 0x00057750-0x000577e3 */
static char *TeamOpString (void *gamep, char *key, bucketop_t op, char *value, int team)
{
	char	composite[64];

	sprintf (composite, "%s_t%d", key, GetTeamIndex (gamep, team));

	return ServerOpString (gamep, composite, op, value, team);
}

/* gamex86.dll 0x2001ca10-0x2001ca60 (manual-confirmed) */
/* gamei386.so 0x000577e4-0x00057881 */
static int PlayerOpInt (void *gamep, char *key, bucketop_t op, int value, int player)
{
	char	composite[64];

	sprintf (composite, "%s_%d", key, GetPlayerIndex (gamep, player));

	return ServerOpInt (gamep, composite, op, value, player);
}

/* gamex86.dll 0x2001ca60-0x2001cab0 (manual-confirmed) */
/* gamei386.so 0x00057884-0x00057925 */
static double PlayerOpFloat (void *gamep, char *key, bucketop_t op, double value, int player)
{
	char	composite[64];

	sprintf (composite, "%s_%d", key, GetPlayerIndex (gamep, player));

	return ServerOpFloat (gamep, composite, op, value, player);
}

/* gamex86.dll 0x2001cab0-0x2001caf8 (manual-confirmed) */
/* gamei386.so 0x00057928-0x000579bb */
static char *PlayerOpString (void *gamep, char *key, bucketop_t op, char *value, int player)
{
	char	composite[64];

	sprintf (composite, "%s_%d", key, GetPlayerIndex (gamep, player));

	return ServerOpString (gamep, composite, op, value, player);
}

/* gamex86.dll 0x2001cb00-0x2001cb0e (manual-confirmed) */
/* gamei386.so 0x000579bc-0x000579ca */
static char *CreateBucketSnapShot (bucketset_t *set)
{
	return DumpBucketSet (set);
}
