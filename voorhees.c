/*
 * Copyright (c) 2008 Emil Renner Berthing
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * The Software shall be used for Good, not Evil.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Based on the JSON_checker
 * Copyright (c) 2005 JSON.org
 * Same restrictions apply.
 */

#include <stdlib.h>
#include <strings.h>

#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>

#define DEFAULT_DEPTH 20
#define STRBUF_SIZE 1024

#define __   -1 /* universal error code */

/*
 * Characters are mapped into these character classes. This allows for
 * a significant reduction in the size of the state transition table.
 */
enum classes {
	C_SPACE,  /* space */
	C_WHITE,  /* other whitespace */
	C_LCURB,  /* {  */
	C_RCURB,  /* } */
	C_LSQRB,  /* [ */
	C_RSQRB,  /* ] */
	C_COLON,  /* : */
	C_COMMA,  /* , */
	C_QUOTE,  /* " */
	C_BACKS,  /* \ */
	C_SLASH,  /* / */
	C_PLUS,   /* + */
	C_MINUS,  /* - */
	C_POINT,  /* . */
	C_ZERO ,  /* 0 */
	C_DIGIT,  /* 123456789 */
	C_LOW_A,  /* a */
	C_LOW_B,  /* b */
	C_LOW_E,  /* e */
	C_LOW_F,  /* f */
	C_LOW_L,  /* l */
	C_LOW_N,  /* n */
	C_LOW_R,  /* r */
	C_LOW_S,  /* s */
	C_LOW_T,  /* t */
	C_LOW_U,  /* u */
	C_HEX,    /* cdABCDF */
	C_E,      /* E */
	C_ETC,    /* everything else */
	NR_CLASSES
};

/*
 * This array maps the first 126 ASCII characters into character classes
 * The remaining Unicode characters should be mapped to C_ETC
 * Non-whitespace control characters are errors
 */
static const signed char ascii_class[126] = {
	__,      __,      __,      __,      __,      __,      __,      __,
	__,      C_WHITE, C_WHITE, __,      __,      C_WHITE, __,      __,
	__,      __,      __,      __,      __,      __,      __,      __,
	__,      __,      __,      __,      __,      __,      __,      __,

	C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
	C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
	C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

	C_ETC,   C_HEX,   C_HEX,   C_HEX,   C_HEX,   C_E,     C_HEX,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

	C_ETC,   C_LOW_A, C_LOW_B, C_HEX,   C_HEX,   C_LOW_E, C_LOW_F, C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
	C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB
};

/*
 * The state codes
 */
enum states {
	GO,  /* start    */
	OK,  /* ok       */
	OB,  /* object   */
	KE,  /* key      */
	CO,  /* colon    */
	VA,  /* value    */
	A0,  /* empty array */
	AR,  /* array    */
	ST,  /* string   */
	ES,  /* escape   */
	U1,  /* u1       */
	U2,  /* u2       */
	U3,  /* u3       */
	U4,  /* u4       */
	L1,  /* l1       */
	L2,  /* l2       */
	MI,  /* minus    */
	ZE,  /* zero     */
	IT,  /* integer  */
	FP,  /* point    */
	FR,  /* fraction */
	E1,  /* e        */
	E2,  /* ex       */
	E3,  /* exp      */
	T1,  /* tr       */
	T2,  /* tru      */
	T3,  /* true     */
	F1,  /* fa       */
	F2,  /* fal      */
	F3,  /* fals     */
	F4,  /* false    */
	N1,  /* nu       */
	N2,  /* nul      */
	N3,  /* null     */
	NR_STATES
};

/*
 * The action codes. All negative
 */
enum actions {
	XO = -14, /* begin object     */
	XA = -13, /* begin array      */
	XS = -12, /* begin string     */
	ZQ = -11, /* end empty object */
	ZO = -10, /* end object       */
	Z0 =  -9, /* end empty array  */
	ZA =  -8, /* end array        */
	ZS =  -7, /* end string       */
	ZN =  -6, /* end number       */
	YE =  -5, /* escaped char     */
	YU =  -4, /* escaped unicode  */
	YN =  -3, /* comma            */
	YV =  -2  /* colon            */
};

