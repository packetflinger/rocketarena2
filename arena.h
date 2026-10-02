#define	MAX_ARENAS			32
#define	MAX_TEAMS			256
#define	MAX_ARENA_SKINS		7

#define	ASTATE_WARMUP			0
#define	ASTATE_COUNTDOWN		1
#define	ASTATE_FIGHTING			2
#define	ASTATE_ROUNDEND			3
#define	ASTATE_INTERMISSION		4
#define	ASTATE_RESULTS			5
#define	ASTATE_NEXTROUND		6

#define	FIGHT_SPECTATING		0
#define	FIGHT_ALIVE				1
#define	FIGHT_DEAD				2

#define	MOD_GRAPPLE				34

#define	STAT_LINEPOSITION		19

#define	STAT_CTF_ID_VIEW		20

#define	STAT_ROUNDTIME			26

// per-client configstring holding the round time remaining
#define	CS_ROUNDTIME			(game.num_items + 0x424)

#define	MAX_STATUS_TEAMS		2
#define	MAX_STATUS_MEMBERS		4

typedef enum
{
	CTF_GRAPPLE_STATE_FLY,
	CTF_GRAPPLE_STATE_PULL,
	CTF_GRAPPLE_STATE_HANG
} ctfgrapplestate_t;

#define	CTF_GRAPPLE_SPEED		650	// speed of grapple in flight
#define	CTF_GRAPPLE_PULL_SPEED	650	// speed player is pulled at

typedef struct motd_s
{
	char			*line;
	struct motd_s	*next;
	struct motd_s	*prev;
} motd_t;

typedef struct team_s
{
	char		*name;
	int			teamnum;
	int			arenanum;
	int			wins;

	qmenu_t		arenalink;

	qboolean	locked;

	int			side;

	int			skin;

	qboolean	fighting;

	qboolean	outofline;
} team_t;

#define	TEAM(node)	((team_t *)(node)->it)

typedef struct arena_settings_s
{
	int			playersperteam;
	int			rounds;
	int			weapons;
	int			armor;
	int			health;
	int			minping;
	int			maxping;
	int			rocket_speed;
	int			shells, bullets, slugs, grenades, rockets, cells;

	int			startdelay;

	int			fastswitch;
	int			armorprotect;
	int			healthprotect;
	qboolean	fallingdamage;

	qboolean	allow_voting_armor;
	qboolean	allow_voting_health;
	qboolean	allow_voting_minping;
	qboolean	allow_voting_maxping;
	qboolean	allow_voting_playersperteam;
	qboolean	allow_voting_rounds;
	qboolean	allow_voting_maxteams;
	qboolean	allow_voting_armorprotect;
	qboolean	allow_voting_healthprotect;
	qboolean	allow_voting_shotgun;
	qboolean	allow_voting_supershotgun;
	qboolean	allow_voting_machinegun;
	qboolean	allow_voting_chaingun;
	qboolean	allow_voting_grenadelauncher;
	qboolean	allow_voting_rocketlauncher;
	qboolean	allow_voting_hyperblaster;
	qboolean	allow_voting_railgun;
	qboolean	allow_voting_bfg;
	qboolean	allow_voting_fallingdamage;

	qboolean	locked;
	qboolean	competition;
	qboolean	scorebydamage;
	qboolean	changed;
} arena_settings_t;

typedef struct arena_s
{
	int			numteams;
	int			_arena_unidentified0;

	qmenu_t		waitingteams;

	qmenu_t		activeteams;

	int			state;

	qboolean	teamplay;

	int			countdown_next_tick;

	int			countdown;

	qboolean	active;

	char		msg[160];

	char		vs[64];

	int			playersperteam;

	int			rounds;

	int			weapons;

	int			armor;
	int			health;

	int			minping, maxping;

	int			rocket_speed;

	int			shells, bullets, slugs, grenades, rockets, cells;

	int			startdelay;

	int			fastswitch;
	int			armorprotect;
	int			healthprotect;
	qboolean	fallingdamage;

	qboolean	allow_voting_armor;
	qboolean	allow_voting_health;
	qboolean	allow_voting_minping;
	qboolean	allow_voting_maxping;
	qboolean	allow_voting_playersperteam;
	qboolean	allow_voting_rounds;
	qboolean	allow_voting_maxteams;
	qboolean	allow_voting_armorprotect;
	qboolean	allow_voting_healthprotect;
	qboolean	allow_voting_shotgun;
	qboolean	allow_voting_supershotgun;
	qboolean	allow_voting_machinegun;
	qboolean	allow_voting_chaingun;
	qboolean	allow_voting_grenadelauncher;
	qboolean	allow_voting_rocketlauncher;
	qboolean	allow_voting_hyperblaster;
	qboolean	allow_voting_railgun;
	qboolean	allow_voting_bfg;
	qboolean	allow_voting_fallingdamage;

	qboolean	locked;

	qboolean	competition;
	qboolean	scorebydamage;

	qboolean	changed;

	float		proposetime;

	arena_settings_t	proposed;

	int			votetries;
	int			votes_yes, votes_no;
	edict_t		*proposer;

	qboolean	idarena;

	int			sidepick;

	int			maxteams;

	int			round;
	team_t		*pickupteam[2];

	void		*statsptr;

	int			roundtimelimit;		// seconds per round, 0 = no limit
	int			roundstart_framenum;	// when the current fight began
	int			roundtime_sent;			// last whole second sent to clients
	qboolean	timed_out;				// round ended by the time limit
	int			timeout_winner;			// teamnum, or -1 for a tie
} arena_t;

