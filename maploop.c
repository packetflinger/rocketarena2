#include <sys/types.h>
#include <sys/stat.h>

#include "g_local.h"

#define	MAX_DEFS	256

typedef struct definition_s
{
	int				count;
	int				count2;
	char			*value;
	int				type;
	void			*value2;
} definition_t;

typedef struct
{
	int				count;
	definition_t	*cursor;
	definition_t	*item;
} blockstack_t;

definition_t	*find_key (char *key, int type, definition_t *items, int count);

static	cvar_t	*gamedir;
static	cvar_t	*arenacfg;
static	char	*line;

definition_t	*map_loop;
definition_t	*map_block;
definition_t	**arena_blocks;
int				votetries_setting = 3;
int				num_definition_blocks = 0;
definition_t	*definition_blocks;

int		weapon_vals[9] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
int		weapons;
int		armor;
int		health;
int		minping;
int		maxping;
int		playersperteam;
int		rounds;
int		max_teams;
int		pickup;
int		rocket_speed;
int		shells;
int		bullets;
int		slugs;
int		grenades;
int		rockets;
int		cells;
int		fastswitch;
int		armorprotect;
int		healthprotect;
int		fallingdamage;
int		roundtimelimit;
int		allow_voting_armor;
int		allow_voting_health;
int		allow_voting_minping;
int		allow_voting_maxping;
int		allow_voting_playersperteam;
int		allow_voting_rounds;
int		allow_voting_maxteams;
int		allow_voting_armorprotect;
int		allow_voting_healthprotect;
int		allow_voting_shotgun;
int		allow_voting_supershotgun;
int		allow_voting_machinegun;
int		allow_voting_chaingun;
int		allow_voting_grenadelauncher;
int		allow_voting_rocketlauncher;
int		allow_voting_hyperblaster;
int		allow_voting_railgun;
int		allow_voting_bfg;
int		allow_voting_fallingdamage;
int		lock_arena;
int		competition_mode;
int		damage_scoring;
qboolean	allow_grapple;

blockstack_t	stack[32];

/* gamex86.dll 0x2001ccb0-0x2001cd60 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004d668-0x0004d6d6 */
int has_val (char *str, char *key)
{
	char	buf[1024];
	char	*tok;

	strcpy (buf, str);
	tok = strtok (buf, " ");

	while (tok)
	{
		if (!strcmp (tok, key))
			return 1;

		tok = strtok (NULL, " ");
	}

	return 0;
}


/* gamex86.dll 0x2001cd60-0x2001ce30 (shape-matched(ratio=0.89)) */
/* gamei386.so 0x0004d6d8-0x0004d750 */
char *get_val (char *str, int index)
{
	static char	fnd[1024];
	char		buf[1024];
	char		*tok;

	strcpy (buf, str);
	tok = strtok (buf, " ");

	while (tok && index)
	{
		index--;
		tok = strtok (NULL, " ");
	}

	if (!tok)
		fnd[0] = 0;
	else
		strcpy (fnd, tok);

	return fnd;
}


