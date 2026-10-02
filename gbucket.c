


























































#include <ctype.h>

#include "g_local.h"
#include "gbucket.h"
#include "nonport.h"

#define NBUCKETS	32

bucketset_t *g_buckets;

static void DumpMap (void *entry, void *userdata);
static void *DoSet (bucket_t *b, void *value);
static char *DoEscape (char *s);
static void *DoGet (bucket_t *b);
static bucket_t *DoFind (bucketset_t *set, char *key);
static int BucketHash (void *key, int nBuckets);
static int DoLower (char c);
static int BucketCompare (void *key1, void *key2);
static int BucketKeyCmp (void *key1, void *key2);
static void BucketFree (void *entry);

/* gamex86.dll 0x2001a980-0x2001a9c0 (manual-confirmed) */
/* gamei386.so 0x00055d70-0x00055dbf */
bucketset_t *NewBucketSet (void)
{
	bucketset_t	*set;
	set = malloc (sizeof(*set));
	assert (set);
	set->buckets = TableNew (sizeof(bucket_t), NBUCKETS, BucketHash, BucketCompare, BucketFree);
	g_buckets = set;
	return set;
}
/* gamex86.dll 0x2001a9c0-0x2001a9e0 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x00055dc0-0x00055e0d */
void FreeBucketSet (bucketset_t *set)
{
	assert (set);
	assert (set->buckets);
	TableFree (set->buckets);
	free (set);
}
#line 100
/* gamex86.dll 0x2001a9e0-0x2001aa40 (manual-confirmed) */
/* gamei386.so 0x00055e10-0x00055e7b */
char *DumpBucketSet (bucketset_t *set)
{
	dumpbuf_t	out;
	if (!set)
		set = g_buckets;
	assert (set);
	out.buf = malloc (DUMPBUF_START);
	out.buf[0] = 0;
	out.len = 0;
	out.size = DUMPBUF_START;
	TableMap (set->buckets, DumpMap, &out);
	return out.buf;
}
/* gamex86.dll 0x2001aa40-0x2001aab0 (manual-confirmed) */
/* gamei386.so 0x00055e7c-0x00055f02 */
void *BucketNew (bucketset_t *set, char *key, buckettype_t type, void *value)
{
	bucket_t	entry;
	if (!set)
		set = g_buckets;
	assert (set);
	entry.key = _strdup (key);
	entry.type = type;
	// clear the whole union: on 64-bit, sval is wider than ival and
	// DoSet would free() the uninitialized upper half
	memset (&entry.value, 0, sizeof(entry.value));
	entry.samples = 1;
	DoSet (&entry, value);
	TableEnter (set->buckets, &entry);
	return DoGet (DoFind (set, key));
}
/* gamex86.dll 0x2001aab0-0x2001aae0 (manual-confirmed) */
/* gamei386.so 0x00055f04-0x00055f38 */
void *BucketSet (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	b->samples = 0;
	return DoSet (b, value);
}
/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055f38-0x00055f51 */
void *BucketGet (bucketset_t *set, char *key)
{
	return DoGet (DoFind (set, key));
}
/* gamex86.dll 0x2001aae0-0x2001ab70 (manual-confirmed(shared DoGet/DoSet+bint/bfloat)) */
/* gamei386.so 0x00055f54-0x00055fe0 */
void *BucketAdd (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	if (b->type == bt_int)
		return DoSet (b, bint (*(int *)DoGet (b) + *(int *)value));
	else if (b->type == bt_float)
		return DoSet (b, bfloat ((*(double *)DoGet (b)) + (*(double *)value)));
	return BucketConcat (set, key, value);
}
/* gamex86.dll 0x2001ab70-0x2001abf0 (manual-confirmed(shared DoGet/DoSet+bint/bfloat)) */
/* gamei386.so 0x00055fe0-0x0005605c */
void *BucketSub (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	if (b->type == bt_int)
		return DoSet (b, bint (*(int *)DoGet (b) - *(int *)value));
	else if (b->type == bt_float)
		return DoSet (b, bfloat ((*(double *)DoGet (b)) - (*(double *)value)));
	return DoGet (b);
}
/* gamex86.dll 0x2001abf0-0x2001ac70 (manual-confirmed(shared DoGet/DoSet+bint/bfloat)) */
/* gamei386.so 0x0005605c-0x000560dc */
void *BucketMult (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	if (b->type == bt_int)
		return DoSet (b, bint (*(int *)DoGet (b) * *(int *)value));
	else if (b->type == bt_float)
		return DoSet (b, bfloat ((*(double *)DoGet (b)) * (*(double *)value)));
	return DoGet (b);
}
/* gamex86.dll 0x2001ac70-0x2001acf0 (manual-confirmed(shared DoGet/DoSet+bint/bfloat)) */
/* gamei386.so 0x000560dc-0x0005615c */
void *BucketDiv (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	if (b->type == bt_int)
		return DoSet (b, bint (*(int *)DoGet (b) / *(int *)value));
	else if (b->type == bt_float)
		return DoSet (b, bfloat ((*(double *)DoGet (b)) / (*(double *)value)));
	return DoGet (b);
}
#line 202
/* gamex86.dll 0x2001acf0-0x2001ada0 (manual-confirmed) */
/* gamei386.so 0x0005615c-0x00056204 */
void *BucketConcat (bucketset_t *set, char *key, void *value)
{
	bucket_t	*pbucket;
	char		*old, *joined;
	pbucket = DoFind (set, key);
	if (!pbucket)
		return NULL;
	assert (pbucket->type == bt_string);
	old = (char *)DoGet (pbucket);
	joined = malloc (strlen (old) + strlen ((char *)value) + 1);
	strcpy (joined, old);
	strcat (joined, (char *)value);
	DoSet (pbucket, joined);
	free (joined);
	return DoGet (pbucket);
}
/* gamex86.dll 0x2001ada0-0x2001ae4b (manual-confirmed(shared DoGet/DoSet+bint/bfloat)) */
/* gamei386.so 0x00056204-0x000562dc */
void *BucketAvg (bucketset_t *set, char *key, void *value)
{
	bucket_t	*b;
	b = DoFind (set, key);
	if (!b)
		return NULL;
	if (b->type == bt_int)
		return DoSet (b, bint ((*(int *)DoGet (b) * b->samples + *(int *)value) / ++b->samples));
	else if (b->type == bt_float)
		return DoSet (b, bfloat (((*(double *)DoGet (b)) * b->samples + (*(double *)value))
			/ ++b->samples));
	else
		return DoGet (b);
}
/* gamex86.dll 0x2001ae50-0x2001ae5f (manual-confirmed(name via gamei386.so)) */
/* gamei386.so 0x000562dc-0x000562eb */
void *bint (int value)
{
	static int	j;
	j = value;
	return &j;
}
/* gamex86.dll 0x2001ae60-0x2001ae79 (manual-confirmed(name via gamei386.so)) */
/* gamei386.so 0x000562ec-0x00056305 */
void *bfloat (double value)
{
	static double	g;
	g = value;
	return &g;
}
/* gamex86.dll 0x2001ae80-0x2001af30 (manual-confirmed) */
/* gamei386.so 0x00056308-0x000563aa */
static void DumpMap (void *entry, void *userdata)
{
	static char	formatspec[NUMBUCKETTYPES][7] =
	{
		"\\%s\\%d",
		"\\%s\\%f",
		"\\%s\\%s"
	};
	bucket_t	*b = (bucket_t *)entry;
	dumpbuf_t		*out = (dumpbuf_t *)userdata;
	unsigned int	need;
	need = strlen (b->key) + 3;
	if (b->type == bt_int || b->type == bt_float)
		need += out->len + 16;
	else if (b->type == bt_string)
		need += strlen (b->value.sval) + out->len;
	if (out->size <= need)
	{
		if (!out->size)
			out->size = need * 2;
		else
			out->size = out->size * 2;
		out->buf = realloc (out->buf, out->size);
	}
	// pass the union member matching the format; the original passed ival
	// for every type, which only worked on 32-bit
	if (b->type == bt_float)
		out->len += sprintf (out->buf + out->len, formatspec[b->type],
			b->key, b->value.dval);
	else if (b->type == bt_string)
		out->len += sprintf (out->buf + out->len, formatspec[b->type],
			b->key, b->value.sval);
	else
		out->len += sprintf (out->buf + out->len, formatspec[b->type],
			b->key, b->value.ival);
}
/* gamex86.dll 0x2001afc0-0x2001afe0 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static char *DoEscape (char *s)
{
	char	*result;
	result = s;
	while (*s)
	{
		if (*s == '\\')
			*s = '/';
		s++;
	}
	return result;
}
/* gamex86.dll 0x2001af30-0x2001afb7 (manual-confirmed(name via gamei386.so)) */
/* gamei386.so 0x000563ac-0x0005642b */
static void *DoSet (bucket_t *b, void *value)
{
	if (b->type == bt_int)
		b->value.ival = *(int *)value;
	else if (b->type == bt_float)
		b->value.dval = *(double *)value;
	else if (b->type == bt_string)
	{
		if (b->value.sval)
			free (b->value.sval);
		b->value.sval = (value == NULL ? NULL : DoEscape (_strdup ((char *)value)));
	}
	return DoGet (b);
}
/* gamex86.dll 0x2001afe0-0x2001aff7 (manual-confirmed(name via gamei386.so)) */
/* gamei386.so 0x0005642c-0x00056448 */
static void *DoGet (bucket_t *b)
{
	if (!b)
		return NULL;
	if (b->type == bt_string)
		return b->value.sval;
	return &b->value;
}
#line 323
/* gamex86.dll 0x2001b000-0x2001b030 (manual-confirmed) */
/* gamei386.so 0x00056448-0x00056491 */
static bucket_t *DoFind (bucketset_t *set, char *key)
{
	bucket_t	find;
	if (!set)
		set = g_buckets;
	assert (set);
	find.key = key;

	return (bucket_t *)TableLookup (set->buckets, &find);
}