extern	int			votetries_setting;
extern	qboolean	allow_grapple;
extern	qboolean	broken;

extern	arena_t		arenas[MAX_ARENAS];
extern	int			num_arenas;
extern	qboolean	idmap;

extern	qmenu_t		*teams;

extern	motd_t		motd;
extern	cvar_t		*admincode;

extern	char		*teamskins[MAX_ARENA_SKINS];
extern	char		*vwepmodels[4];
extern	qboolean	teamskins_precachem[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachef[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachecw[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachecb[MAX_ARENA_SKINS];

extern	char		*omode_descriptions[4];

extern	char		*dm_statusbar;

extern	int			weapon_vals[9];

int			count_queue (qmenu_t *head);
int			count_players_queue (qmenu_t *head);

void		set_damage (int arenanum, int state);
void		give_ammo (edict_t *ent);

team_t		*add_to_team (edict_t *ent, char *teamname);
void		remove_from_team (edict_t *ent);

edict_t		*SelectRandomArenaSpawnPoint (char *classn, int arenanum, int side);
edict_t		*SelectFarthestArenaSpawnPoint (char *classn, int arenanum);

void		track_SetStats (edict_t *ent);
void		eyecam_think (edict_t *ent, usercmd_t *ucmd);
qboolean	eyecam_active (edict_t *ent);
void		eyecam_SetView (edict_t *ent);
void		track_think (edict_t *ent, usercmd_t *ucmd);
void		track_change (edict_t *ent, int dir);
void		track_next (edict_t *ent);
void		track_prev (edict_t *ent);
void		SetObserverMode (edict_t *ent);

void		move_to_arena (edict_t *ent, int arenanum, int mode);
void		ChangeOMode (edict_t *ent);

int			getfreeskin (int arenanum);
char		*mylcase (char *s);
qboolean	checkvwepmodel (char *s);
void		setteamskin (edict_t *ent, char *userinfo, int skinnum);

void		SendTeamToArena (qmenu_t *team, int arenanum, qboolean observer, qboolean announce);
int			AddtoArena (edict_t *ent, int arenanum, int allow_partial, int skip_checks);
void		check_teams (int arenanum);

void		init_player (edict_t *ent);
void		reinit_player (edict_t *ent);

void		show_stringc (char *s, int context);
void		show_string (int priority, char *s, int context);
void		stuffcmd (edict_t *ent, char *s);
void		send_sound_to_arena (char *soundname, int context);
void		send_configstring (edict_t *e, int index, char *string);
void		show_countdown (int countdown, int arenanum);
void		show_roundtime (int arenanum, qboolean force);
int			round_time_left (int arenanum);
int			health_winner (int arenanum);
int		show_rank (qmenu_t *node);

qboolean	check_for_teams (int arenanum);
int			fill_arena (int arenanum);
int			fight_done (int arenanum);

void		CTFSetIDView (edict_t *ent);
void		UpdateStatusBars (int arenanum);
void		check_telefrag (int arenanum);

void		start_voting (edict_t *proposer, int arenanum);
void		check_voting (int arenanum);

void		arena_think (int arenanum);
void		multi_arena_think (void);
void		arena_init (edict_t *wsent);

void		SP_trigger_teleport (edict_t *ent);
void		SP_func_illusionary (edict_t *ent);
void		SP_info_teleport_destination (edict_t *ent);

char		*getarenaname (int arenanum);
void		menu_centerprint (edict_t *ent, char *message);
int			menuRefreshTeamList (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

void		motd_menu (edict_t *ent);

void		CTFPlayerResetGrapple (edict_t *ent);
void		CTFResetGrapple (edict_t *self);
void		CTFGrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void		CTFGrappleDrawCable (edict_t *self);
void		CTFGrapplePull (edict_t *self);
void		CTFFireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect);
void		CTFGrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect);
void		CTFWeapon_Grapple_Fire (edict_t *ent);
void		CTFWeapon_Grapple (edict_t *ent);

void		GSLogStartup (void);
void		GSLogShutdown (void);
void		GSLogNewmap (void);
void		GSLogEnter (edict_t *ent);
void		GSLogExit (edict_t *ent);
void		GSLogDeath (edict_t *self, edict_t *inflictor, edict_t *attacker);

void		*NewGame (int mode);
void		NewPlayer (void *gamep, int index, char *name);
void		NewTeam (void *gamep, int index, char *name);
void		NewStatsPlayer (void *gamep, edict_t *ent, int team);
void		ValidatePlayer (edict_t *ent, void *gamep);
void		RemovePlayer (void *gamep, int index);
void		RemoveTeam (void *gamep, int teamnum);
int			GetPlayerIndex (void *gamep, int index);
int			GetTeamIndex (void *gamep, int index);
void		set_server_bucket_info (int arenanum);
int			SendGameSnapShot (void *game, char *gamedata, int done);
void		FreeGame (void *game);
int			InitStatsConnection (int port);
void		CloseStatsConnection (void);
#ifdef _WIN32
int			NetShutdown (int mode);
#endif
qboolean	IsStatsConnected (void);
char		*GetChallenge (void *game);
char		*GenerateAuth (char *cdkey, char *challenge, char *outbuf);

extern char	gcd_gamename[256];
extern char	gcd_secret_key[256];

static team_t *TeamFromNode (qmenu_t *node)
{
	return (team_t *)((qmenu_t *)node->it)->it;
}