/* gamex86.dll 0x2001ce30-0x2001d610 (padded+majority) */
/* gamei386.so 0x0004d750-0x0004f16d */
void get_settings (definition_t *items, int count)
{
	definition_t	*key;
	unsigned int	mask;
	int				i, n;

	key = find_key ("weapons", 1, items, count);
	if (key)
	{
		mask = 0;

		for (i = 0; i <= 8; i++)
		{
			if (i == 8)
				n = 0;
			else
				n = i + 2;

			if (has_val (key->value2, va ("%d", n)))
				mask |= weapon_vals[i];
		}

		weapons = mask;
	}

	key = find_key ("armor", 1, items, count);
	if (key)
		armor = atoi (get_val (key->value2, 0));

	key = find_key ("health", 1, items, count);
	if (key)
		health = atoi (get_val (key->value2, 0));

	key = find_key ("minping", 1, items, count);
	if (key)
		minping = atoi (get_val (key->value2, 0));

	key = find_key ("maxping", 1, items, count);
	if (key)
		maxping = atoi (get_val (key->value2, 0));

	key = find_key ("playersperteam", 1, items, count);
	if (key)
		playersperteam = atoi (get_val (key->value2, 0));

	key = find_key ("rounds", 1, items, count);
	if (key)
		rounds = atoi (get_val (key->value2, 0));

	key = find_key ("maxteams", 1, items, count);
	if (key)
		max_teams = atoi (get_val (key->value2, 0));

	key = find_key ("pickup", 1, items, count);
	if (key)
		pickup = atoi (get_val (key->value2, 0));

	key = find_key ("rocketspeed", 1, items, count);
	if (key)
		rocket_speed = atoi (get_val (key->value2, 0));

	key = find_key ("shells", 1, items, count);
	if (key)
		shells = atoi (get_val (key->value2, 0));

	key = find_key ("bullets", 1, items, count);
	if (key)
		bullets = atoi (get_val (key->value2, 0));

	key = find_key ("slugs", 1, items, count);
	if (key)
		slugs = atoi (get_val (key->value2, 0));

	key = find_key ("grenades", 1, items, count);
	if (key)
		grenades = atoi (get_val (key->value2, 0));

	key = find_key ("rockets", 1, items, count);
	if (key)
		rockets = atoi (get_val (key->value2, 0));

	key = find_key ("cells", 1, items, count);
	if (key)
		cells = atoi (get_val (key->value2, 0));

	key = find_key ("fastswitch", 1, items, count);
	if (key)
		fastswitch = atoi (get_val (key->value2, 0));

	key = find_key ("armorprotect", 1, items, count);
	if (key)
		armorprotect = atoi (get_val (key->value2, 0));

	key = find_key ("healthprotect", 1, items, count);
	if (key)
		healthprotect = atoi (get_val (key->value2, 0));

	key = find_key ("fallingdamage", 1, items, count);
	if (key)
		fallingdamage = atoi (get_val (key->value2, 0));

	key = find_key ("roundtimelimit", 1, items, count);
	if (key)
		roundtimelimit = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingarmor", 1, items, count);
	if (key)
		allow_voting_armor = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotinghealth", 1, items, count);
	if (key)
		allow_voting_health = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingminping", 1, items, count);
	if (key)
		allow_voting_minping = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingmaxping", 1, items, count);
	if (key)
		allow_voting_maxping = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingplayersperteam", 1, items, count);
	if (key)
		allow_voting_playersperteam = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingrounds", 1, items, count);
	if (key)
		allow_voting_rounds = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingmaxteams", 1, items, count);
	if (key)
		allow_voting_maxteams = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingarmorprotect", 1, items, count);
	if (key)
		allow_voting_armorprotect = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotinghealthprotect", 1, items, count);
	if (key)
		allow_voting_healthprotect = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingshotgun", 1, items, count);
	if (key)
		allow_voting_shotgun = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingsupershotgun", 1, items, count);
	if (key)
		allow_voting_supershotgun = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingmachinegun", 1, items, count);
	if (key)
		allow_voting_machinegun = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingchaingun", 1, items, count);
	if (key)
		allow_voting_chaingun = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotinggrenadelauncher", 1, items, count);
	if (key)
		allow_voting_grenadelauncher = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingrocketlauncher", 1, items, count);
	if (key)
		allow_voting_rocketlauncher = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotinghyperblaster", 1, items, count);
	if (key)
		allow_voting_hyperblaster = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingrailgun", 1, items, count);
	if (key)
		allow_voting_railgun = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingbfg", 1, items, count);
	if (key)
		allow_voting_bfg = atoi (get_val (key->value2, 0));

	key = find_key ("allowvotingfallingdamage", 1, items, count);
	if (key)
		allow_voting_fallingdamage = atoi (get_val (key->value2, 0));

	key = find_key ("lockarena", 1, items, count);
	if (key)
		lock_arena = atoi (get_val (key->value2, 0));

	key = find_key ("competitionmode", 1, items, count);
	if (key)
		competition_mode = atoi (get_val (key->value2, 0));

	key = find_key ("damagescoring", 1, items, count);
	if (key)
		damage_scoring = atoi (get_val (key->value2, 0));
}