/*
 * The state transition table takes the current state and the current symbol,
 * and returns either a new state or an action. An action is represented as a
 * negative number. A JSON text is accepted if at the end of the text the
 * state is OK and if the mode is MODE_DONE.
 */
static const signed char state_transition_table[NR_STATES][NR_CLASSES] = {
/*               white                                      1-9                           cdABCDF  etc
 *           space |  {  }  [  ]  :  ,  "  \  /  +  -  .  0  |  a  b  e  f  l  n  r  s  t  u  |  E  |*/
/*start  GO*/ {GO,GO,XO,__,XA,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ok     OK*/ {OK,OK,__,ZO,__,ZA,__,YN,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*object OB*/ {OB,OB,__,ZQ,__,__,__,__,XS,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*key    KE*/ {KE,KE,__,__,__,__,__,__,XS,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*colon  CO*/ {CO,CO,__,__,__,__,YV,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*value  VA*/ {VA,VA,XO,__,XA,__,__,__,XS,__,__,__,MI,__,ZE,IT,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*array  A0*/ {A0,A0,XO,__,XA,Z0,__,__,XS,__,__,__,MI,__,ZE,IT,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*array  AR*/ {AR,AR,XO,__,XA,ZA,__,__,XS,__,__,__,MI,__,ZE,IT,__,__,__,F1,__,N1,__,__,T1,__,__,__,__},
/*string ST*/ {ST,__,ST,ST,ST,ST,ST,ST,ZS,ES,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST,ST},
/*escape ES*/ {__,__,__,__,__,__,__,__,YE,YE,YE,__,__,__,__,__,__,YE,__,YE,__,YE,YE,__,YE,U1,__,__,__},
/*u1     U1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U2,U2,U2,U2,U2,U2,__,__,__,__,__,__,U2,U2,__},
/*u2     U2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U3,U3,U3,U3,U3,U3,__,__,__,__,__,__,U3,U3,__},
/*u3     U3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,U4,U4,U4,U4,U4,U4,__,__,__,__,__,__,U4,U4,__},
/*u4     U4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,YU,YU,YU,YU,YU,YU,__,__,__,__,__,__,YU,YU,__},
/*l1     L1*/ {__,__,__,__,__,__,__,__,__,L2,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*l2     L2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,U1,__,__,__},
/*minus  MI*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,ZE,IT,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*zero   ZE*/ {ZN,ZN,__,ZN,__,ZN,__,ZN,__,__,__,__,__,FP,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*int    IT*/ {ZN,ZN,__,ZN,__,ZN,__,ZN,__,__,__,__,__,FP,IT,IT,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*point  FP*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,FR,FR,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*frac   FR*/ {ZN,ZN,__,ZN,__,ZN,__,ZN,__,__,__,__,__,__,FR,FR,__,__,E1,__,__,__,__,__,__,__,__,E1,__},
/*e      E1*/ {__,__,__,__,__,__,__,__,__,__,__,E2,E2,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*ex     E2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*exp    E3*/ {ZN,ZN,__,ZN,__,ZN,__,ZN,__,__,__,__,__,__,E3,E3,__,__,__,__,__,__,__,__,__,__,__,__,__},
/*tr     T1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T2,__,__,__,__,__,__},
/*tru    T2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,T3,__,__,__},
/*true   T3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*fa     F1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F2,__,__,__,__,__,__,__,__,__,__,__,__},
/*fal    F2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F3,__,__,__,__,__,__,__,__},
/*fals   F3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,F4,__,__,__,__,__},
/*false  F4*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__,__,__},
/*nu     N1*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N2,__,__,__},
/*nul    N2*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,N3,__,__,__,__,__,__,__,__},
/*null   N3*/ {__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__,OK,__,__,__,__,__,__,__,__}
};

/*
 * These modes can be pushed on the stack
 */
enum modes {
	MODE_DONE,
	MODE_ARRAY,
	MODE_KEY,
	MODE_OBJECT
};

/*
 * Data needed by the getchar functions
 */
struct input {
	const unsigned char *p;
	size_t len;
	size_t read;
	int string_index;
};

/*
 * Buffer to store parsed strings and numbers
 */
struct strbuf {
	unsigned int parts;
	size_t written;
	char *p;
	char base[STRBUF_SIZE];
};

/*
 * Runs the Lua generator function to get another chunk
 * of the JSON document if one is provided
 */
static int getchunk(lua_State *L, struct input *in)
{
	if (in->string_index == 0)
		return 1;

	lua_pushvalue(L, 1);
	if (lua_pcall(L, 0, 1, 0)) {
		lua_pop(L, 1);
		return -1;
	}

	lua_replace(L, in->string_index);

	in->p = (unsigned char *)lua_tolstring(L, in->string_index, &in->len);
	if (in->p == NULL || in->len == 0)
		return 1;

	return 0;
}

/*
 * Type of a pointer to a getchar function
 */
typedef int (*getchar_func)(lua_State *L, struct input *in);

/*
 * Lookup table to determine the number of bytes following
 * the first of a multibyte UTF-8 character
 */
static const char utf8_trailing_bytes[128] = {
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Lookup table to get a mask for the important suffix
 * of the first of a multibyte UTF-8 character
 */
static const int utf8_mask[4] = { 0, 31, 15, 7 };

/*
 * This function reads a UTF-8 encoded character from the input
 */
static int utf8_getchar(lua_State *L, struct input *in)
{
	unsigned int trailing_bytes;
	int c;

	if (in->len == 0 && getchunk(L, in)) {
		return 0;
	}

	c = *in->p++;
	in->len--;
	in->read++;

	/* Is this a multibyte (non-ASCII) character? */
	if (c & 128) {
		trailing_bytes = utf8_trailing_bytes[c & 127];
		if (trailing_bytes == 0) {
			return -1;
		}

		c &= utf8_mask[trailing_bytes];

		do {
			char b;

			if (in->len == 0 && getchunk(L, in)) {
				return -1;
			}

			b = *in->p++;
			in->len--;
			in->read++;

			if ((b & 0xC0) != 0x80) {
				return -1;
			}

			c <<= 6;
			c |= (b & 63);

			trailing_bytes--;
		} while (trailing_bytes);
	}

	return c;
}

/*
 * This function reads a little endian UTF-16 encoded character
 * from the input
 */
static int utf16le_getchar(lua_State *L, struct input *in)
{
	int high_sur;
	int c;

	if (in->len == 0 && getchunk(L, in)) {
		return 0;
	}

	c = *in->p++;
	in->len--;

	if (in->len == 0 && getchunk(L, in)) {
		return -1;
	}

	c |= (*in->p++ << 8);
	in->len--;
	in->read += 2;

	if (c >= 0xD800 && c < 0xE000) {
		if (c >= 0xDC00) {
			return -1;
		}

		high_sur = c;

		if (in->len == 0 && getchunk(L, in)) {
			return -1;
		}
		c = *in->p++;
		in->len--;

		if (in->len == 0 && getchunk(L, in)) {
			return -1;
		}
		c |= (*in->p++ << 8);
		in->len--;

		if (c < 0xDC00 || c >= 0xE000) {
			return -1;
		}

		c &= 1023;
		c |= (high_sur & 1023) << 16;
		c |= 0x10000;
	}

	return c;
}

/*
 * This function reads a big endian UTF-16 encoded character
 * from the input
 */
static int utf16be_getchar(lua_State *L, struct input *in)
{
	int high_sur;
	int c;

	if (in->len == 0 && getchunk(L, in)) {
		return 0;
	}

	c = *in->p++ << 8;
	in->len--;

	if (in->len == 0 && getchunk(L, in)) {
		return -1;
	}

	c |= *in->p++;
	in->len--;
	in->read += 2;

	if (c >= 0xD800 && c < 0xE000) {
		if (c >= 0xDC00) {
			return -1;
		}

		high_sur = c;

		if (in->len == 0 && getchunk(L, in)) {
			return -1;
		}
		c = (*in->p++ << 8);
		in->len--;

		if (in->len == 0 && getchunk(L, in)) {
			return -1;
		}
		c |= *in->p++;
		in->len--;

		if (c < 0xDC00 || c >= 0xE000) {
			return -1;
		}

		c &= 1023;
		c |= (high_sur & 1023) << 16;
		c |= 0x10000;
	}

	return c;
}

/*
 * This function is responsible for detecting the encoding
 * of the input and return the right getchar function
 */
static getchar_func detect_encoding(struct input *in)
{
	/* Only UTF-8 encoded JSON texts can be
	 * that short and legal */
	if (in->len < 4)
		return utf8_getchar;

	/* Check for UTF-16 byte order marks */
	if (in->p[0] == 0xFF && in->p[1] == 0xFE) {
		in->p += 2;
		in->len -= 2;
		in->read += 2;
		return utf16le_getchar;
	}
	if (in->p[0] == 0xFE && in->p[1] == 0xFF) {
		in->p += 2;
		in->len -= 2;
		in->read += 2;
		return utf16be_getchar;
	}
	
	/* Check for UTF-8 marker */
	if (in->p[0] == 0xEF &&
			in->p[1] == 0xBB &&
			in->p[2] == 0xBF) {
		in->p += 3;
		in->len -= 3;
		in->read += 3;
		return utf8_getchar;
	}

	/* First two characters of a legal JSON text will always be
	 * a non-zero ASCII character, so we can check for UTF-16
	 * BE or LE like this */
	if (in->p[1] == 0 && in->p[3] == 0)
		return utf16le_getchar;
	if (in->p[0] == 0 && in->p[2] == 0)
		return utf16be_getchar;

	/* Fall back on UTF-8 */
	return utf8_getchar;
}


/*
 * Type of a pointer to a getchar function
 */
typedef void (*putchar_func)(struct strbuf *s, int c);

/*
 * This function writes a UTF-8 encoded Unicode character
 * to the string buffer
 */
static void utf8_putchar(struct strbuf *s, int c)
{
	if (c < 0x80) {
		*s->p++ = (char)c;
		s->written++;
	} else if (c < 0x800) {
		*s->p++ = (char)(192 | (c >> 6));
		*s->p++ = (char)(128 | (c & 63));
		s->written += 2;
	} else if (c < 0x10000) {
		*s->p++ = (char)(224 | (c >> 12));
		*s->p++ = (char)(128 | ((c >> 6) & 63));
		*s->p++ = (char)(128 | (c & 63));
		s->written += 3;
	} else /* if (c < 0x200000) */ {
		*s->p++ = (char)(240 | (c >> 18));
		*s->p++ = (char)(128 | ((c >> 12) & 63));
		*s->p++ = (char)(128 | ((c >> 6) & 63));
		*s->p++ = (char)(128 | (c & 63));
		s->written += 4;
	}
}

/*
 * This function writes a little endian UTF-16 encoded Unicode
 * character to the string buffer
 */
static void utf16le_putchar (struct strbuf *s, int c)
{
	if (c < 0x10000) {
		*s->p++ = (char)(c & 255);
		*s->p++ = (char)(c >> 8);
		s->written += 2;
	} else {
		c &= 0x10000;
		*s->p++ = (char)((c >> 10) & 255);
		*s->p++ = (char)(0xD8 | ((c >> 18) & 3));
		*s->p++ = (char)(c & 255);
		*s->p++ = (char)(0xDC | ((c >>  8) & 3));
		s->written += 4;
	}
}

/*
 * This function writes code points less than 256
 * in plain latin-1 to the string buffer.
 * All other Unicode is mapped to '?'
 */
static void latin1_putchar(struct strbuf *s, int c)
{
	if (c < 256) {
		*s->p++ = (char)c;
	} else {
		*s->p++ = '?';
	}

	s->written++;
}

/*
 * This macro pushes the contents of the string buffer
 * to the Lua stack. All the pieces will be
 * lua_concat()'ed in the end.
 */
#define flush_buffer() \
	luaL_checkstack(L, 1, "out of memory"); \
	lua_pushlstring(L, s.base, s.written); \
	r++; \
	s.parts++; \
	s.written = 0; \
	s.p = s.base

/*
 * This is the parse function exported to Lua
 *
 * The first part reads the arguments provided
 * and initialises the input, string buffer
 * and putchar and getchar functions accordingly
 *
 * The middle loop reads a character from the input
 * and looks up the next state or action in the state
 * table and performs the corresponding actions
 * until the JSON document is finished or a
 * syntax or encoding error occurs
 *
 * The last part checks if everything went alright,
 * frees the stack and returns the parsed array
 * or object table
 */
static int l_parse(lua_State *L)
{
	struct input in;
	struct strbuf s;
	int next_char;
	signed char *stack;
	unsigned int top = 0;
	unsigned int depth = DEFAULT_DEPTH;
	signed char state = GO;
	int null_index = lua_upvalueindex(1);
	int high_sur = 0;
	int unicode = 0;
	int r = 0;
	putchar_func putchar = utf8_putchar;
	getchar_func getchar;
	int nargs = lua_gettop(L);

	/*
	 * Part 1: Read Lua arguments and initialise
	 * variables
	 */
	if (nargs < 1) {
		return luaL_error(L, "too few arguments");
	}

	in.read = 0;
	switch (lua_type(L, 1)) {
	case LUA_TSTRING:
		in.p = (unsigned char *)lua_tolstring(L, 1, &in.len);
		if (in.p == NULL || in.len < 2) {
			lua_pushnil(L);
			lua_pushliteral(L, "string too short");
			return 2;
		}
		in.string_index = 0;
		break;
	case LUA_TFUNCTION:
		lua_pushvalue(L, 1);
		if (lua_pcall(L, 0, 1, 0)) {
			lua_pushnil(L);
			lua_insert(L, -2);
			return 2;
		}

		in.p = (unsigned char *)lua_tolstring(L, -1, &in.len);
		if (in.p == NULL || in.len == 0) {
			lua_pushnil(L);
			lua_pushliteral(L, "string too short");
			return 2;
		}

		in.string_index = lua_gettop(L);

		/* Make sure the first chunk is at least 4
		 * characters long if possible */
		while (in.len < 4) {
			lua_pushvalue(L, 1);
			if (lua_pcall(L, 0, 1, 0) ||
					!lua_isstring(L, -1)) {
				in.string_index = 0;
				lua_pop(L, 1);
				break;
			}
			/* Even if the user is silly enough to pass only
			 * 1 byte at a time we'll only concat strings
			 * 3 times in this loop */
			lua_concat(L, 2);
			in.p = (unsigned char *)lua_tolstring(L, -1, &in.len);
		}
		break;
	default:
		return luaL_argerror(L, 1, "expected string or function");
	}

	getchar = detect_encoding(&in);
	
	if (nargs >= 2) {
		const char *str = lua_tostring(L, 2);
		if (str == NULL) {
			return luaL_argerror(L, 2,
					"encoding must be a string");
		}
		if (strcasecmp(str, "utf8") == 0) {
			putchar = utf8_putchar;
		} else if (strcasecmp(str, "utf16") == 0 ||
				strcasecmp(str, "utf16le") == 0) {
			putchar = utf16le_putchar;
		} else if (strcasecmp(str, "latin1") == 0) {
			putchar = latin1_putchar;
		} else {
			return luaL_error(L,
					"bad argument #2 "
					"(unknown encoding '%s')",
					str);
		}
	}

	if (nargs >= 3) {
		depth = (int)lua_tonumber(L, 3);
		if (depth < 1) {
			return luaL_argerror(L, 3, "depth must be 1 or greater");
		}
	}

	/* Expand the Lua stack size so we have room enough to parse
	 * the JSON documents of maximum depth */
	luaL_checkstack(L, depth + 1, "out of memory");

	/* Allocate memory form the stack */
	stack = malloc(depth);
	if (stack == NULL) {
		return luaL_error(L, "out of memory");
	}
	stack[0] = MODE_DONE;

	if (nargs >= 4)
		null_index = 4;

	/* Initialise the string buffer */
	s.parts = 0;
	s.written = 0;
	s.p = s.base;


	/*
	 * Part 2: The parsing loop
	 */
	while ((next_char = getchar(L, &in)) > 0) {
		signed char next_class;
		
		/* Determine the character's class. */
		if (next_char >= 126) {
			next_class = C_ETC;
		} else {
			next_class = ascii_class[next_char];
			if (next_class <= __) {
				goto syntax_error;
			}
		}

again:
		/* Get the next state/action from the state transition table */
		state = state_transition_table[state][next_class];

		/* Perform actions according to the state/action */
		switch (state) {
		case N1:
			lua_pushvalue(L, null_index);
			r++;
			break;

		case T1:
			lua_pushboolean(L, 1);
			r++;
			break;

		case F1:
			lua_pushboolean(L, 0);
			r++;
			break;

		case MI:
		case ZE:
		case IT:
		case FP:
		case FR:
		case E1:
		case E2:
		case E3:
			*s.p++ = (char)next_char;
			s.written++;
			if (s.written == STRBUF_SIZE) {
				flush_buffer();
			}
			break;

		case XS: /* begin string */
			/* this is a special action, so we
			 * don't push the beginning " */
			state = ST;
			break;

		case ST:
			putchar(&s, next_char);
			if (s.written >= (STRBUF_SIZE - 4)) {
				flush_buffer();
			}
			break;

		case YE: /* put an escaped character */
			switch (next_char) {
			case 'b':
				putchar(&s, '\b');
				break;
			case 'f':
				putchar(&s, '\f');
				break;
			case 'n':
				putchar(&s, '\n');
				break;
			case 'r':
				putchar(&s, '\r');
				break;
			case 't':
				putchar(&s, '\t');
				break;
			default:
				putchar(&s, next_char);
			}
			if (s.written >= (STRBUF_SIZE - 4)) {
				flush_buffer();
			}
			state = ST;
			break;

		case U3:
		case U4:
			unicode <<= 4;
		case U2:
			if (next_char <= '9') {
				unicode |= (next_char - '0');
			} else if (next_char <= 'F') {
				unicode |= next_char - 55;
			} else {
				unicode |= next_char - 87;
			}
			break;

		case YU: /* write the escaped unicode character */
			/* Read the last hex char */
			unicode <<= 4;
			if (next_char <= '9') {
				unicode |= next_char - '0';
			} else if (next_char <= 'F') {
				unicode |= next_char - 55;
			} else {
				unicode |= next_char - 87;
			}
			/* If its a high surrogate.. */
			if (unicode >= 0xD800 && unicode < 0xDC00) {
				high_sur = unicode;
				state = L1; /* ..the low must follow */
			} else {
				/* Should this be a low surrogate? */
				if (high_sur) { 
					if (unicode < 0xDC00 ||
							unicode >= 0xE000)
						goto syntax_error;

					unicode &= 1023;
					unicode |= (high_sur & 1023) << 10;
					unicode |= 0x10000;
					high_sur = 0;
				}
				putchar(&s, unicode);
				if (s.written >= (STRBUF_SIZE - 4)) {
					flush_buffer();
				}
				state = ST;
			}
			unicode = 0;
			break;

		case XA: /* begin array */
			top++;
			if (top == depth) {
				goto stack_overflow;
			}
			stack[top] = MODE_ARRAY;
			lua_newtable(L);
			r++;
			state = A0;
			break;

		case XO: /* begin object */
			top++;
			if (top == depth) {
				goto stack_overflow;
			}
			stack[top] = MODE_KEY;
			lua_newtable(L);
			r++;
			state = OB;
			break;

		case ZN: /* end number */
			if (s.written) {
				flush_buffer();
			}
			lua_concat(L, s.parts);
			r += 1 - s.parts;
			s.parts = 0;
			lua_pushnumber(L, lua_tonumber(L, -1));
			lua_remove(L, -2);
			state = OK;
			goto again;

		case ZS: /* end string */
			if (s.written) {
				flush_buffer();
			}
			lua_concat(L, s.parts);
			r += 1 - s.parts;
			s.parts = 0;
			switch (stack[top]) {
			case MODE_KEY:
				state = CO;
				break;
			case MODE_ARRAY:
			case MODE_OBJECT:
				state = OK;
				break;
			default:
				goto syntax_error;
			}
			break;

		case Z0: /* end empty array */
			if (stack[top] != MODE_ARRAY) {
				goto syntax_error;
			}
			top--;
			state = OK;
			break;

		case ZA: /* end array */
			if (stack[top] != MODE_ARRAY) {
				goto syntax_error;
			}
			top--;
			lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
			r--;
			state = OK;
			break;

		case ZQ: /* end empty object */
			if (stack[top] != MODE_KEY) {
				goto syntax_error;
			}
			top--;
			state = OK;
			break;

		case ZO: /* end object */
			if (stack[top] != MODE_OBJECT) {
				goto syntax_error;
			}
			top--;
			lua_rawset(L, -3);
			r -= 2;
			state = OK;
			break;

		case YN: /* next key/value pair or array entry */
			switch (stack[top]) {
			case MODE_OBJECT:
				/* A comma causes a flip from
				 * object mode to key mode. */
				if (stack[top] != MODE_OBJECT) {
					goto syntax_error;
				}
				stack[top] = MODE_KEY;
				lua_rawset(L, -3);
				r -= 2;
				state = KE;
				break;
			case MODE_ARRAY:
				lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
				r--;
				state = VA;
				break;
			default:
				goto syntax_error;
			}
			break;

		case YV: /* key read, now read the value */
			/* A colon causes a flip from key mode
			 * to object mode. */
			if (stack[top] != MODE_KEY) {
				goto syntax_error;
			}
			stack[top] = MODE_OBJECT;
			state = VA;
			break;

		case __: /* bad state */
			goto syntax_error;
		}
	}

	/*
	 * Part 3: Check everything went alright,
	 * free the stack and return
	 */

	/* Did we encounter an encoding error? */
	if (next_char < 0) {
		lua_pop(L, r);
		free(stack);
		lua_pushnil(L);
		lua_pushfstring(L, "encoding error after %d bytes",
				(int)in.read);
		return 2;
	}

	/* Check if the JSON text finish properly */
	if (state != OK || stack[top] != MODE_DONE) {
		goto syntax_error;
	}

	free(stack);

	/* If this fails we did something wrong */
	if (r != 1) {
		lua_pop(L, r);
		lua_pushnil(L);
		lua_pushfstring(L, "r = %d", r);
		return 2;
	}

	return 1;

	/*
	 * We jump to here in case of errors encountered
	 * while parsing
	 */
syntax_error:
	lua_pop(L, r);
	free(stack);
	lua_pushnil(L);
	lua_pushfstring(L, "syntax error after %d bytes", (int)in.read);
	return 2;

stack_overflow:
	lua_pop(L, r);
	free(stack);
	lua_pushnil(L);
	lua_pushliteral(L, "stack overflow");
	return 2;
}

/*
 * This function is run when the library is loaded
 * Usually by the require function in Lua
 * It returns the module which is just a table with
 * the parse function and the default null value
 */
LUALIB_API int luaopen_voorhees(lua_State *L)
{
	/* Create new module table */
	lua_newtable(L);

	/* Insert null value */
	(void)luaL_dostring(L, "local function f() return f end return f");
	lua_pushvalue(L, 3);
	lua_setfield(L, 2, "null");

	/* Insert the decoder function */
	lua_pushcclosure(L, l_parse, 1);
	lua_setfield(L, 2, "parse");

	return 1;
}
