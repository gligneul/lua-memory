/*
** $Id$
** Auxiliary functions for handling generic streams in Lua.
** See Copyright Notice in lstraux.h
*/

#define lmemlib_c

#define LUAMEMLIB_API

#include "lmemlib.h"

#include <string.h>



LUAMEMLIB_API char *luamem_newalloc (lua_State *L, size_t l) {
	char *mem = lua_newuserdata(L, l * sizeof(char));
	luaL_newmetatable(L, LUAMEM_ALLOC);
	lua_setmetatable(L, -2);
	return mem;
}


#define LUAMEM_REFREGISTRY	"luamem_ReferenceRegistry"

static int pushref (lua_State *L, char *mem) {
	if (!luaL_getsubtable(L, LUA_REGISTRYINDEX, LUAMEM_REFREGISTRY)) {
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
		lua_pushvalue(L, -1);
		lua_setmetatable(L, -2);
	}
	lua_pushlightuserdata(L, mem);
	return lua_gettable(L, -2) == LUA_TUSERDATA;
}

static int luaunref (lua_State *L) {
	luamem_Ref *ref = luamem_toref(L, 1);
	if (ref && ref->unref) ref->unref(ref->memory, ref->len);
	return 0;
}

LUAMEMLIB_API void luamem_pushrefmt (lua_State *L) {
	if (luaL_newmetatable(L, LUAMEM_REF)) {
		lua_pushcfunction(L, luaunref);
		lua_setfield(L, -2, "__gc");
	}
}

LUAMEMLIB_API luamem_Ref *luamem_pushref (lua_State *L, char *mem) {
	luamem_Ref *ref;
	if (pushref(L, mem)) {
		ref = (luamem_Ref *)lua_touserdata(L, -1);
	} else {
		lua_pop(L, 1);
		ref = (luamem_Ref *)lua_newuserdata(L, sizeof(luamen_Ref));
		ref->len = 0;
		ref->unref = NULL;
		ref->memory = mem;
		luamem_pushrefmt(L);
		lua_setmetatable(L, -2);
		lua_pushlightuserdata(L, mem);
		lua_pushvalue(L, -2);
		lua_settable(L, -4)
	}
	lua_remove(L, -2);
	return ref;
}

LUAMEMLIB_API luamem_Ref *luamem_getref (lua_State *L, char *mem) {
	luamem_Ref *ref;
	pushref(L, mem);
	ref = (luamem_Ref *)lua_touserdata(L, -1);
	lua_pop(L, 2);
	return ref;
}


LUAMEMLIB_API char *luamem_tomemory (lua_State *L, int idx, size_t *len) {
	void *p = lua_touserdata(L, idx);
	if (p) {  /* value is a userdata? */
		if (lua_getmetatable(L, idx)) {  /* does it have a metatable? */
			char *mem;
			luaL_getmetatable(L, LUAMEM_ALLOC);  /* get allocated memory metatable */
			if (lua_rawequal(L, -1, -2)) {  /* is the same? */
				if (len) *len = lua_rawlen(L, idx);
				mem = (char *)p;
			} else {
				lua_pop(L, 1);  /* remove allocated memory metatable */
				luaL_getmetatable(L, LUAMEM_REF);  /* get referenced memory metatable */
				if (lua_rawequal(L, -1, -2)) {
					luamem_Ref *ref = (luamem_Ref *)p;
					if (len) *len = ref->len;
					mem = ref->memory;
				} else {
					if (len) *len = 0;
					mem = NULL;
				}
			}
			lua_pop(L, 2);  /* remove both metatables */
			return mem;
		}
	}
	if (len) *len = 0;
	return NULL;
}

LUAMEMLIB_API char *luamem_checkmemory (lua_State *L, int idx, size_t *len) {
	char *mem = luamem_tomemory(L, idx, len);
	if (!mem) luaL_argerror(L, idx, "memory expected");
	return mem;
}


LUAMEMLIB_API int luamem_isstring (lua_State *L, int idx) {
	return (lua_isstring(L, idx) || luamem_ismemory(L, idx));
}

LUAMEMLIB_API const char *luamem_tostring (lua_State *L, int idx, size_t *len) {
	if (lua_isstring(L, idx)) return lua_tolstring(L, idx, len);
	return luamem_tomemory(L, idx, len);
}

LUAMEMLIB_API const char *luamem_checkstring (lua_State *L, int idx, size_t *len) {
	const char *s = luamem_tostring(L, idx, len);
	if (!s) luaL_argerror(L, idx, "string expected");
	return s;
}


/*
* NOTE: most of the code below is copied from the source of Lua 5.3.1 by
*       R. Ierusalimschy, L. H. de Figueiredo, W. Celes - Lua.org, PUC-Rio.
*
* Copyright (C) 1994-2015 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


/* macro to 'unsign' a character */
#define uchar(c)	((unsigned char)(c))


/* translate a relative string position: negative means back from end */
LUAMEMLIB_API lua_Integer luamem_posrelat (lua_Integer pos, size_t len) {
	if (pos >= 0) return pos;
	else if (0u - (size_t)pos > len) return 0;
	else return (lua_Integer)len + pos + 1;
}

LUAMEMLIB_API int luamem_str2byte (lua_State *L, const char *s, size_t l) {
	lua_Integer posi = luamem_posrelat(luaL_optinteger(L, 2, 1), l);
	lua_Integer pose = luamem_posrelat(luaL_optinteger(L, 3, posi), l);
	int n, i;
	if (posi < 1) posi = 1;
	if (pose > (lua_Integer)l) pose = l;
	if (posi > pose) return 0;  /* empty interval; return no values */
	n = (int)(pose - posi + 1);
	if (posi + n <= pose)  /* arithmetic overflow? */
		return luaL_error(L, "string slice too long");
	luaL_checkstack(L, n, "string slice too long");
	for (i=0; i<n; i++)
		lua_pushinteger(L, uchar(s[posi+i-1]));
	return n;
}

LUAMEMLIB_API void luamem_code2char (lua_State *L, int idx, char *p, int n) {
	int i;
	for (i=0; i<n; ++i, ++idx) {
		lua_Integer c = luaL_checkinteger(L, idx);
		luaL_argcheck(L, uchar(c) == c, idx, "value out of range");
		p[i] = uchar(c);
	}
}


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->initb)


LUAMEMLIB_API void luamem_pushresbuf (luamem_Buffer *B) {
	lua_State *L = B->L;
	if (!buffonstack(B) || B->n < B->size) {
		char *p = (char *)luamem_newalloc(L, B->n * sizeof(char));
		/* move content to new buffer */
		memcpy(p, B->b, B->n * sizeof(char));
		if (buffonstack(B))
			lua_remove(L, -2);  /* remove old buffer */
	}
}


LUAMEMLIB_API void luamem_pushresbufsize (luamem_Buffer *B, size_t sz) {
	luaL_addsize(B, sz);
	luamem_pushresbuf(B);
}


LUAMEMLIB_API void luamem_addvalue (luamem_Buffer *B) {
	lua_State *L = B->L;
	size_t l;
	const char *s = luamem_tostring(L, -1, &l);
	if (buffonstack(B))
		lua_insert(L, -2);  /* put value below buffer */
	luamem_addlstring(B, s, l);
	lua_remove(L, (buffonstack(B)) ? -2 : -1);  /* remove value */
}

/* }====================================================== */