/* gamex86.dll 0x2001d610-0x2001d980 (manual-confirmed) */
/* gamei386.so 0x0004f170-0x0004f5da */
void set_config (int first, int last)
{
	int		i;

	i = first;
	if (i > last)
		return;

	for (; i <= last; i++)
	{
		weapons = 0xff;
		armor = 200;
		health = 100;
		minping = 0;
		maxping = 1000;
		playersperteam = 1;
		if (!idmap)
			rounds = 1;
		else
			rounds = 9;
		max_teams = 128;
		if (!idmap)
			pickup = 0;
		else
			pickup = 1;
		rocket_speed = 650;
		shells = 100;
		bullets = 200;
		slugs = 50;
		grenades = 50;
		rockets = 50;
		cells = 150;
		fastswitch = 1;
		armorprotect = 2;
		healthprotect = 1;
		fallingdamage = 1;
		roundtimelimit = 180;
		allow_voting_armor = 1;
		allow_voting_health = 1;
		allow_voting_minping = 1;
		allow_voting_maxping = 1;
		allow_voting_playersperteam = 1;
		allow_voting_rounds = 1;
		allow_voting_maxteams = 1;
		allow_voting_armorprotect = 1;
		allow_voting_healthprotect = 1;
		allow_voting_shotgun = 1;
		allow_voting_supershotgun = 1;
		allow_voting_machinegun = 1;
		allow_voting_chaingun = 1;
		allow_voting_grenadelauncher = 1;
		allow_voting_rocketlauncher = 1;
		allow_voting_hyperblaster = 1;
		allow_voting_railgun = 1;
		allow_voting_bfg = 1;
		allow_voting_fallingdamage = 1;
		lock_arena = 0;
		competition_mode = 0;
		damage_scoring = 0;

		get_settings (definition_blocks, num_definition_blocks);

		if (map_block)
			get_settings (map_block->value2, map_block->count2);

		if (map_block && arena_blocks[i])
			get_settings (arena_blocks[i]->value2, arena_blocks[i]->count2);

		arenas[i].weapons = weapons;
		arenas[i].armor = armor;
		arenas[i].health = health;
		arenas[i].minping = minping;
		arenas[i].maxping = maxping;
		arenas[i].playersperteam = playersperteam;
		arenas[i].rounds = rounds;
		arenas[i].maxteams = max_teams;
		arenas[i].idarena = pickup;
		arenas[i].rocket_speed = rocket_speed;
		arenas[i].shells = shells;
		arenas[i].bullets = bullets;
		arenas[i].slugs = slugs;
		arenas[i].grenades = grenades;
		arenas[i].rockets = rockets;
		arenas[i].cells = cells;
		arenas[i].fastswitch = fastswitch;
		arenas[i].armorprotect = armorprotect;
		arenas[i].healthprotect = healthprotect;
		arenas[i].fallingdamage = fallingdamage;
		arenas[i].roundtimelimit = roundtimelimit < 0 ? 0 : roundtimelimit;
		arenas[i].allow_voting_armor = allow_voting_armor;
		arenas[i].allow_voting_health = allow_voting_health;
		arenas[i].allow_voting_minping = allow_voting_minping;
		arenas[i].allow_voting_maxping = allow_voting_maxping;
		arenas[i].allow_voting_playersperteam = allow_voting_playersperteam;
		arenas[i].allow_voting_rounds = allow_voting_rounds;
		arenas[i].allow_voting_maxteams = allow_voting_maxteams;
		arenas[i].allow_voting_armorprotect = allow_voting_armorprotect;
		arenas[i].allow_voting_healthprotect = allow_voting_healthprotect;
		arenas[i].allow_voting_shotgun = allow_voting_shotgun;
		arenas[i].allow_voting_supershotgun = allow_voting_supershotgun;
		arenas[i].allow_voting_machinegun = allow_voting_machinegun;
		arenas[i].allow_voting_chaingun = allow_voting_chaingun;
		arenas[i].allow_voting_grenadelauncher = allow_voting_grenadelauncher;
		arenas[i].allow_voting_rocketlauncher = allow_voting_rocketlauncher;
		arenas[i].allow_voting_hyperblaster = allow_voting_hyperblaster;
		arenas[i].allow_voting_railgun = allow_voting_railgun;
		arenas[i].allow_voting_bfg = allow_voting_bfg;
		arenas[i].allow_voting_fallingdamage = allow_voting_fallingdamage;
		arenas[i].locked = lock_arena;
		arenas[i].competition = competition_mode;
		arenas[i].scorebydamage = damage_scoring;
		arenas[i].changed = 0;
	}
}