/* gamex86.dll 0x2001b090-0x2001b0a5 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static int DoLower (char c)
{
	if (isascii (c))
#ifdef _WIN32
		return tolower (c);
#else
		return (char)tolower (c);
#endif

	return c;
}

/* gamex86.dll 0x2001b030-0x2001b090 (manual-confirmed(shared fold helper + strlen loop)) */
/* gamei386.so 0x00056494-0x0005650e */
static int BucketHash (void *key, int nBuckets)
{
	bucket_t		*b = (bucket_t *)key;
	char			*s = b->key;
	unsigned int	hash;
	unsigned int	i;

	hash = 0;
	for (i = 0; i < strlen (s); i++)
	{
		char	c = DoLower (s[i]);

		hash = hash * 0x9ccf9319u + c;
	}

	return hash % nBuckets;
}

/* gamex86.dll 0x2001b0d0-0x2001b0f0 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static int BucketKeyCmp (void *key1, void *key2)
{
	bucket_t	*b1 = (bucket_t *)key1;
	bucket_t	*b2 = (bucket_t *)key2;

	return strcasecmp (b1->key, b2->key);
}

/* gamex86.dll 0x2001b0b0-0x2001b0c3 (manual-confirmed(literal callee constant)) */
/* gamei386.so 0x00056510-0x00056527 */
static int BucketCompare (void *key1, void *key2)
{
	return BucketKeyCmp (key1, key2);
}

/* gamex86.dll 0x2001b0f0-0x2001b120 (manual-confirmed) */
/* gamei386.so 0x00056528-0x00056550 */
static void BucketFree (void *entry)
{
	bucket_t	*b = (bucket_t *)entry;

	free (b->key);
	if (b->type == bt_string && b->value.sval)
		free (b->value.sval);
}
