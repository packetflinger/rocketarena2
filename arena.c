#include "g_local.h"
#include "gbucket.h"

extern int	votetries_setting;
qboolean	broken = false;

arena_t		arenas[MAX_ARENAS];
int			num_arenas;
qboolean	idmap;

qmenu_t		*teams;

motd_t		motd;
cvar_t		*admincode;

char		*teamskins[MAX_ARENA_SKINS] =
{
	"r2red", "r2blue", "r2dgre", "r2oran", "r2yell", "r2aqua", "r2lgre"
};

char		*vwepmodels[4] =
{
	"male", "female", "cyborg", "crakhor"
};

qboolean	teamskins_precachem[MAX_ARENA_SKINS];
qboolean	teamskins_precachef[MAX_ARENA_SKINS];
qboolean	teamskins_precachecw[MAX_ARENA_SKINS];
qboolean	teamskins_precachecb[MAX_ARENA_SKINS];

extern char	*dm_statusbar;

char		*va (char *format, ...);
float		PlayersRangeFromSpot (edict_t *spot);
void		ClientUserinfoChanged (edict_t *ent, char *userinfo);

void		teleporter_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);

void		P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
void		Weapon_Generic (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent));

qboolean	CheckTeamDamage (edict_t *targ, edict_t *attacker);

void		SV_AddGravity (edict_t *ent);

void		load_config (int numarenas);
void		set_config (int first, int last);
void		load_motd (void);

void		show_observer_menu (edict_t *ent);
void		show_arena_menu (edict_t *ent);
void		show_teamconfirm_menu (edict_t *ent, int arenanum);

int			ServerOpInt (void *gamep, char *key, bucketop_t op, int value, int owner);
char		*ServerOpString (void *gamep, char *key, bucketop_t op, char *value, int owner);
int			PlayerOpInt (void *gamep, char *key, bucketop_t op, int value, int player);
char		*PlayerOpString (void *gamep, char *key, bucketop_t op, char *value, int player);

/* gamex86.dll 0x20001000-0x20001030 (call-propagated) */
/* gamei386.so 0x00047b50-0x00047b77 */
void
add_to_queue (qmenu_t *node, qmenu_t *head)
{
	for (; head->next ; head = head->next)
		;

	head->next = node;
	node->prev = head;
	node->next = NULL;
}

/* gamex86.dll 0x20001030-0x20001070 (shape-matched(ratio=0.96)+collision-resolved) */
/* gamei386.so 0x00047b78-0x00047bbf */
qmenu_t *
remove_from_queue (qmenu_t *node, qmenu_t *head)
{
	if (!node)
	{
		if (head)
			node = head->next;

		if (!node)
			return NULL;
	}

	if (node->prev)
		node->prev->next = node->next;
	if (node->next)
		node->next->prev = node->prev;

	node->prev = NULL;
	node->next = NULL;

	return node;
}

/* gamex86.dll 0x20001070-0x200010a0 (call-propagated) */
/* gamei386.so 0x00047bc0-0x00047c0d */
void
add_to_front_queue (qmenu_t *node, qmenu_t *head)
{
	remove_from_queue (node, NULL);

	node->prev = head;
	node->next = head->next;
	if (head->next)
		head->next->prev = node;
	head->next = node;
}

/* gamex86.dll 0x200010a0-0x200010b6 (manual-confirmed) */
/* gamei386.so 0x00047c10-0x00047c27 */
int count_queue (qmenu_t *head)
{
	int		count;

	count = 0;
	while (head->next)
	{
		head = head->next;
		count++;
	}

	return count;
}

/* gamex86.dll 0x200010c0-0x200010ea (manual-confirmed) */
/* gamei386.so 0x00047c28-0x00047c4b */
int count_players_queue (qmenu_t *head)
{
	int			count;

	count = 0;
	while (head->next)
	{
		head = head->next;
		if (((edict_t *)head->it)->client->resp.fightstate == FIGHT_ALIVE)
			count++;
	}

	return count;
}

/* gamex86.dll 0x200010f0-0x20001147 (manual-confirmed) */
/* gamei386.so 0x00047c4c-0x00047c93 */
void set_damage (int arenanum, int state)
{
	qmenu_t		*tnode, *mnode;
	edict_t		*e;

	tnode = &arenas[arenanum].activeteams;

	while (tnode->next)
	{
		tnode = tnode->next;

		mnode = (qmenu_t *)tnode->it;

		while (mnode->next)
		{
			mnode = mnode->next;
			e = (edict_t *)mnode->it;
			if (e->client->resp.fightstate)
				e->takedamage = state;
		}
	}
}