/* gamex86.dll 0x2001d980-0x2001d9b0 (bracketed) */
/* gamei386.so 0x0004f5dc-0x0004f603 */
int ra_isalnum (char ch)
{
	int	c;

	c = ch;

	if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
		return 1;

	return 0;
}


/* gamex86.dll 0x2001d9b0-0x2001da80 (bracketed) */
/* gamei386.so 0x0004f604-0x0004f6ce */
char *next_token (char *str)
{
	static char	*token = NULL;
	static char	foo[1024];
	char		*out;
	char		c;

	if (str)
		token = str;
	else if (!token)
		return NULL;

	if (!*token || *token == '\n')
		return NULL;

	out = foo;

	if (!ra_isalnum (*token))
	{
		c = *out++ = *token++;

		if (*token == '/' && c == '/')
			*out++ = *token++;

		*out = 0;
		return foo;
	}

	while (ra_isalnum (*token))
		*out++ = *token++;
	*out = 0;
	return foo;
}


/* gamex86.dll 0x2001da80-0x2001daa0 (bracketed) */
/* gamei386.so 0x0004f6d0-0x0004f6e5 */
definition_t *new_def_block (void)
{
	return gi.TagMalloc (sizeof(definition_t) * MAX_DEFS, TAG_LEVEL);
}


/* gamex86.dll 0x2001daa0-0x2001dad0 (bracketed) */
/* gamei386.so 0x0004f6e8-0x0004f70e */
char *new_val_block (void)
{
	char	*buf;

	buf = gi.TagMalloc (0x400, TAG_LEVEL);
	sprintf (buf, "");

	return buf;
}


/* gamex86.dll 0x2001dad0-0x2001db30 (bracketed) */
/* gamei386.so 0x0004f710-0x0004f732 */
void add_val (char *dest, char *token)
{
	strcat (dest, " ");
	strcat (dest, token);
}


/* gamex86.dll 0x2001db30-0x2001db60 (bracketed) */
/* gamei386.so 0x0004f734-0x0004f777 */
definition_t *new_def_item (definition_t *item)
{
	item->count = 0;
	item->count2 = 0;
	item->value = new_val_block ();
	item->type = 0;

	return item;
}


