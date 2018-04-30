#include "target.h"

// For DM_ARRAY_SIZE!
#include "libdevmapper.h"

#include <stdlib.h>

//----------------------------------------------------------------

static const char *_tok_cpy(const char *b, const char *e, const char *str)
{
	char *new = malloc((e - b) + 1);
	char *ptr = new;

	if (new)
        	while (b != e)
                	*ptr++ = *b++;

	return new;
}

static bool _tok_eq(const char *b, const char *e, const char *str)
{
	while (b != e) {
		if (!*str || *b != *str)
        		return false;

        	b++;
        	str++;
	}

	return !*str;
}

static bool _parse_operating_mode(const char *b, const char *e,
                                  enum vdo_operating_mode *r)
{
	static struct {
        	const char *str;
        	enum vdo_operating_mode mode;
	} _table[] = {
        	{"recovering", VDO_MODE_RECOVERING},
        	{"read-only", VDO_MODE_READ_ONLY},
        	{"normal", VDO_MODE_NORMAL}
	};

	unsigned i;
	for (i = 0; i < DM_ARRAY_SIZE(_table); i++) {
        	if (_tok_eq(b, e, _table[i].str)) {
                	*r = _table[i].mode;
                	return true;
        	}
	}

	return false;
}

static bool _parse_compression_state(const char *b, const char *e,
                                     enum vdo_compression_state *r)
{
	static struct {
        	const char *str;
        	enum vdo_compression_state state;
	} _table[] = {
        	{"online", VDO_COMPRESSION_ONLINE},
        	{"offline", VDO_COMPRESSION_OFFLINE}
	};

	unsigned i;
	for (i = 0; i < DM_ARRAY_SIZE(_table); i++) {
        	if (_tok_eq(b, e, _table[i].str)) {
                	*r = _table[i].state;
                	return true;
        	}
	}

	return false;
}

static bool _parse_recovering(const char *b, const char *e, bool *r)
{
	if (_tok_eq(b, e, "recovering"))
		*r = true;

	else if (_tok_eq(b, e, "-"))
        	*r = false;

        else
        	return false;

        return true;
}

static bool _parse_index_state(const char *b, const char *e,
                               enum vdo_index_state *r)
{
        static struct {
                const char *str;
                enum vdo_index_state state;
        } _table[] = {
                {"error", VDO_INDEX_ERROR},
                {"closed", VDO_INDEX_CLOSED},
                {"opening", VDO_INDEX_OPENING},
                {"closing", VDO_INDEX_CLOSING},
                {"offline", VDO_INDEX_OFFLINE},
                {"online", VDO_INDEX_ONLINE},
                {"unknown", VDO_INDEX_UNKNOWN}
        };

	unsigned i;
	for (i = 0; i < DM_ARRAY_SIZE(_table); i++) {
        	if (_tok_eq(b, e, _table[i].str)) {
                	*r = _table[i].state;
                	return true;
        	}
	}

        return false;
}

static const char *_eat_space(const char *b, const char *e)
{
	while (b != e && isspace(*b))
        	b++;

        return b;
}

static const char *_next_tok(const char *b, const char *e)
{
	while (b != e && !isspace(*b))
        	b++;

        return b == e ? NULL : b;
}

static bool _parse_field(const char **b, const char *e,
                         bool (*p_fn)(const char *, const char *, void *)
                         void *field, const char *field_name,
                         struct parse_result *result)
{
        const char *te;

        te = _next_tok(*b, e);
        if (!te) {
                snprintf(result->error, sizeof(result->error),
                         "couldn't get token for '%s'", field_name);
                return false;
        }

        if (!p_fn(*b, te, field) {
                snprintf(result->error, sizeof(result->error),
                         "couldn't parse '%s'", field_name);
                return false;
        }

	*b = _eat_space(te);
        return true;

}

bool parse_vdo_status(const char *input, struct parse_result *result)
{
	const char *b = _eat_space(input);
	const char *e = input + strlen(input);
	const char *te;
	struct vdo_status *s = malloc(sizeof(*s));

	if (!s) {
        	result->error = "out of memory";
        	return false;
	}

	te = _next_tok(b, e);
	if (!te) {
        	result->error = "couldn't get token for device";
        	free(s);
        	return false;
	}

	s->device = _tok_cpy(b, te);
	if (!s->device) {
		result->error = "out of memory";
		free(s);
		return false;
	}

	b = _eat_space(te);

#define XX(p, f, fn) if (!_parse_field(&b, e, p, f, fn, result)) goto bad;
	XX(_parse_recovering, &s->recovering, "recovering");
	XX(_parse_index, &s->index_state, "index state");
	XX(_parse_compression_state, &s->compression_state, "compression state");
	XX(_parse_uint64, &s->used_blocks, "used blocks");
	XX(_parse_uint64, &s->total_blocks, "total blocks");
#undef XX

        result->result = s;
        return true;

bad:
	free(s->device);
	free(s);
	return false;
}

//----------------------------------------------------------------