/* gamex86.dll 0x20001150-0x20001590 (padded+majority) */
/* gamei386.so 0x00047c94-0x00048110 */
void give_ammo (edict_t *e)
{
	gitem_t		*w[9];
	arena_t		*arena = &arenas[e->client->resp.context];
	//	  0  2  3  4  5   6    9   8   7
	int			weapon_vals_x[] = { 256, 1, 2, 4, 8, 16, 128, 64, 32 };
	gitem_t		*it, *rl;
	qboolean	needswitch;
	int			i;

	// give health
	if (arena->health)
		e->health = arena->health;
	else
		e->health = 100;

	// give weapons
	rl = NULL;
	memset (w, 0, sizeof(w));

	w[0] = FindItemByClassname ("weapon_bfg");
	w[1] = FindItemByClassname ("weapon_shotgun");
	w[2] = FindItemByClassname ("weapon_supershotgun");
	w[3] = FindItemByClassname ("weapon_machinegun");
	w[4] = FindItemByClassname ("weapon_chaingun");
	w[5] = FindItemByClassname ("weapon_grenadelauncher");
	w[6] = FindItemByClassname ("weapon_railgun");
	w[7] = FindItemByClassname ("weapon_hyperblaster");
	w[8] = FindItemByClassname ("weapon_rocketlauncher");

	needswitch = false;

	for (i = 8; i >= 0; i--)
	{
		if (arena->weapons & weapon_vals_x[i])
		{
			if (!rl)
				rl = w[i];

			if (!e->client->pers.inventory[ITEM_INDEX(rl)] || needswitch)
			{
				e->client->newweapon = rl;
				e->client->pers.selected_item =
					e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
				needswitch = false;
			}

			e->client->pers.inventory[ITEM_INDEX(w[i])] = 1;
		}
		else
		{
			if (e->client->pers.weapon == w[i])
				needswitch = true;

			e->client->pers.inventory[ITEM_INDEX(w[i])] = 0;
		}
	}

	if (needswitch)
	{
		rl = FindItemByClassname ("weapon_blaster");
		e->client->newweapon = rl;
		e->client->pers.selected_item =
			e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
	}

	// give ammo
	if ((it = FindItemByClassname ("ammo_shells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->shells;
	if ((it = FindItemByClassname ("ammo_bullets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->bullets;
	if ((it = FindItemByClassname ("ammo_slugs"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->slugs;
	if ((it = FindItemByClassname ("ammo_grenades"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->grenades;
	if ((it = FindItemByClassname ("ammo_rockets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->rockets;
	if ((it = FindItemByClassname ("ammo_cells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->cells;

	// give body armor
	if ((it = FindItemByClassname ("item_armor_body")))
		e->client->pers.inventory[ITEM_INDEX(it)] = arena->armor;

	if (allow_grapple)
	{
		it = FindItem ("Grapple");
		if (it)
			e->client->pers.inventory[ITEM_INDEX(it)] = 1;
	}
}

/* gamex86.dll 0x20001590-0x20001670 (manual-confirmed) */
/* gamei386.so 0x00048110-0x000481e8 */
void
ValidatePlayer (edict_t *ent, void *gamep)
{
	char	*pid;
	char	*pass;
	char	auth[36];
	int		pidnum;

	pid = Info_ValueForKey (ent->client->pers.userinfo, "pid");
	pass = Info_ValueForKey (ent->client->pers.userinfo, "pass");

	if (!pid[0] || !pass[0])
		return;

	pidnum = atoi (pid);

	GenerateAuth (GetChallenge (gamep), pass, auth);

	bopfuncs[BOP_PLAYER_INT] (gamep, "pid", bucketfuncs[BUCKET_SET],
		pidnum, ent - g_edicts + 1);
	((bucketopstrfn_t)bopfuncs[BOP_PLAYER_STRING]) (gamep, "auth",
		bucketfuncs[BUCKET_SET], auth, ent - g_edicts + 1);
}

/* gamex86.dll 0x20001670-0x200017cc (manual-confirmed) */
/* gamei386.so 0x000481e8-0x000483e4 */
void
NewStatsPlayer (void *gamep, edict_t *ent, int team)
{
	NewPlayer (gamep, ent - g_edicts + 1, ent->client->pers.netname);

	bopfuncs[BOP_PLAYER_INT] (gamep, "team", bucketfuncs[BUCKET_SET],
		GetTeamIndex (gamep, team), ent - g_edicts + 1);
	bopfuncs[BOP_PLAYER_INT] (gamep, "score", bucketfuncs[BUCKET_SET], 0,
		ent - g_edicts + 1);
	bopfuncs[BOP_PLAYER_INT] (gamep, "ping", bucketfuncs[BUCKET_SET],
		ent->client->ping, ent - g_edicts + 1);
	bopfuncs[BOP_PLAYER_INT] (gamep, "deaths", bucketfuncs[BUCKET_SET], 0,
		ent - g_edicts + 1);
	bopfuncs[BOP_PLAYER_INT] (gamep, "suicides", bucketfuncs[BUCKET_SET], 0,
		ent - g_edicts + 1);

	ValidatePlayer (ent, gamep);
}

/* gamex86.dll 0x200017d0-0x200019f0 (padded) */
/* gamei386.so 0x000483e4-0x00048661 */
team_t *add_to_team (edict_t *ent, char *teamname)
{
	int		i;
	team_t	*t;

	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (!teams[i].it)
			continue;

		t = teams[i].it;
		if (strcmp (t->name, teamname))
			continue;

		if (t->arenanum)
		{
			if (count_queue (&teams[i]) == arenas[t->arenanum].playersperteam)
				return NULL;

			if (arenas[t->arenanum].locked)
				return NULL;

			if (t->fighting && arenas[t->arenanum].statsptr)
				NewStatsPlayer (arenas[t->arenanum].statsptr, ent, i);
		}

		add_to_queue (&ent->client->resp.teammember, &teams[i]);
		ent->client->resp.teamnum = i;

		if (t->skin != -1)
			setteamskin (ent, ent->client->pers.userinfo, t->skin);

		gi.bprintf (PRINT_MEDIUM, "%s has been added to team %d (%s)\n",
			ent->client->pers.netname, i, teamname);

		return t;
	}

	for (i = 0; i < MAX_TEAMS; i++)
		if (!teams[i].it)
			break;

	t = gi.TagMalloc (sizeof (team_t), TAG_LEVEL);
	if (!t)
	{
		gi.error ("Ateam malloc failed!\n");
		return NULL;
	}

	t->name = teamname;
	t->teamnum = i;
	t->arenanum = 0;
	t->wins = -1;
	t->skin = -1;
	t->arenalink.it = &teams[i];
	t->fighting = 0;
	teams[i].it = t;

	if (ent)
	{
		add_to_queue (&t->arenalink, &arenas[0].waitingteams);
		t->locked = false;
		t->side = -1;

		add_to_queue (&ent->client->resp.teammember, &teams[i]);
		ent->client->resp.teamnum = i;

		gi.bprintf (PRINT_MEDIUM, "%s has created team number %d (%s)\n",
			ent->client->pers.netname, i, teamname);
	}
	else
		t->locked = true;

	return t;
}

/* gamex86.dll 0x200019f0-0x20001ae0 (padded) */
/* gamei386.so 0x00048664-0x00048762 */
void remove_from_team (edict_t *ent)
{
	qmenu_t	*node;

	if (ent->client->resp.teamnum < 0)
		return;

	node = &ent->client->resp.teammember;

	if (!TEAM (&teams[ent->client->resp.teamnum])->name)
	{
		gi.dprintf ("ERROR in remove_from_team -- please e-mail crt\n");
		return;
	}

	gi.bprintf (PRINT_MEDIUM, "%s has been removed from team %d (%s)\n",
		ent->client->pers.netname, ent->client->resp.teamnum,
		TEAM (&teams[ent->client->resp.teamnum])->name);

	if (TEAM (&teams[ent->client->resp.teamnum])->fighting &&
		arenas[ent->client->resp.context].statsptr)
		RemovePlayer (arenas[ent->client->resp.context].statsptr, ent - g_edicts + 1);

	remove_from_queue (node, NULL);

	check_teams (ent->client->resp.context);

	ent->client->resp.teamnum = -1;
}

/* gamex86.dll 0x20001ae0-0x20001b90 (manual-confirmed) */
/* gamei386.so 0x00048764-0x0004880b */
edict_t *SelectRandomArenaSpawnPoint (char *classn, int arenanum, int side)
{
	edict_t		*spot;
	int			count = 0;
	int			selection;

	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), classn)) != NULL)
	{
		if (spot->arena != arenanum && idmap == false) continue;
		count++;
	}

	if (!count)
		return NULL;

	selection = rand () % count;

	//gi.dprintf("%d spots, %d selected\n",count,selection);

	if (side)
	{
		selection &= ~1;
		if (side == 1)
		{
			selection++;
			if (selection >= count)
				selection = 1;
		}
	}

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), classn);
		if (spot->arena != arenanum && idmap == false)
			selection++;
	} while (selection--);

	return spot;
}

/* gamex86.dll 0x20001b90-0x20001c20 (aligned) */
/* gamei386.so 0x0004880c-0x00048907 */
edict_t *SelectFarthestArenaSpawnPoint (char *classn, int arenanum)
{
	edict_t		*bestspot;
	float		bestdistance, bestplayerdistance;
	edict_t		*spot;

	spot = NULL;
	bestspot = NULL;
	bestdistance = 50;
	while ((spot = G_Find (spot, FOFS(classname), classn)) != NULL)
	{
		//gi.bprintf (PRINT_HIGH,"arena %d spot %d\n", arenanum, spot->arena);
		if (spot->arena != arenanum && idmap == false) continue;
		bestplayerdistance = PlayersRangeFromSpot (spot);

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
	{
		return bestspot;
	}

	// if there is a player just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	return SelectRandomArenaSpawnPoint (classn, arenanum, 0);
}

/* gamex86.dll 0x20001c20-0x20001c80 (aligned) */
/* gamei386.so 0x00048908-0x00048973 */
void track_SetStats (edict_t *ent)
{
	edict_t	*target;
	int		score;

	target = ent->client->resp.track_target;
	score = ent->client->resp.score;

	memcpy (ent->client->ps.stats, target->client->ps.stats,
		sizeof (ent->client->ps.stats));

	ent->client->ps.stats[STAT_FRAGS] = score;

	if (ent->client->scoremode)
		ent->client->ps.stats[STAT_LAYOUTS] |= 1;
	else
		ent->client->ps.stats[STAT_LAYOUTS] &= ~1;

	CTFSetIDView (ent);
}

/* gamex86.dll 0x20001c80-0x20001e0c (aligned) */
/* gamei386.so 0x00048974-0x00048c74 */
void eyecam_think (edict_t *ent, usercmd_t *ucmd)
{
	edict_t		*target;
	vec3_t		forward;
	vec3_t		dest;
	vec3_t		zero = {0, 0, 0};
	vec3_t		mins = {-16, -16, -24};
	vec3_t		maxs = {16, 16, 32};
	int			i;

	target = ent->client->resp.track_target;
	if (!target || target->client->resp.fightstate != FIGHT_ALIVE)
	{
		track_next (ent);
		return;
	}

	gi.unlinkentity (ent);

	VectorCopy (target->s.origin, ent->s.origin);

	AngleVectors (target->client->v_angle, forward, NULL, NULL);
	VectorScale (forward, 20, dest);

	ent->s.origin[0] = ent->s.origin[0] + dest[0];
	ent->s.origin[1] = ent->s.origin[1] + dest[1];
	ent->s.origin[2] = ent->s.origin[2] + dest[2] + 22;

	VectorCopy (zero, ent->velocity);

	VectorCopy (target->client->v_angle, ent->s.angles);
	VectorCopy (target->client->v_angle, ent->client->ps.viewangles);
	VectorCopy (target->client->v_angle, ent->client->v_angle);

	for (i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT (ent->s.angles[i] - ent->client->resp.cmd_angles[i]);

	gi.linkentity (ent);

	track_SetStats (ent);
}

/*
 * True when ent is a spectator watching a live player through their eyes.
 */
qboolean eyecam_active (edict_t *ent)
{
	edict_t	*target;

	if (ent->client->resp.fightstate != FIGHT_SPECTATING ||
		ent->client->resp.omode != EYECAM)
		return false;

	target = ent->client->resp.track_target;
	return target && target->inuse && target->client &&
		target->client->resp.fightstate == FIGHT_ALIVE;
}

/*
 * Called from ClientEndServerFrame after the target's own view has been
 * calculated for this frame. Mirrors the target's view (eye position,
 * angles, kick, gun model and screen blend) onto the spectator.
 *
 * On servers that support GMF_CLIENTNUM the target's entity is hidden from
 * the spectator by setting clientNum, so the camera can sit exactly in the
 * target's eyes. Older servers would draw the target's model around the
 * camera, so the camera is pushed out in front of their face instead.
 */
void eyecam_SetView (edict_t *ent)
{
	gclient_t	*client = ent->client;
	gclient_t	*tclient;
	edict_t		*target;
	vec3_t		eye, goal, forward, delta;
	trace_t		tr;
	int			i;

	if (!eyecam_active (ent))
	{
		// leaving in-eyes: give the spectator their own gun back
		if (client->eyecam_view)
		{
			client->eyecam_view = false;
			VectorClear (client->ps.kick_angles);
			VectorClear (client->ps.gunangles);
			VectorClear (client->ps.gunoffset);
			client->ps.gunframe = 0;
			if (client->pers.weapon)
			{
				client->weaponstate = WEAPON_ACTIVATING;
				client->ps.gunindex = gi.modelindex (client->pers.weapon->view_model);
			}
			else
				client->ps.gunindex = 0;
		}
		return;
	}

	target = client->resp.track_target;
	tclient = target->client;
	client->eyecam_view = true;

	VectorAdd (target->s.origin, tclient->ps.viewoffset, eye);
	VectorCopy (eye, goal);

	if (game.server_features & GMF_CLIENTNUM)
		client->clientNum = target - g_edicts - 1;
	else
	{
		AngleVectors (tclient->v_angle, forward, NULL, NULL);
		VectorMA (eye, 30, forward, goal);

		// don't poke the camera through walls
		tr = gi.trace (eye, vec3_origin, vec3_origin, goal, target, MASK_SOLID);
		if (tr.fraction < 1)
		{
			VectorSubtract (tr.endpos, eye, delta);
			VectorMA (eye, 0.9f, delta, goal);
		}
	}

	// the client adds viewoffset to the origin, so back it out
	VectorSubtract (goal, tclient->ps.viewoffset, ent->s.origin);
	VectorClear (ent->velocity);
	gi.linkentity (ent);

	for (i = 0; i < 3; i++)
	{
		client->ps.pmove.origin[i] = ent->s.origin[i] * 8;
		client->ps.pmove.velocity[i] = 0;
	}
	client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

	VectorCopy (tclient->ps.viewoffset, client->ps.viewoffset);
	VectorCopy (tclient->ps.viewangles, client->ps.viewangles);
	VectorCopy (tclient->ps.kick_angles, client->ps.kick_angles);

	client->ps.gunindex = tclient->ps.gunindex;
	client->ps.gunframe = tclient->ps.gunframe;
	VectorCopy (tclient->ps.gunangles, client->ps.gunangles);
	VectorCopy (tclient->ps.gunoffset, client->ps.gunoffset);

	for (i = 0; i < 4; i++)
		client->ps.blend[i] = tclient->ps.blend[i];
	client->ps.rdflags = tclient->ps.rdflags;
}

/* gamex86.dll 0x20001e10-0x200020c4 (manual-confirmed) */
/* gamei386.so 0x00048c74-0x00048fbb */
void track_think (edict_t *ent, usercmd_t *ucmd)
{
	edict_t		*target;
	vec3_t		forward;
	vec3_t		dest;
	int			stuck = 0;
	vec3_t		zero = {0, 0, 0};
	vec3_t		mins = {-16, -16, -24};
	vec3_t		maxs = {16, 16, 32};
	trace_t		tr;
	int			i;

	target = ent->client->resp.track_target;
	if (!target || target->client->resp.fightstate != FIGHT_ALIVE)
	{
		track_next (ent);
		return;
	}

	VectorCopy (ent->client->ps.viewangles, forward);
	AngleVectors (forward, forward, NULL, NULL);

	VectorScale (forward, 150, dest);
	VectorSubtract (target->s.origin, dest, dest);

	tr = gi.trace (target->s.origin, zero, zero, dest, target, MASK_SOLID);
	if (tr.fraction < 1.0)
	{
		VectorScale (forward, tr.fraction * -130, dest);
		VectorAdd (target->s.origin, dest, dest);
	}

	tr = gi.trace (ent->s.origin, mins, maxs, ent->s.origin, ent,
		MASK_PLAYERSOLID);
	if (tr.contents & MASK_SOLID)
		stuck = 1;

	if (!stuck)
		tr = gi.trace (ent->s.origin, mins, maxs, dest, ent, MASK_SOLID);

	if (tr.fraction < 1.0 || stuck)
	{
		gi.unlinkentity (ent);
		VectorCopy (dest, ent->s.origin);
		gi.linkentity (ent);
		VectorCopy (zero, ent->velocity);
	}
	else
	{
		VectorSubtract (dest, ent->s.origin, dest);
		for (i = 0; i < 3; i++)
			ent->velocity[i] = dest[i] * 10;
	}

	track_SetStats (ent);
}

/* gamex86.dll 0x200020d0-0x20002254 (manual-confirmed) */
/* gamei386.so 0x00048fbc-0x0004917b */
void track_change (edict_t *ent, int dir)
{
	edict_t		*target, *e;
	int			i;
	qboolean	wrapped;

	wrapped = false;

	target = ent->client->resp.track_target;
	if (!target)
	{
		target = &g_edicts[1];
		wrapped = true;
	}
	else if (target->client->resp.fightstate != FIGHT_ALIVE ||
			 target->client->resp.context != ent->client->resp.context)
		wrapped = true;

	i = target - g_edicts;

	do
	{
		i += dir;
		if ((float) i > maxclients->value)
			i = 1;
		if (i < 1)
			i = (int) maxclients->value;

		e = &g_edicts[i];

		if (e->inuse &&
			e->client->resp.fightstate == FIGHT_ALIVE &&
			e->client->resp.context == ent->client->resp.context &&
			(!arenas[ent->client->resp.context].competition ||
			 ent->client->resp.teamnum == e->client->resp.teamnum) &&
			e->solid)
		{
			wrapped = false;
			break;
		}
	} while (e != target);

	if (e != target || !wrapped)
	{
		ent->client->resp.track_target = e;
		gi.cprintf (ent, PRINT_HIGH, "Tracking %s\n", e->client->pers.netname);
		return;
	}

	ent->client->resp.omode = ent->client->resp.lastomode;
	move_to_arena (ent, ent->client->resp.context, 2);
	gi.cprintf (ent, PRINT_HIGH, "No one to track\n");
}

/* gamex86.dll 0x20002260-0x20002270 (manual-confirmed) */
/* gamei386.so 0x0004917c-0x0004918c */
void track_next (edict_t *ent)
{
	track_change (ent, 1);
}

/* gamex86.dll 0x20002270-0x20002280 (manual-confirmed) */
/* gamei386.so 0x0004918c-0x0004919c */
void track_prev (edict_t *ent)
{
	track_change (ent, -1);
}

/* gamex86.dll 0x20002280-0x200024e4 (aligned) */
/* gamei386.so 0x0004919c-0x000493e6 */
void SetObserverMode (edict_t *ent)
{
	int		i;

	switch (ent->client->resp.omode)
	{
	case NORMAL:
		ent->movetype = MOVETYPE_WALK;
		ent->solid = SOLID_BBOX;
		ent->clipmask = MASK_PLAYERSOLID;
		ent->svflags &= ~SVF_NOCLIENT;
		ent->client->resp.track_target = NULL;
		ent->s.modelindex = 255;
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		break;

	case FREEFLYING:
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->svflags |= SVF_NOCLIENT;
		ent->client->resp.track_target = NULL;
		ent->client->ps.pmove.pm_time = 0;
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		ent->client->ps.pmove.pm_flags &= ~PMF_TIME_TELEPORT;
		break;

	case TRACKCAM:
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->svflags |= SVF_NOCLIENT;
		ent->s.modelindex = 0;
		ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

		for (i = 0; i < 3; i++)
		{
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT (-ent->client->resp.cmd_angles[i]);
			ent->s.angles[i] = 0;
		}

		VectorCopy (ent->s.angles, ent->client->ps.viewangles);
		VectorCopy (ent->s.angles, ent->client->v_angle);

		if (!ent->client->resp.track_target ||
			ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
			track_next (ent);
		break;

	case EYECAM:
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->svflags |= SVF_NOCLIENT;
		ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

		for (i = 0; i < 3; i++)
		{
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT (-ent->client->resp.cmd_angles[i]);
			ent->s.angles[i] = 0;
		}

		VectorCopy (ent->s.angles, ent->client->ps.viewangles);
		VectorCopy (ent->s.angles, ent->client->v_angle);

		if (!ent->client->resp.track_target ||
			ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
			track_next (ent);
		break;
	}
}

/* gamex86.dll 0x20002500-0x20002812 (manual-confirmed) */
/* gamei386.so 0x000493e8-0x0004986c */
void move_to_arena (edict_t *ent, int arenanum, int mode)
{
	edict_t		*dest;
	int			i;
	vec3_t		mins = {-16, -16, -24};
	vec3_t		maxs = {16, 16, 32};

	if (ent->client->resp.isbot)
	{
		gi.dprintf ("\n%s IS A ZBOT %d\n", ent->client->pers.netname, ent->client->resp.isbot);
		gi.centerprintf (ent, "The server seems to think you\nare a bot. If you aren't,\n you may wish to reconnect");
	}

	if (mode)
	{

		if (!arenas[arenanum].active)
			dest = SelectFarthestArenaSpawnPoint ("misc_teleporter_dest", arenanum);
		else
			dest = SelectFarthestArenaSpawnPoint ("info_player_deathmatch", arenanum);

		if (arenanum)
		{
			if (ent->client->resp.context == 0)
			{
				ent->client->resp.context = arenanum;
				show_observer_menu (ent);
			}
		}
		else
		{
			ent->client->resp.track_target = NULL;
			if (ent->client->resp.teamnum != -1)
				show_arena_menu (ent);
		}

		ent->client->resp.context = arenanum;
	}
	else
	{
		//get rid of all menus
		ent->client->resp.context = arenanum;
		ClientUserinfoChanged (ent, ent->client->pers.userinfo);

		if (arenas[arenanum].idarena)
			dest = SelectRandomArenaSpawnPoint ("info_player_deathmatch", arenanum,
				(TEAM (&teams[ent->client->resp.teamnum])->side == arenas[arenanum].sidepick) ? 1 : 2);
		else
			dest = SelectFarthestArenaSpawnPoint ("info_player_deathmatch", arenanum);
	}

	if (!dest)
	{
		gi.bprintf (PRINT_HIGH, "no dest found\n");
		return;
	}

	gi.unlinkentity (ent);

	VectorCopy (dest->s.origin, ent->s.origin);
	VectorCopy (dest->s.origin, ent->s.old_origin);
	ent->s.origin[2] += 10;

	// clear the velocity and hold them in place briefly
	VectorClear (ent->velocity);

	ent->client->ps.pmove.pm_time = 160>>3;		// hold time
	ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

	// draw the teleport splash at source and on the player
	if (mode == 0)
		ent->s.event = EV_PLAYER_TELEPORT;

	// set angles
	for (i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT (dest->s.angles[i] - ent->client->resp.cmd_angles[i]);

	VectorClear (ent->s.angles);
	VectorClear (ent->client->ps.viewangles);
	VectorClear (ent->client->v_angle);

	// telefrag avoidance at destination
	if (!KillBox (ent))
	{
	}

	if (mode)
	{
		if (arenas[arenanum].active && ent->client->resp.omode == NORMAL)
			ent->client->resp.omode = FREEFLYING;

		if (arenas[arenanum].competition && mode != 2)
			ent->client->resp.omode = EYECAM;

		SetObserverMode (ent);
	}
	else
	{
		ent->client->resp.omode = NORMAL;
		SetObserverMode (ent);
	}

	gi.linkentity (ent);

	if (arenas[arenanum].proposetime > level.time && !ent->client->resp.voted)
	{
		menu_centerprint (ent, va ("Settings changes have been proposed\n by %s!\nGoto the observer menu (TAB) to vote",
			arenas[arenanum].proposer->client->pers.netname));
		stuffcmd (ent, "play misc/pc_up.wav\n");
	}
}

char		*omode_descriptions[4] =
{
	"Normal", "Free Flying", "Trackcam", "In Eyes"
};

/* gamex86.dll 0x20002820-0x200028a0 (padded) */
/* gamei386.so 0x0004986c-0x000498db */
void ChangeOMode (edict_t *ent)
{
	if (!ent->client->resp.fightstate)
	{
		if (ent->client->resp.omode != TRACKCAM && ent->client->resp.omode != EYECAM)
			ent->client->resp.lastomode = ent->client->resp.omode;

		ent->client->resp.omode = (ent->client->resp.omode + 1) % 4;

		gi.cprintf (ent, PRINT_HIGH, "Switched Observer Mode to: %s\n",
			omode_descriptions[ent->client->resp.omode]);

		move_to_arena (ent, ent->client->resp.context, 1);
	}
}

/* gamex86.dll 0x200028a0-0x20002910 (shape-matched(ratio=0.66)) */
/* gamei386.so 0x000498dc-0x00049a19 */
int getfreeskin (int arenanum)
{
	qboolean	used[MAX_ARENA_SKINS];
	int			i;
	team_t		*t;

	memset (used, 0, sizeof (used));

	for (i = 0; i < MAX_TEAMS; i++)
	{
		t = teams[i].it;
		if (!t)
			continue;
		if (t->arenanum != arenanum)
			continue;
		if (t->skin == -1)
			continue;

		used[t->skin] = true;
	}

	for (i = 0; i < MAX_ARENA_SKINS; i++)
		if (!used[i])
			return i;

	return rand () % MAX_ARENA_SKINS;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00049a1c-0x00049a44 */
char *mylcase (char *s)
{
	char	*p;
	int		c;

	for (p = s; *p; p++)
	{
		c = *p;
		if (c >= 'A' && c <= 'Z')
			*p += 'a' - 'A';
	}

	return s;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00049a44-0x00049aa1 */
qboolean checkvwepmodel (char *s)
{
	int		i;

	for (i = 0; i < 4; i++)
		if (strstr (s, vwepmodels[i]))
			return true;

	return false;
}

/* gamex86.dll 0x20002910-0x20002cc0 (call-propagated) */
/* gamei386.so 0x00049aa4-0x00049d16 */
void setteamskin (edict_t *ent, char *userinfo, int skinnum)
{
	char	*val;
	int		pnum;

	pnum = ent - g_edicts - 1;
	val = Info_ValueForKey (userinfo, "skin");

	if (val[0] == 'f')
	{
		if (strcmp (val, va ("female/%s", teamskins[skinnum])))
			gi.configstring (CS_PLAYERSKINS + pnum,
				va ("%s\\female/%s", ent->client->pers.netname, teamskins[skinnum]));

		Info_RemoveKey (userinfo, "skin");
		strcat (userinfo, va ("\\skin\\female/%s", teamskins[skinnum]));

		stuffcmd (ent, "skin female/nullxxx\n");
	}
	else if (val[0] == 'c' && val[1] == 'r')
	{
		if (strcmp (val, va ("crakhor/%s", teamskins[skinnum])))
			gi.configstring (CS_PLAYERSKINS + pnum,
				va ("%s\\crakhor/%s", ent->client->pers.netname, teamskins[skinnum]));

		Info_RemoveKey (userinfo, "skin");
		strcat (userinfo, va ("\\skin\\crakhor/%s", teamskins[skinnum]));

		stuffcmd (ent, "skin crakhor/nullxxx\n");
	}
	else if (val[0] == 'c' && val[1] == 'y')
	{
		if (strcmp (val, va ("cyborg/%s", teamskins[skinnum])))
			gi.configstring (CS_PLAYERSKINS + pnum,
				va ("%s\\cyborg/%s", ent->client->pers.netname, teamskins[skinnum]));

		Info_RemoveKey (userinfo, "skin");
		strcat (userinfo, va ("\\skin\\cyborg/%s", teamskins[skinnum]));

		stuffcmd (ent, "skin cyborg/nullxxx\n");
	}
	else
	{
		if (strcmp (val, va ("male/%s", teamskins[skinnum])))
			gi.configstring (CS_PLAYERSKINS + pnum,
				va ("%s\\male/%s", ent->client->pers.netname, teamskins[skinnum]));

		Info_RemoveKey (userinfo, "skin");
		strcat (userinfo, va ("\\skin\\male/%s", teamskins[skinnum]));

		stuffcmd (ent, "skin male/nullxxx\n");
	}
}

/* gamex86.dll 0x20002cc0-0x20002ed0 (call-propagated-reverse) */
/* gamei386.so 0x00049d18-0x0004a058 */
void SendTeamToArena (qmenu_t *team, int arenanum, qboolean observer, qboolean announce)
{
	qmenu_t		*mnode;
	edict_t		*ent;
	int			statsteam;

	statsteam = -1;
	mnode = team;

	if (!TEAM (team)->outofline)
	{
		if (arenanum && TEAM (team)->skin == -1
			&& (arenas[arenanum].playersperteam > 1 || arenas[arenanum].idarena))
			TEAM (team)->skin = getfreeskin (arenanum);
		else if (!arenanum
			|| (arenas[arenanum].playersperteam == 1 && !arenas[arenanum].idarena))
			TEAM (team)->skin = -1;
	}

	if (!observer && announce && arenas[arenanum].statsptr)
	{
		statsteam = team - teams;

		NewTeam (arenas[arenanum].statsptr, statsteam, TEAM (team)->name);
		bopfuncs[BOP_TEAM_INT] (arenas[arenanum].statsptr, "score",
			bucketfuncs[BUCKET_SET], 0, statsteam);
	}

	while (mnode->next)
	{
		mnode = mnode->next;
		ent = (edict_t *)mnode->it;

		if (TEAM (team)->skin != -1)
			setteamskin (ent, ent->client->pers.userinfo, TEAM (team)->skin);

		if (observer)
		{
			ent->client->resp.fightstate = FIGHT_SPECTATING;
			ent->takedamage = DAMAGE_NO;
			move_to_arena (ent, arenanum, 1);
		}
		else
		{
			ent->client->resp.fightstate = FIGHT_ALIVE;
			ent->takedamage = DAMAGE_NO;
			move_to_arena (ent, arenanum, 0);
			give_ammo (ent);

			if (statsteam != -1)
				NewStatsPlayer (arenas[arenanum].statsptr, ent, statsteam);
		}
	}

	if (announce)
	{
		if (observer)
		{
			TEAM (team)->fighting = false;
			add_to_queue (&TEAM (team)->arenalink, &arenas[arenanum].waitingteams);
		}
		else
		{
			add_to_queue (&TEAM (team)->arenalink, &arenas[arenanum].activeteams);
		}
	}

	TEAM (team)->arenanum = arenanum;

	gi.dprintf ("%d: %d %s entered\n", arenanum, TEAM (team)->teamnum,
		TEAM (team)->name);
}

/* gamex86.dll 0x20002ed0-0x20003103 (unpadded-prologue+majority+corrected) */
/* gamei386.so 0x0004a058-0x0004a2b4 */
int AddtoArena (edict_t *ent, int arenanum, int allow_partial, int skip_checks)
{
	team_t	*t;
	int		membercount;


	if (!skip_checks)
	{
		if (arenas[arenanum].minping && ent->client->ping < arenas[arenanum].minping)
		{
			menu_centerprint (ent, va ("Your ping is too low\nMinimum ping for this arena: %d", arenas[arenanum].minping));
			return 1;
		}

		if (arenas[arenanum].maxping && ent->client->ping > arenas[arenanum].maxping)
		{
			menu_centerprint (ent, va ("Your ping is too high\nMaximum ping for this arena: %d", arenas[arenanum].maxping));
			return 1;
		}

		if (arenas[arenanum].locked)
		{
			menu_centerprint (ent, "Sorry, that Arena is locked by an admin\n");
			return 1;
		}

		if (arenas[arenanum].idarena)
		{
			menu_centerprint (ent, "You must join a pickup team to\n enter that arena");
			return 1;
		}

		if (count_queue (&arenas[arenanum].waitingteams) + count_queue (&arenas[arenanum].activeteams) >= arenas[arenanum].maxteams)
		{
			menu_centerprint (ent, "Sorry, that arena is full");
			return 1;
		}
	}

	membercount = count_queue (&teams[ent->client->resp.teamnum]);

	if (!(membercount != arenas[arenanum].playersperteam
		&& (membercount > arenas[arenanum].playersperteam || !allow_partial)))
	{
		TEAM (&teams[ent->client->resp.teamnum])->outofline = skip_checks;

		if (!skip_checks)
		{
			remove_from_queue (&TEAM (&teams[ent->client->resp.teamnum])->arenalink, NULL);
			SendTeamToArena (&teams[ent->client->resp.teamnum], arenanum, true, true);
		}
		else
			SendTeamToArena (&teams[ent->client->resp.teamnum], arenanum, true, false);

		return 0;
	}

	if (count_queue (&teams[ent->client->resp.teamnum]) < arenas[arenanum].playersperteam)
	{
		show_teamconfirm_menu (ent, arenanum);
		return 1;
	}

	menu_centerprint (ent, va ("You have the incorrect number\nof team members, you need %d to play \nin that arena", arenas[arenanum].playersperteam));

	return 1;
}

/* gamex86.dll 0x20003110-0x200032bf (manual-confirmed) */
/* gamei386.so 0x0004a2b4-0x0004a524 */
void check_teams (int arenanum)
{
	qmenu_t		*tnode, *prev_tnode, *mnode;
	int			i;
	int			ping;
	qboolean	rejected;

	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (!teams[i].it)
			continue;
		if (count_queue (&teams[i]) != 0)
			continue;

		if (TEAM (&teams[i])->locked)
			continue;

		remove_from_queue (&TEAM (&teams[i])->arenalink, NULL);
		gi.dprintf ("Clearing team %d (%s)\n", TEAM (&teams[i])->teamnum,
			TEAM (&teams[i])->name);
		gi.TagFree (TEAM (&teams[i]));
		teams[i].it = NULL;
	}

	if (!arenanum)
		return;

	tnode = &arenas[arenanum].waitingteams;

	while (tnode->next)
	{
		tnode = tnode->next;
		mnode = (qmenu_t *)tnode->it;
		rejected = false;

		if (!arenas[arenanum].idarena)
		{
			while (mnode->next)
			{
				mnode = mnode->next;

				ping = ((edict_t *)mnode->it)->client->ping;

				if ((ping > arenas[arenanum].maxping && ping < 1000)
					|| ping < arenas[arenanum].minping)
				{
					rejected = true;
					gi.cprintf ((edict_t *)mnode->it, PRINT_HIGH,
						"Sorry, your ping of %d does not work in this arena\n", ping);
				}
			}
		}

		if (count_queue ((qmenu_t *)tnode->it) > arenas[arenanum].playersperteam || rejected)
		{
			gi.bprintf (PRINT_MEDIUM, "Removing team %d (%s)\n",
				TEAM ((qmenu_t *)tnode->it)->teamnum,
				TEAM ((qmenu_t *)tnode->it)->name);

			prev_tnode = tnode->prev;
			remove_from_queue (tnode, NULL);
			SendTeamToArena ((qmenu_t *)tnode->it, 0, true, true);
			tnode = prev_tnode;
		}
	}

	if (arenas[arenanum].changed && count_queue (&arenas[arenanum].waitingteams) + count_queue (&arenas[arenanum].activeteams) == 0)
	{
		set_config (arenanum, arenanum);
		gi.dprintf ("%d: Reseting to default config\n", arenanum);
	}
}

/* gamex86.dll 0x200032c0-0x20003370 (manual-confirmed) */
/* gamei386.so 0x0004a524-0x0004a5d4 */
void init_player (edict_t *ent)
{
	ent->client->resp.teammember.it = ent;
	ent->client->resp.fightstate = FIGHT_SPECTATING;
	ent->client->resp.context = 0;
	ent->client->resp.teamnum = -1;

	if (ent->client->pers.showmotd)
		motd_menu (ent);
	else
		menuRefreshTeamList (ent, NULL, NULL, 0);

	ent->client->resp.track_target = NULL;
	ent->client->resp.lastomode = FREEFLYING;
	ent->takedamage = DAMAGE_NO;

	send_configstring (ent, game.num_items + 0x422, " Red");
	send_configstring (ent, game.num_items + 0x423, "Blue");
}

/* gamex86.dll 0x20003370-0x200033a0 (manual-confirmed) */
/* gamei386.so 0x0004a5d4-0x0004a600 */
void reinit_player (edict_t *ent)
{
	ent->client->resp.fightstate = FIGHT_SPECTATING;
	ent->client->resp.track_target = NULL;
	ent->client->resp.lastomode = FREEFLYING;
}

/* gamex86.dll 0x200033a0-0x20003420 (manual-confirmed) */
/* gamei386.so 0x0004a600-0x0004a66f */
void show_stringc (char *s, int context)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<maxclients->value ; i++)
	{
		e = &g_edicts[i + 1];
		if(e->inuse && e->client && e->client->resp.context == context) {
			gi.centerprintf(e, s);
		}
	}
}

/* gamex86.dll 0x20003420-0x200034a0 (manual-confirmed) */
/* gamei386.so 0x0004a670-0x0004a6e4 */
void show_string (int priority, char *s, int context)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<maxclients->value ; i++)
	{
		e = &g_edicts[i + 1];
		if(e->inuse && e->client && e->client->resp.context == context) {
			gi.cprintf(e,priority, s);
		}
	}
}

/* gamex86.dll 0x200034a0-0x200034d0 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004a6e4-0x0004a70f */
void stuffcmd (edict_t *ent, char *s)
{
	gi.WriteByte (svc_stufftext);
	gi.WriteString (s);
	gi.unicast (ent, true);
}

/* gamex86.dll 0x200034d0-0x20003560 (padded) */
/* gamei386.so 0x0004a710-0x0004a7a1 */
void send_sound_to_arena (char *soundname, int context)
{
	int		i;
	edict_t	*e;

	for (i=0 ; i<maxclients->value ; i++)
	{
		e = &g_edicts[i + 1];
		if(e->inuse && e->client && e->client->resp.context == context) {
			stuffcmd(e, va("play %s\n",soundname));

		}
	}
}

/* gamex86.dll 0x20003560-0x20003590 (bracketed) */
/* gamei386.so 0x0004a7a4-0x0004a7dd */
void send_configstring (edict_t *e, int index, char *string)
{
	gi.WriteByte (svc_configstring);
	gi.WriteShort (index);
	gi.WriteString (string);
	gi.unicast (e, true);
}

/* gamex86.dll 0x20003590-0x20003780 (bracketed) */
/* gamei386.so 0x0004a7e0-0x0004aa61 */
void show_countdown (int countdown, int arenanum)
{
	int		i;
	edict_t	*e;


	for (i = 0; i < maxclients->value; i++)
	{
		e = &g_edicts[i + 1];
		if (!e->inuse || !e->client)
			continue;
		if (e->client->resp.context != arenanum)
			continue;

		if (arenas[arenanum].state == 0)
			send_configstring (e, game.num_items + 0x420, "Waiting for match to start");
		else
			send_configstring (e, game.num_items + 0x420, arenas[arenanum].vs);

		if (arenas[arenanum].rounds > 1)
			send_configstring (e, game.num_items + 0x421, va ("Round %d of %d", arenas[arenanum].round, arenas[arenanum].rounds));
		else
			send_configstring (e, game.num_items + 0x421, "");

		e->client->ps.stats[STAT_ARENASTATUS] = game.num_items + 0x420;
		e->client->ps.stats[STAT_ROUNDINFO] = game.num_items + 0x421;
		e->client->ps.stats[STAT_COUNTDOWN] = countdown;

		if (countdown == 15 || countdown == 10 || countdown == 5)
		{
			if (!e->client->showmenu)
				SendStatusBar (e, dm_statusbar, true);
		}

		if (countdown > 0 && countdown < 4 && arenas[arenanum].state)
			stuffcmd (e, va ("play ra/%d.wav\n", countdown));
		else if (!countdown && arenas[arenanum].state)
		{
			stuffcmd (e, "play ra/fight.wav\n");
			gi.centerprintf (e, "FIGHT!");
		}
	}
}

/*
 * Whole seconds left in the current fight, rounded up so the clock
 * reads 0:00 only once time has actually run out.
 */
int round_time_left (int arenanum)
{
	arena_t	*arena = &arenas[arenanum];
	int		frames;

	// 10Hz server frames
	frames = arena->roundtimelimit * 10 - (level.framenum - arena->roundstart_framenum);
	if (frames <= 0)
		return 0;

	return (frames + 9) / 10;
}

/*
 * Sends the time remaining in the current fight to everyone in the arena,
 * once per second (or immediately if force is set).
 */
void show_roundtime (int arenanum, qboolean force)
{
	arena_t	*arena = &arenas[arenanum];
	edict_t	*e;
	char	*s;
	int		i, secs;

	if (!arena->roundtimelimit)
		return;

	secs = round_time_left (arenanum);
	if (!force && secs == arena->roundtime_sent)
		return;
	arena->roundtime_sent = secs;

	s = va ("%2d:%02d", secs / 60, secs % 60);

	for (i = 0; i < maxclients->value; i++)
	{
		e = &g_edicts[i + 1];
		if (e->inuse && e->client && e->client->resp.context == arenanum)
			send_configstring (e, CS_ROUNDTIME, s);
	}
}

/* gamex86.dll 0x20003780-0x200037a0 (bracketed) */
/* gamei386.so 0x0004aa64-0x0004aa75 */
int show_rank (qmenu_t *node)
{
	int		count;

	count = 0;
	for (node = node->prev; node; node = node->prev)
		count++;

	return count;
}

/* gamex86.dll 0x200037a0-0x20003810 (bracketed) */
/* gamei386.so 0x0004aa78-0x0004ab05 */
qboolean check_for_teams (int arenanum)
{
	qmenu_t	*tnode;
	int		i;


	if (count_queue (&arenas[arenanum].waitingteams) >= arenas[arenanum].numteams)
	{
		tnode = &arenas[arenanum].waitingteams;
		i = 0;
		while (tnode->next)
		{
			if (i >= arenas[arenanum].numteams)
				break;
			tnode = tnode->next;
			i++;
			if (count_queue ((qmenu_t *)tnode->it) == 0)
				return false;
		}

		return true;
	}

	return false;
}

/* gamex86.dll 0x20003810-0x200039d0 (unpadded-prologue) */
/* gamei386.so 0x0004ab08-0x0004ace7 */
int fill_arena (int arenanum)
{
	qmenu_t	*popped;
	int		count;
	int		firstskin;
	char	vs[256];

	firstskin = -1;
	vs[0] = 0;

	arenas[arenanum].sidepick = rand () % 2;

	for (count = 0; count < arenas[arenanum].numteams; count++)
	{
		popped = remove_from_queue (NULL, &arenas[arenanum].waitingteams);

		if (!popped)
		{
			gi.dprintf ("Team left during multi-round match\n");
			return 1;
		}

		if (firstskin == -1)
			firstskin = TEAM ((qmenu_t *)popped->it)->skin;
		else if (firstskin == TEAM ((qmenu_t *)popped->it)->skin)
		{
			gi.dprintf ("Skin conflict in arena %d\n", arenanum);
			TEAM ((qmenu_t *)popped->it)->skin = (firstskin + 1) % MAX_ARENA_SKINS;
		}

		SendTeamToArena ((qmenu_t *)popped->it, arenanum, false, true);

		if (count)
			strcat (vs, " vs ");
		strcat (vs, TEAM ((qmenu_t *)popped->it)->name);

		if (arenas[arenanum].round == 1)
			TEAM ((qmenu_t *)popped->it)->wins = 0;

		TEAM ((qmenu_t *)popped->it)->fighting = true;
	}

	strncpy (arenas[arenanum].vs, vs, sizeof (arenas[arenanum].vs) - 1);
	arenas[arenanum].vs[sizeof (arenas[arenanum].vs) - 1] = 0;
	gi.dprintf ("%d: %s\n", arenanum, arenas[arenanum].vs);

	return 1;
}

/* gamex86.dll 0x200039d0-0x20003a4e (manual-confirmed) */
/* gamei386.so 0x0004ace8-0x0004ad54 */
int fight_done (int arenanum)
{
	qmenu_t		*tnode, *mnode;
	edict_t		*e;
	int			winner;

	winner = -1;

	tnode = &arenas[arenanum].activeteams;

	while (tnode->next)
	{
		tnode = tnode->next;

		mnode = (qmenu_t *)tnode->it;

		while (mnode->next)
		{
			mnode = mnode->next;
			e = (edict_t *)mnode->it;

			if (e->takedamage != DAMAGE_AIM || e->deadflag != DEAD_NO)
				continue;

			if (winner == -1)
				winner = e->client->resp.teamnum;
			else if (winner != e->client->resp.teamnum)
				return -2;
		}
	}

	return winner;
}

/*
 * Decides a round that ran out of time: the team with the most combined
 * health + armor among its living players wins. Returns -1 on a tie.
 */
int health_winner (int arenanum)
{
	qmenu_t		*tnode, *mnode;
	edict_t		*e;
	int			winner, best, total;
	qboolean	tie;

	winner = -1;
	best = 0;
	tie = false;

	tnode = &arenas[arenanum].activeteams;

	while (tnode->next)
	{
		tnode = tnode->next;

		mnode = (qmenu_t *)tnode->it;
		total = 0;

		while (mnode->next)
		{
			mnode = mnode->next;
			e = (edict_t *)mnode->it;

			if (e->takedamage != DAMAGE_AIM || e->deadflag != DEAD_NO)
				continue;

			total += e->health;
			if (ArmorIndex (e))
				total += e->client->pers.inventory[ArmorIndex (e)];
		}

		if (total <= 0)
			continue;

		if (total > best)
		{
			best = total;
			winner = ((team_t *)((qmenu_t *)tnode->it)->it)->teamnum;
			tie = false;
		}
		else if (total == best)
			tie = true;
	}

	return tie ? -1 : winner;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so: no symbol -- inlined into its callers */
static void loc_buildboxpoints (vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs)
{
	VectorAdd (org, mins, p[0]);
	VectorCopy (p[0], p[1]);
	p[1][0] -= mins[0];
	VectorCopy (p[0], p[2]);
	p[2][1] -= mins[1];
	VectorCopy (p[0], p[3]);
	p[3][0] -= mins[0];
	p[3][1] -= mins[1];
	VectorAdd (org, maxs, p[4]);
	VectorCopy (p[4], p[5]);
	p[5][0] -= maxs[0];
	VectorCopy (p[0], p[6]);
	p[6][1] -= maxs[1];
	VectorCopy (p[0], p[7]);
	p[7][0] -= maxs[0];
	p[7][1] -= maxs[1];
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x0004ad54-0x0004af25 */
static qboolean loc_CanSee (edict_t *targ, edict_t *inflictor)
{
	trace_t	trace;
	vec3_t	targpoints[8];
	int		i;
	vec3_t	viewpoint;

// bmodels need special checking because their origin is 0,0,0
	if (targ->movetype == MOVETYPE_PUSH)
		return false;		// bmodels not supported

	loc_buildboxpoints (targpoints, targ->s.origin, targ->mins, targ->maxs);

	VectorCopy (inflictor->s.origin, viewpoint);
	viewpoint[2] += inflictor->viewheight;

	for (i = 0; i < 8; i++)
	{
		trace = gi.trace (viewpoint, vec3_origin, vec3_origin, targpoints[i], inflictor, MASK_SOLID);
		if (trace.fraction == 1.0)
			return true;
	}

	return false;
}

/* gamex86.dll 0x20003a50-0x20003b90 (manual-confirmed) */
/* gamei386.so 0x0004af28-0x0004b007 */
void CTFSetIDView (edict_t *ent)
{
	vec3_t		forward;
	trace_t		tr;

	ent->client->ps.stats[STAT_CTF_ID_VIEW] = 0;

	if (ent->client->resp.fightstate)
		return;

	if (ent->client->resp.track_target)
	{
		ent->client->ps.stats[STAT_CTF_ID_VIEW] = CS_PLAYERSKINS + (ent->client->resp.track_target - g_edicts) - 1;
		return;
	}

	AngleVectors (ent->client->v_angle, forward, NULL, NULL);
	VectorScale (forward, 1024, forward);
	VectorAdd (ent->s.origin, forward, forward);

	tr = gi.trace (ent->s.origin, NULL, NULL, forward, ent, CONTENTS_SOLID|CONTENTS_MONSTER);

	if (tr.fraction < 1 && tr.ent && tr.ent->client && tr.ent->solid)
		ent->client->ps.stats[STAT_CTF_ID_VIEW] = CS_PLAYERSKINS + (tr.ent - g_edicts) - 1;
}

/* gamex86.dll 0x20003b90-0x20003e00 (aligned) */
/* gamei386.so 0x0004b008-0x0004b4fa */
void UpdateStatusBars (int arenanum)
{
	qmenu_t	*tnode, *mnode;
	edict_t	*e;
	int		numteams;
	int		ti, i, y, n;
	int		membercount[MAX_STATUS_TEAMS];
	char	*teamname[MAX_STATUS_TEAMS];
	char	*names[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
	int		health[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
	char	*p;
	char	string[1400];

	tnode = &arenas[arenanum].activeteams;
	numteams = -1;
	while (tnode->next)
	{
		if (numteams >= MAX_STATUS_TEAMS)
			break;
		numteams++;
		tnode = tnode->next;

		mnode = (qmenu_t *)tnode->it;
		teamname[numteams] = ((team_t *)mnode->it)->name;
		membercount[numteams] = -1;

		while (mnode->next)
		{
			if (membercount[numteams] >= MAX_STATUS_MEMBERS - 1)
				break;
			mnode = mnode->next;

			e = (edict_t *)mnode->it;
			if (e->takedamage != DAMAGE_AIM)
				continue;
			if (e->deadflag != DEAD_NO)
				continue;

			membercount[numteams]++;
			names[numteams][membercount[numteams]] = e->client->pers.netname;
			health[numteams][membercount[numteams]] = e->health;
		}
	}

	y = 40;

	strcpy (string, "xl 8 yb -10 string2 \"Line Position:\" xl 100 yb -24 num 2 19 ");

	p = string + strlen (string);
	if (!arenas[arenanum].competition)
	{
		for (ti = 0; ti <= numteams; ti++)
		{
			sprintf (p, "xl %d yt %d string2 \"%s\" ", 8, y, teamname[ti]);
			p = string + strlen (string);
			y += 8;

			for (i = 0; i <= membercount[ti]; i++)
			{
				sprintf (p, "xl %d yt %d string2 \"%s: %d\" ", 8, y,
					names[ti][i], health[ti][i]);
				p = string + strlen (string);
				y += 8;
			}

			y += 8;
		}
	}

	strcpy (p, "if 20 xv 0 yb -58 stat_string 20 endif "
		"if 26 xr -42 yt 28 stat_string 26 endif ");

	for (n = 0; n < maxclients->value; n++)
	{
		e = &g_edicts[n + 1];

		if (!e->inuse || !e->client)
			continue;
		if (e->client->resp.context != arenanum)
			continue;
		if (e->client->resp.fightstate != FIGHT_SPECTATING)
			continue;
		if (e->client->showmenu)
			continue;

		if (e->client->resp.track_target || e->client->scoremode)
		{
			SendStatusBar (e, dm_statusbar, true);
		}
		else
		{
			e->client->ps.stats[STAT_LINEPOSITION] =
				show_rank (&((team_t *)teams[e->client->resp.teamnum].it)->arenalink);
			SendStatusBar (e, string, true);
		}
	}
}

/* gamex86.dll 0x20003e00-0x20003fe0 (aligned) */
/* gamei386.so 0x0004b4fc-0x0004b6b0 */
void check_telefrag (int arenanum)
{
	int			i;
	edict_t		*e;
	trace_t		tr;
	vec3_t		angles;
	vec3_t		forward;

	for (i = 0; i < maxclients->value; i++)
	{
		e = &g_edicts[i + 1];

		if (!e->inuse)
			continue;
		if (!e->client)
			continue;
		if (e->client->resp.context != arenanum)
			continue;
		if (!e->client->resp.fightstate)
			continue;
		if (!e->client->resp.spawn_recheck)
			continue;
		if (e->client->resp.spawn_recheck > level.framenum)
			continue;

		tr = gi.trace (e->s.origin, e->mins, e->maxs, e->s.origin, NULL, MASK_PLAYERSOLID);

		if (tr.contents == CONTENTS_SOLID)
		{
			e->solid = SOLID_NOT;

			angles[1] = rand () % 360;
			angles[0] = 0;
			angles[2] = 0;
			AngleVectors (angles, forward, NULL, NULL);
			VectorScale (forward, 600, forward);
			VectorAdd (e->velocity, forward, e->velocity);

			e->client->resp.spawn_recheck = level.framenum + 0.5 / FRAMETIME;
		}
		else
		{
			e->solid = SOLID_BBOX;

			gi.unlinkentity (e);
			KillBox (e);
			gi.linkentity (e);
		}
	}
}

/* gamex86.dll 0x20003fe0-0x20004140 (padded+majority) */
/* gamei386.so 0x0004b6b0-0x0004b82b */
void start_voting (edict_t *proposer, int arenanum)
{
	int			i;
	edict_t		*cl_ent;


	if (arenas[arenanum].state == ASTATE_FIGHTING || arenas[arenanum].state == ASTATE_COUNTDOWN)
		arenas[arenanum].proposetime = level.time + 30000;
	else
		arenas[arenanum].proposetime = level.time + 30;

	arenas[arenanum].votetries = arenas[arenanum].votes_no = arenas[arenanum].votes_yes = 0;
	arenas[arenanum].proposer = proposer;

	for (i = 0; i < maxclients->value; i++)
	{
		cl_ent = &g_edicts[i + 1];
		if (!cl_ent->inuse)
			continue;

		if (!cl_ent->client)
			continue;

		if (cl_ent->client->resp.context != arenanum)
			continue;

		cl_ent->client->resp.voted = false;
		arenas[arenanum].votetries++;

		if (cl_ent->client->resp.fightstate != FIGHT_SPECTATING)
			continue;

		if (cl_ent != proposer)
		{
			menu_centerprint (cl_ent, va ("Settings changes have been proposed\nby %s!\nGoto the observer menu (TAB) to vote",
				arenas[arenanum].proposer->client->pers.netname));
			stuffcmd (cl_ent, "play misc/pc_up.wav\n");
		}
		else
		{
			stuffcmd (cl_ent, "play misc/pc_up.wav\n");
		}
	}

	gi.dprintf ("Starting Voting in Arena %d with %d voters\n", arenanum, arenas[arenanum].votetries);
}

/* gamex86.dll 0x20004140-0x200042b0 (padded) */
/* gamei386.so 0x0004b82c-0x0004b9c2 */
void check_voting (int arenanum)
{
	int			i;
	edict_t		*cl_ent;
	char		msg[80];


	if (!arenas[arenanum].proposetime)
		return;

	if (arenas[arenanum].proposetime > level.time)
		return;

	arenas[arenanum].proposetime = 0;

	if (arenas[arenanum].votes_yes - arenas[arenanum].votes_no >= (float)arenas[arenanum].votetries * (1.0 / 3.0))
	{
		memcpy (&arenas[arenanum].playersperteam, &arenas[arenanum].proposed, sizeof (arena_settings_t));
		arenas[arenanum].changed = true;

		sprintf (msg, "Changes Passed! Yes votes: %d No votes: %d\n",
			arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
	}
	else
	{
		sprintf (msg, "Changes Failed! Yes votes: %d No votes: %d\n",
			arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
	}

	for (i = 0; i < maxclients->value; i++)
	{
		cl_ent = &g_edicts[i + 1];
		if (!cl_ent->inuse)
			continue;

		if (!cl_ent->client)
			continue;

		if (cl_ent->client->resp.context != arenanum)
			continue;

		gi.cprintf (cl_ent, PRINT_CHAT, msg);

		if (arenas[arenanum].changed)
			cl_ent->client->resp.votes = votetries_setting;
	}

	gi.dprintf (msg);

	check_teams (arenanum);
}

/* gamex86.dll 0x200042b0-0x20004457 (manual-confirmed) */
/* gamei386.so 0x0004b9c4-0x0004bba6 */
void set_server_bucket_info (int arenanum)
{
	void	*game = arenas[arenanum].statsptr;

	SETSTR (game, "hostname", hostname->string);
	SETSTR (game, "gamever", GAMEVERSION);
	SETSTR (game, "mapname", level.mapname);
	SETINT (game, "hostport", (int)hostport->value);
	SETINT (game, "arena", arenanum);
	SETINT (game, "rounds", arenas[arenanum].rounds);
	SETINT (game, "round", 1);
	SETINT (game, "armor", arenas[arenanum].armor);
	SETINT (game, "health", arenas[arenanum].health);
	SETINT (game, "armorprotect", arenas[arenanum].armorprotect);
	SETINT (game, "healthprotect", arenas[arenanum].healthprotect);
	SETINT (game, "fallingdamage", arenas[arenanum].fallingdamage);
	SETINT (game, "compmode", arenas[arenanum].competition);
	SETINT (game, "damagescoring", arenas[arenanum].scorebydamage);
}

/* gamex86.dll 0x20004460-0x20004a70 (manual-confirmed) */
/* gamei386.so 0x0004bba8-0x0004c4e8 */
void arena_think (int arenanum)
{
	int			winner;
	int			morewins;
	qmenu_t		*tnode, *popped;
	arena_t		*arena;


	arena = &arenas[arenanum];

	check_teams (arenanum);
	check_voting (arenanum);
	check_telefrag (arenanum);


	if (arena->state == ASTATE_COUNTDOWN || arena->state == ASTATE_WARMUP)
	{
		if (arena->countdown_next_tick == 0)
		{
			arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;

			if (arena->state == ASTATE_WARMUP)
				arena->countdown = 15;
			else if (arena->proposetime > level.time)
				arena->countdown = 10;
			else
				arena->countdown = 5;

			show_countdown (arena->countdown, arenanum);
			return;
		}

		if (arena->countdown_next_tick >= level.framenum)
			return;

		show_countdown (--arena->countdown, arenanum);

		if (arena->countdown == 0)
		{
			arena->countdown_next_tick = 0;

			if (arena->state == ASTATE_WARMUP)
			{
				if (!check_for_teams (arenanum))
				{
					arena->state = ASTATE_ROUNDEND;
					show_stringc ("Not enough teams to start", arenanum);
					return;
				}

				if (arena->idarena == 1)
				{
					if (arena->statsptr)
						FreeGame (arena->statsptr);
					if (netlog->string[0] && !IsStatsConnected ())
						InitStatsConnection ((int)hostport->value);
					arena->statsptr = NewGame (1);
					set_server_bucket_info (arenanum);
				}

				arena->state = ASTATE_COUNTDOWN;
				fill_arena (arenanum);
				return;
			}

			arena->state = ASTATE_FIGHTING;
			set_damage (arenanum, DAMAGE_AIM);
			arena->roundstart_framenum = level.framenum;
			arena->timed_out = false;
			show_roundtime (arenanum, true);
			return;
		}

		arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;
		return;
	}
	else if (arena->state == ASTATE_FIGHTING && !broken)
	{
		show_roundtime (arenanum, false);
		UpdateStatusBars (arenanum);

		if (fight_done (arenanum) <= -2)
		{
			if (!arena->roundtimelimit || round_time_left (arenanum) > 0)
				return;

			// out of time: decide on health + armor now, and stop the
			// fighting so the result can't change during the results pause
			arena->timeout_winner = health_winner (arenanum);
			arena->timed_out = true;
			set_damage (arenanum, DAMAGE_NO);
			show_stringc ("Time's up!", arenanum);
		}

		arena->state = ASTATE_RESULTS;
		return;
	}
	else if (arena->state == ASTATE_ROUNDEND)
	{
		if (!check_for_teams (arenanum))
			return;

		arena->round = 1;

		if (arenas[arenanum].idarena)
		{
			arena->state = ASTATE_WARMUP;
			return;
		}

		if (arena->idarena == 1)
		{
			if (arena->statsptr)
				FreeGame (arena->statsptr);
			if (netlog->string[0] && !IsStatsConnected ())
				InitStatsConnection ((int)hostport->value);
			arena->statsptr = NewGame (1);
			set_server_bucket_info (arenanum);
		}

		arena->state = ASTATE_COUNTDOWN;
		fill_arena (arenanum);
		return;
	}
	else if (arena->state == ASTATE_RESULTS)
	{
		UpdateStatusBars (arenanum);

		if (arena->countdown_next_tick == 0)
		{
			arena->countdown_next_tick = level.framenum + 3 / FRAMETIME;
			return;
		}

		if (arena->countdown_next_tick >= level.framenum)
			return;

		arena->state = ASTATE_NEXTROUND;
		arena->countdown_next_tick = 0;

		if (arena->proposetime - level.time <= 30)
			return;

		arena->proposetime = level.time + 30;
		return;
	}
	else if (arena->state == ASTATE_NEXTROUND)
	{
		if (arena->timed_out)
		{
			winner = arena->timeout_winner;
			arena->timed_out = false;
		}
		else
			winner = fight_done (arenanum);

		if (winner == -1)
		{
			sprintf (arena->msg, "It was a tie!");
			if (arena->statsptr)
				SendGameSnapShot (arena->statsptr, NULL, 0);
		}
		else
		{
			if (arena->statsptr)
				bopfuncs[BOP_TEAM_INT] (arena->statsptr, "score",
					bucketfuncs[BUCKET_ADD], 1, winner);

			if (++((team_t *)teams[winner].it)->wins > arenas[arenanum].rounds / 2)
			{
				Com_sprintf (arena->msg, sizeof (arena->msg), "%s has won the match!!",
					((team_t *)teams[winner].it)->name);
				arena->round = arena->rounds;

				if (arena->statsptr)
				{
					SendGameSnapShot (arena->statsptr, NULL, 1);
					FreeGame (arena->statsptr);
					arena->statsptr = NULL;
				}
			}
			else
			{
				Com_sprintf (arena->msg, sizeof (arena->msg), "%s has won the round!",
					((team_t *)teams[winner].it)->name);
				if (arena->statsptr)
					SendGameSnapShot (arena->statsptr, NULL, 0);
			}
		}

		if (winner == -1)
		{
			if (count_queue (&arenas[arenanum].activeteams) != 0)
			{
				if (count_queue ((qmenu_t *)arenas[arenanum].activeteams.next->it) != 0)
					arena->round--;
			}
		}

		gi.dprintf ("%d: %d %s\n", arenanum, winner, arena->msg);
		set_damage (arenanum, DAMAGE_NO);
		show_stringc (arena->msg, arenanum);
		tnode = &arenas[arenanum].activeteams;
		arenas[arenanum].sidepick = rand () % 2;

		while (tnode->next)
		{
			tnode = tnode->next;

			if (arena->round < arenas[arenanum].rounds)
			{
				morewins = arenas[arenanum].rounds / 2 + 1 - ((team_t *)((qmenu_t *)tnode->it)->it)->wins;
				Com_sprintf (arena->msg, sizeof (arena->msg), "%s has %d wins and needs %d more to take the match\n", ((team_t *)((qmenu_t *)tnode->it)->it)->name, ((team_t *)((qmenu_t *)tnode->it)->it)->wins, morewins);
				show_string (2, arena->msg, arenanum);
				SendTeamToArena ((qmenu_t *)tnode->it, arenanum, false, false);
			}
			else
			{
				popped = remove_from_queue (NULL, &arenas[arenanum].activeteams);
				((team_t *)((qmenu_t *)popped->it)->it)->fighting = false;
				tnode = &arenas[arenanum].activeteams;

				if (((team_t *)((qmenu_t *)popped->it)->it)->teamnum == winner || winner == -1)
					add_to_front_queue (popped, &arenas[arenanum].waitingteams);
				else
					add_to_queue (popped, &arenas[arenanum].waitingteams);
			}
		}

		if (arena->round < arenas[arenanum].rounds)
		{
			if (arena->statsptr)
				bopfuncs[BOP_SERVER_INT] (arena->statsptr, "round",
					bucketfuncs[BUCKET_ADD], 1, 0);

			arena->round++;
			arena->state = ASTATE_COUNTDOWN;
			return;
		}

		arena->state = ASTATE_ROUNDEND;
		return;
	}
}

/* gamex86.dll 0x20004a70-0x20004ac0 (shape-matched(ratio=0.87)) */
/* gamei386.so 0x0004c4e8-0x0004c532 */
void multi_arena_think (void)
{
	int		i;

	if (level.intermissiontime)
		return;

	i = level.framenum % (num_arenas * 2);
	if (i % 2)
		return;

	arena_think (i / 2 + 1);
}

/* gamex86.dll 0x20004ac0-0x20004d10 (padded+majority) */
/* gamei386.so 0x0004c534-0x0004c7e0 */
void arena_init (edict_t *wsent)
{
	int		i;
	team_t	*t;
	char	*name;

	if (!wsent)
		return;

	strcpy (gcd_gamename, "ra2");
	strcpy (gcd_secret_key, "9z3312");

	if (netlog->string[0] && !IsStatsConnected ())
		InitStatsConnection ((int)hostport->value);

	teams = gi.TagMalloc (MAX_TEAMS * sizeof (qmenu_t), TAG_LEVEL);
	memset (teams, 0, MAX_TEAMS * sizeof (qmenu_t));
	memset (arenas, 0, sizeof (arenas));

	admincode = gi.cvar ("admincode", "0", 0);

	num_arenas = wsent->arena;	//worldspawn arena flag is # of arenas
	if (!num_arenas)
	{
		num_arenas = 1;
		idmap = true;
	}
	else idmap = false;

	load_config (num_arenas + 1);
	set_config (1, num_arenas);

	for (i = 0; i <= num_arenas; i++)
	{
		arenas[i].state = ASTATE_ROUNDEND;
		arenas[i].statsptr = NULL;
		arenas[i].active = idmap;
		arenas[i].numteams = 2;
		arenas[i]._arena_unidentified0 = 0;
		arenas[i].waitingteams.prev = NULL;
		arenas[i].activeteams.prev = NULL;
		arenas[i].waitingteams.next = NULL;
		arenas[i].activeteams.next = NULL;
		arenas[i].countdown_next_tick = 0;
		arenas[i].countdown = 0;
		arenas[i].proposetime = 0;
		arenas[i].round = 0;

		if (!SelectFarthestArenaSpawnPoint ("misc_teleporter_dest", i))
		{
			gi.dprintf ("Setting arena %d to idarena mode\n", i);
			arenas[i].active = true;
		}

		if (i && arenas[i].idarena)
		{
			name = gi.TagMalloc (100, TAG_LEVEL);
			sprintf (name, "#%d Pickup Red", i);
			t = add_to_team (NULL, name);
			t->side = 0;
			SendTeamToArena (t->arenalink.it, i, true, true);
			arenas[i].pickupteam[0] = t;

			name = gi.TagMalloc (100, TAG_LEVEL);
			sprintf (name, "#%d Pickup Blue", i);
			t = add_to_team (NULL, name);
			t->side = 1;
			SendTeamToArena (t->arenalink.it, i, true, true);
			arenas[i].pickupteam[1] = t;

			arenas[i].maxteams = 2;
			arenas[i].playersperteam = 128;
		}
	}

	load_motd ();
}

/* gamex86.dll 0x20004d10-0x20004d60 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004c7e0-0x0004c830 */
void SP_trigger_teleport (edict_t *ent)
{
	ent->touch = teleporter_touch;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_TRIGGER;
	ent->use = NULL;
	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}

/* gamex86.dll 0x20004d60-0x20004d90 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004c830-0x0004c865 */
void SP_func_illusionary (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}

/* gamex86.dll 0x2001fe80-0x2001fe90 (manual-confirmed) */
/* gamei386.so 0x0004c868-0x0004c869 */
void SP_info_teleport_destination (edict_t *ent)
{
}

/* gamex86.dll 0x20004d90-0x20004db0 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004c86c-0x0004c88b */
void CTFPlayerResetGrapple (edict_t *ent)
{
	if (ent->client && ent->client->ctf_grapple)
		CTFResetGrapple (ent->client->ctf_grapple);
}

// self is the grapple hook itself, not the player
/* gamex86.dll 0x20004db0-0x20004e60 (padded) */
/* gamei386.so 0x0004c88c-0x0004c929 */
void CTFResetGrapple (edict_t *self)
{
	if (self->owner->client->ctf_grapple)
	{
		float		volume = 1.0;
		gclient_t	*cl;

		if (self->owner->client->silencer_shots)
			volume = 0.2;

		gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
			gi.soundindex ("weapons/grapple/grreset.wav"), volume, ATTN_NORM, 0);

		cl = self->owner->client;
		cl->ctf_grapple = NULL;
		cl->ctf_grapplereleasetime = level.time;
		cl->hookbutton = 0;
		cl->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY;
		cl->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

		G_FreeEdict (self);
	}
}

/* gamex86.dll 0x20004e60-0x20005000 (padded) */
/* gamei386.so 0x0004c92c-0x0004cb8b */
void CTFGrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	float	volume = 1.0;

	if (other == self->owner)
		return;

	if (self->owner->client->ctf_grapplestate != CTF_GRAPPLE_STATE_FLY)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		CTFResetGrapple (self);
		return;
	}

	VectorCopy (vec3_origin, self->velocity);

	PlayerNoise (self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage)
	{
		T_Damage (other, self, self->owner, self->velocity, self->s.origin,
			plane->normal, self->dmg, 1, 0, MOD_UNKNOWN);
		CTFResetGrapple (self);
		return;
	}

	self->owner->client->ctf_grapplestate = CTF_GRAPPLE_STATE_PULL;
	self->enemy = other;

	self->solid = SOLID_NOT;

	if (self->owner->client->silencer_shots)
		volume = 0.2;

	gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grpull.wav"), volume, ATTN_NORM, 0);
	gi.sound (self, CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grhit.wav"), volume, ATTN_NORM, 0);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPARKS);
	gi.WritePosition (self->s.origin);
	if (!plane)
		gi.WriteDir (vec3_origin);
	else
		gi.WriteDir (plane->normal);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

/* gamex86.dll 0x20005000-0x20005160 (shape-matched(ratio=0.98)) */
/* gamei386.so 0x0004cb8c-0x0004ccee */
void CTFGrappleDrawCable (edict_t *self)
{
	vec3_t	offset, start, end, f, r;
	vec3_t	dir;
	float	distance;

	AngleVectors (self->owner->client->v_angle, f, r, NULL);
	VectorSet (offset, 16, 16, self->owner->viewheight - 8);
	P_ProjectSource (self->owner->client, self->owner->s.origin, offset, f, r, start);

	VectorSubtract (start, self->owner->s.origin, offset);

	VectorSubtract (start, self->s.origin, dir);
	distance = VectorLength (dir);
	if (distance < 64)
		return;

	VectorCopy (self->s.origin, end);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GRAPPLE_CABLE);
	gi.WriteShort (self->owner - g_edicts);
	gi.WritePosition (self->owner->s.origin);
	gi.WritePosition (end);
	gi.WritePosition (offset);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

/* gamex86.dll 0x20005160-0x20005460 (manual-confirmed) */
/* gamei386.so 0x0004ccf0-0x0004d0a3 */
void CTFGrapplePull (edict_t *self)
{
	vec3_t	hookdir, v;
	float	vlen;

	if (self->enemy)
	{
		if (self->enemy->solid == SOLID_NOT)
		{
			CTFResetGrapple (self);
			return;
		}

		if (self->enemy->solid == SOLID_BBOX)
		{
			VectorScale (self->enemy->size, 0.5, v);
			VectorAdd (v, self->enemy->s.origin, v);
			VectorAdd (v, self->enemy->mins, self->s.origin);
			gi.linkentity (self);
		}
		else
			VectorCopy (self->enemy->velocity, self->velocity);

		if (self->enemy->takedamage && !CheckTeamDamage (self->enemy, self->owner))
		{
			float	volume = 1.0;

			if (self->owner->client->silencer_shots)
				volume = 0.2;

			T_Damage (self->enemy, self, self->owner, self->velocity, self->s.origin,
				vec3_origin, 1, 1, 0, MOD_UNKNOWN);
			gi.sound (self, CHAN_WEAPON,
				gi.soundindex ("weapons/grapple/grhurt.wav"), volume, ATTN_NORM, 0);
		}

		// he died
		if (self->enemy->deadflag)
		{
			CTFResetGrapple (self);
			return;
		}
	}

	CTFGrappleDrawCable (self);

	if (self->owner->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
	{
		vec3_t	forward, up;

		AngleVectors (self->owner->client->v_angle, forward, NULL, up);
		VectorCopy (self->owner->s.origin, v);
		v[2] += self->owner->viewheight;
		VectorSubtract (self->s.origin, v, hookdir);

		vlen = VectorLength (hookdir);

		if (self->owner->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL && vlen < 64)
		{
			float	volume = 1.0;

			if (self->owner->client->silencer_shots)
				volume = 0.2;

			self->owner->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
			gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
				gi.soundindex ("weapons/grapple/grhang.wav"), volume, ATTN_NORM, 0);
			self->owner->client->ctf_grapplestate = CTF_GRAPPLE_STATE_HANG;
		}

		VectorNormalize (hookdir);
		VectorScale (hookdir, CTF_GRAPPLE_PULL_SPEED, hookdir);
		VectorCopy (hookdir, self->owner->velocity);
		SV_AddGravity (self->owner);
	}
}

/* gamex86.dll 0x20005460-0x200055c0 (padded) */
/* gamei386.so 0x0004d0a4-0x0004d224 */
void CTFFireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
	edict_t	*grapple;
	trace_t	tr;

	VectorNormalize (dir);

	grapple = G_Spawn ();
	VectorCopy (start, grapple->s.origin);
	VectorCopy (start, grapple->s.old_origin);
	vectoangles (dir, grapple->s.angles);
	VectorScale (dir, speed, grapple->velocity);
	grapple->movetype = MOVETYPE_FLYMISSILE;
	grapple->clipmask = MASK_SHOT;
	grapple->solid = SOLID_BBOX;
	grapple->s.effects |= effect;
	VectorClear (grapple->mins);
	VectorClear (grapple->maxs);
	grapple->s.modelindex = gi.modelindex ("models/weapons/grapple/hook/tris.md2");
	grapple->owner = self;
	grapple->touch = CTFGrappleTouch;
	grapple->dmg = damage;
	self->client->ctf_grapple = grapple;
	self->client->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY;
	gi.linkentity (grapple);

	tr = gi.trace (self->s.origin, NULL, NULL, grapple->s.origin, grapple, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA (grapple->s.origin, -10, dir, grapple->s.origin);
		grapple->touch (grapple, tr.ent, NULL, NULL);
	}
}

/* gamex86.dll 0x200055c0-0x200056f0 (padded) */
/* gamei386.so 0x0004d224-0x0004d4af */
void CTFGrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;
	float	volume = 1.0;

	if (ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
		return;

	AngleVectors (ent->client->v_angle, forward, right, NULL);
	VectorSet (offset, 24, 8, ent->viewheight - 8 + 2);
	VectorAdd (offset, g_offset, offset);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	if (ent->client->silencer_shots)
		volume = 0.2;

	gi.sound (ent, CHAN_RELIABLE + CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grfire.wav"), volume, ATTN_NORM, 0);
	CTFFireGrapple (ent, start, forward, damage, CTF_GRAPPLE_SPEED, effect);

	PlayerNoise (ent, start, PNOISE_WEAPON);
}

/* gamex86.dll 0x200056f0-0x20005720 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004d4b0-0x0004d4cf */
void CTFWeapon_Grapple_Fire (edict_t *ent)
{
	int		damage;

	damage = 10;
	CTFGrappleFire (ent, vec3_origin, damage, 0);
	ent->client->ps.gunframe++;
}

/* gamex86.dll 0x20005720-0x20005830 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004d4d0-0x0004d667 */
void CTFWeapon_Grapple (edict_t *ent)
{
	static int	pause_frames[] = { 10, 18, 27, 0 };
	static int	fire_frames[] = { 6, 0 };
	int			prevstate;

	if ((ent->client->buttons & BUTTON_ATTACK) &&
		ent->client->weaponstate == WEAPON_FIRING &&
		ent->client->ctf_grapple)
		ent->client->ps.gunframe = 9;

	if (!(ent->client->buttons & BUTTON_ATTACK) && ent->client->ctf_grapple)
	{
		CTFResetGrapple (ent->client->ctf_grapple);
		if (ent->client->weaponstate == WEAPON_FIRING)
			ent->client->weaponstate = WEAPON_READY;
	}

	if (ent->client->newweapon &&
		ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY &&
		ent->client->weaponstate == WEAPON_FIRING)
	{
		ent->client->weaponstate = WEAPON_DROPPING;
		ent->client->ps.gunframe = 32;
	}

	prevstate = ent->client->weaponstate;
	Weapon_Generic (ent, 5, 9, 31, 36, pause_frames, fire_frames, CTFWeapon_Grapple_Fire);

	if (prevstate == WEAPON_ACTIVATING &&
		ent->client->weaponstate == WEAPON_READY &&
		ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
	{
		if (!(ent->client->buttons & BUTTON_ATTACK))
			ent->client->ps.gunframe = 9;
		else
			ent->client->ps.gunframe = 5;
		ent->client->weaponstate = WEAPON_FIRING;
	}
}