/* gamex86.dll 0x2001db60-0x2001dd70 (padded) */
/* gamei386.so 0x0004f778-0x0004faac */
int read_block (FILE *fp, definition_t *cursor)
{
	definition_t	*cur;
	int				depth;
	int				mode;
	int				count;
	char			*tok;
	int				c;

	depth = 0;
	count = 0;

	cur = new_def_item (cursor++);
	mode = 0;

	while (1)
	{
		if (fscanf (fp, "%s", line) < 1)
		{
			if (depth)
			{
				gi.dprintf ("Error reading config file: unbalanced {}\n");
				return 0;
			}

			return count;
		}

		for (tok = next_token (line); tok; tok = next_token (NULL))
		{
			if (tok[0] == '/' && tok[1] == '/')
			{
				do
				{
					c = fgetc (fp);
					if (c < 1)
						return count;
				} while (c != '\n');

				break;
			}

			if (!mode)
			{
				if (*tok == '{')
				{
					cur->type = 2;
					cur->value2 = new_def_block ();

					stack[depth].count = count;
					stack[depth].cursor = cursor;
					stack[depth].item = cur;

					count = 0;
					cursor = cur->value2;
					cur = new_def_item (cursor++);
					depth++;
				}
				else if (*tok == ':')
				{
					cur->type = 1;
					cur->value2 = new_val_block ();
					mode = 1;
				}
				else if (*tok == '}')
				{
					if (!depth)
					{
						gi.dprintf ("Error reading config file: unbalanced {}\n");
						return 0;
					}

					depth--;
					cur = stack[depth].item;
					cur->count2 = count;
					cursor = stack[depth].cursor;
					count = stack[depth].count;

					cur = new_def_item (cursor++);
					mode = 0;
					count++;
				}
				else
				{
					add_val (cur->value, tok);
					cur->count++;
				}
			}
			else if (mode == 1)
			{
				if (*tok == ';')
				{
					cur = new_def_item (cursor++);
					mode = 0;
					count++;
				}
				else
				{
					add_val (cur->value2, tok);
					cur->count2++;
				}
			}
		}
	}
}


/* gamex86.dll 0x2001dd70-0x2001ddc0 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x0004faac-0x0004fb00 */
void read_config (FILE *fp)
{
	definition_blocks = new_def_block ();
	line = gi.TagMalloc (0x1000, TAG_LEVEL);
	num_definition_blocks = 0;
	num_definition_blocks = read_block (fp, definition_blocks);
}


/* gamex86.dll 0x2001ddc0-0x2001dec0 (manual-confirmed) */
/* gamei386.so 0x0004fb00-0x0004fc90 */
definition_t *find_key (char *key, int type, definition_t *items, int count)
{
	int		i;
	char	buf[1024];
	char	*tok;

	for (i = 0; i < count; i++)
	{
		if (items[i].type == type)
		{
			strcpy (buf, items[i].value);

			for (tok = strtok (buf, " "); tok; tok = strtok (NULL, " "))
				if (!strcmp (tok, key))
					return &items[i];
		}
	}

	return NULL;
}


/* gamex86.dll 0x2001dec0-0x2001e040 (unpadded-prologue) */
/* gamei386.so 0x0004fc90-0x0004ffec */
void list_keys (edict_t *ent)
{
	definition_t	*items;
	definition_t	*key;
	int				i;
	int				count;
	int				argc;
	char			path[1024];

	count = num_definition_blocks;
	items = definition_blocks;
	argc = gi.argc ();
	sprintf (path, "");

	for (i = 1; i < argc; i++)
	{
		key = find_key (gi.argv (i), 2, items, count);

		if (!key)
		{
			gi.cprintf (ent, PRINT_HIGH, "Block not found: %s\n", gi.argv (i));
			return;
		}

		items = key->value2;
		count = key->count2;
	}

	for (i = 0; i < count; i++)
	{
		strcat (path, items[i].value);
		strcat (path, "  ");

		if (items[i].type == 1)
			strcat (path, va ("V  %s\n", items[i].value2));
		else if (items[i].type == 2)
			strcat (path, "B\n");
		else
			strcat (path, "U\n");
	}

	gi.cprintf (ent, PRINT_HIGH, "%s", path);
}


/* gamex86.dll 0x2001e040-0x2001e290 (manual-confirmed) */
/* gamei386.so 0x0004ffec-0x000508f4 */
void load_config (int num_arenas)
{
	FILE			*fp;
	char			path[80];
	definition_t	*key, *block, *ablock;
	int				i;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	arenacfg = gi.cvar ("arenacfg", "arena.cfg", 0);

	strcpy (path, gamedir->string);
#ifdef _WIN32
	strcat (path, "\\");
#else
	strcat (path, "/");
#endif
	strcat (path, arenacfg->string);

	fp = fopen (path, "r");
	if (!fp)
	{
		gi.dprintf ("Error: Couldn't read %s\n", path);
		return;
	}

	read_config (fp);
	fclose (fp);

	arena_blocks = gi.TagMalloc (sizeof(definition_t *) * num_arenas, TAG_LEVEL);

	block = find_key (level.mapname, 2, definition_blocks, num_definition_blocks);

	if (block)
	{
		gi.dprintf ("arena.cfg info for map found: %s\n", level.mapname);
		map_block = block;

		for (i = 0; i < num_arenas; i++)
		{
			ablock = find_key (va ("%d", i), 2, block->value2, block->count2);
			arena_blocks[i] = ablock;
		}
	}
	else
	{
		gi.dprintf ("arena.cfg info for map not found: %s\n", level.mapname);
		map_block = 0;
	}

	key = find_key ("votetries", 1, definition_blocks, num_definition_blocks);
	if (key)
		votetries_setting = atoi (get_val (key->value2, 0));

	key = find_key ("grapple", 1, definition_blocks, num_definition_blocks);
	if (key)
		allow_grapple = atoi (get_val (key->value2, 0));

	map_loop = find_key ("maploop", 1, definition_blocks, num_definition_blocks);

	if (map_loop)
		gi.dprintf ("Map loop read\n");
}


/* gamex86.dll 0x2001e290-0x2001e380 (shape-matched(ratio=0.73)) */
/* gamei386.so 0x000508f4-0x00050b33 */
char *get_next_map (char *current)
{
	int		i;
	char	*val;

	if (!map_loop)
		return NULL;

	if (!has_val (map_loop->value2, current))
		return get_val (map_loop->value2, 0);

	for (i = 0; i < map_loop->count2; i++)
	{
		val = get_val (map_loop->value2, i);

		if (!strcmp (current, val))
		{
			val = get_val (map_loop->value2, i + 1);

			if (!strlen (val))
				return get_val (map_loop->value2, 0);

			return val;
		}
	}

	return level.mapname;
}


/* gamex86.dll 0x2001e380-0x2001e3c0 (padded) */
/* gamei386.so 0x00050b34-0x00050b6b */
void print_map_loop (edict_t *ent)
{
	if (map_loop)
		gi.cprintf (ent, PRINT_MEDIUM, "%s\n", map_loop->value2);
	else
		gi.cprintf (ent, PRINT_MEDIUM, "No map loop set\n");
}


/* gamex86.dll 0x2001e3c0-0x2001e555 (aligned-cross-object) */
/* gamei386.so 0x00050b6c-0x00050c9f */
void load_motd (void)
{
	FILE		*fp;
	struct stat	st;
	char		*buf;
	char		*p;
	motd_t		*node;
	char		path[80];

	motd.next = motd.prev = NULL;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);

	strcpy (path, gamedir->string);
#ifdef _WIN32
	strcat (path, "\\motd.txt");
#else
	strcat (path, "/motd.txt");
#endif

	fp = fopen (path, "r");
	if (!fp)
	{
		gi.dprintf ("Error: Couldn't read %s\n", path);
		return;
	}
	else
		gi.dprintf ("Sucessfully read %s\n", path);

#ifdef _WIN32
	fstat (fileno (fp), &st);

	buf = gi.TagMalloc (st.st_size + 2, TAG_LEVEL);
	if (!buf)
	{
		gi.dprintf ("Error: Couldn't malloc %d\n", (int)st.st_size);
		return;
	}
#else
	buf = gi.TagMalloc (2048, TAG_LEVEL);
#endif

	p = buf;
	while ((p = fgets (p, 99999, fp)) > 0)
	{
		if (p[strlen (p) - 1] == '\n')
			p[strlen (p) - 1] = 0;

		node = gi.TagMalloc (sizeof(motd_t), TAG_LEVEL);
		node->line = p;
		add_to_queue ((qmenu_t *)node, (qmenu_t *)&motd);

		p += strlen (p) + 1;
	}

	fclose (fp);
}
