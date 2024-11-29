#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ljson.h"

typedef enum {
  LJSON_TOKEN_NULL = 0,   /* Null token                 'null'   */
  LJSON_TOKEN_FALSE,      /* False token                'false'  */
  LJSON_TOKEN_TRUE,       /* True token                 'true'   */
  LJSON_TOKEN_NUMBER,     /* Number token               '012..9' */
  LJSON_TOKEN_STRING,     /* String token               '"'      */
  LJSON_TOKEN_LBRACE,     /* Left bracket token         '{'      */
  LJSON_TOKEN_RBRACE,     /* Right bracket token        '}'      */
  LJSON_TOKEN_LSQUAR,     /* Left square bracket token  '['      */
  LJSON_TOKEN_RSQUAR,     /* Right square bracket token ']'      */
  LJSON_TOKEN_COMMA,      /* Comma token                ','      */
  LJSON_TOKEN_DOT,        /* Dot token                  '.'      */
  LJSON_TOKEN_MINUS,      /* Minus token                '-'      */
  LJSON_TOKEN_COLON,      /* Colon token                ':'      */
  LJSON_TOKEN_IDENTIFIER, /* Identifier token                    */
  LJSON_TOKEN_ERROR,      /* Error                               */
  LJSON_TOKEN_EOF         /* End of file                         */
} LJSON_TOKEN_;

struct ljson_scanner_s {
    const char* start;
    const char* current;
    int line;
};

struct ljson_token_s {
    int type;
    const char* start;
    int length;
    int line;
};

typedef struct ljson_token_s ljson_token_t;
typedef struct ljson_scanner_s ljson_scanner_t;

struct ljson_decoder_s {
    ljson_token_t current;
    ljson_token_t previous;
    int hand_error;
    int panic_mode;
};

typedef struct ljson_decoder_s ljson_decoder_t;

struct ljson_encoder_s {
  int indent_level;
  int offset, size;
  char* tmp_buffer;
};
typedef struct ljson_encoder_s ljson_encoder_t;

static ljson_scanner_t scanner;
static ljson_decoder_t decoder;
static ljson_encoder_t encoder;

/* decoder */
static int s_decode_json(lua_State* L);
/* encoder */
static int s_encode_json(lua_State* L);

/* utils */
static char* s_file_read(const char* filename);

static int l_json_encode(lua_State* L) {
  return s_encode_json(L);
}

static int l_json_decode(lua_State* L) {
    return s_decode_json(L);
}

int luaopen_json(lua_State* L) {
    luaL_Reg reg[] = {
        { "encode", l_json_encode },
        { "decode", l_json_decode },
        { NULL, NULL }
    };
    luaL_newlib(L, reg);
    return 1;
}

/*==============*
 *   Scanner    *
 *==============*/

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alpha(char c) { return c >= 'a' && c <= 'z'; }
static int is_at_end() { return *(scanner.current) == '\0'; }
static char advance_scanner() {
    scanner.current++;
    return scanner.current[-1];
}
static char peek() { return *(scanner.current); };
static char peek_next() {
    if (is_at_end()) return '\0';
    return scanner.current[1];
}
static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch(c) {
            case ' ':
            case '\r':
            case '\t':
                advance_scanner();
                break;
            case '\n':
                scanner.line++;
                advance_scanner();
                break;
            default:
                return;
        }
    }
}

static ljson_token_t s_make_token(LJSON_TOKEN_ type) {
    ljson_token_t token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static ljson_token_t s_error_token(const char* message) {
    ljson_token_t token;
    token.type = LJSON_TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static LJSON_TOKEN_ identifier_type() {
    switch (scanner.start[0]) {
        case 'n': return LJSON_TOKEN_NULL;
        case 't': return LJSON_TOKEN_TRUE;
        case 'f': return LJSON_TOKEN_FALSE;
    }
    return LJSON_TOKEN_IDENTIFIER;
}

static ljson_token_t string_token() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') scanner.line++;
        advance_scanner();
    }

    if (is_at_end()) return s_error_token("Unterminated string");

    advance_scanner();
    return s_make_token(LJSON_TOKEN_STRING);
}

static ljson_token_t number_token() {
    while (is_digit(peek())) advance_scanner();
    if (peek() == '.' && is_digit(peek_next())) {
        advance_scanner();
        while (is_digit(peek())) advance_scanner();
    }
    return s_make_token(LJSON_TOKEN_NUMBER);
}

static ljson_token_t identifier_token() {
    while (is_alpha(peek())) advance_scanner();
    return s_make_token(identifier_type());
}

static void s_init_scanner(const char* json_str) {
    scanner.start = json_str;
    scanner.current = json_str;
    scanner.line = 1;
}

static ljson_token_t s_scan_token(void) {
    skip_whitespace();
    scanner.start = scanner.current;
    if (is_at_end()) return s_make_token(LJSON_TOKEN_EOF);
    char c = advance_scanner();
    if (is_alpha(c)) return identifier_token();
    if (is_digit(c)) return number_token();

    switch(c) {
        case '{': return s_make_token(LJSON_TOKEN_LBRACE);
        case '}': return s_make_token(LJSON_TOKEN_RBRACE);
        case '[': return s_make_token(LJSON_TOKEN_LSQUAR);
        case ']': return s_make_token(LJSON_TOKEN_RSQUAR);
        case ',': return s_make_token(LJSON_TOKEN_COMMA);
        case '.': return s_make_token(LJSON_TOKEN_DOT);
        case '-': return s_make_token(LJSON_TOKEN_MINUS);
        case ':': return s_make_token(LJSON_TOKEN_COLON);
        case '"': return string_token();
    }
    return s_error_token("Unexpected character");
}

/*==============*
 *   Encoder    *
 *==============*/

static int s_encode_string(lua_State* L, int index) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    buf[0] = '"';
    size_t len;
    const char* str = luaL_checklstring(L, index, &len);
    if ((encoder.offset + len + 2) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }
    buf++;
    strncpy(buf, str, len);
    buf += len;
    buf[0] = '"';
    encoder.offset += len+2;
    return 1;
}

static int s_encode_integer(lua_State* L, int index) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    char aux[128];
    int val = luaL_checkinteger(L, index);
    sprintf(aux, "%d", val);
    int len = strlen(aux);
    if ((encoder.offset + len) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }
    strncpy(buf, aux, len);
    buf += len;
    encoder.offset += len;
    return 1;
}

static int s_encode_boolean(lua_State* L, int index) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    int val = lua_toboolean(L, index);
    int len = val ? 4 : 5;
    if ((encoder.offset + len) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }
    if (val)
        strncpy(buf, "true", len);
    else
        strncpy(buf, "false", len);
    buf += len;
    encoder.offset += len;
    return 1;
}

static int s_encode_null(lua_State* L) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    int len = 4;
    if ((encoder.offset + len) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }
    strncpy(buf, "null", len);
    buf += len;
    encoder.offset += len;
    return 1;
}

static int s_encode_table(lua_State* L, int index);
static int s_encode_array(lua_State* L, int index);
static int s_encode_object(lua_State* L, int index);

int s_encode_array(lua_State* L, int index) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    buf[0] = '[';
    lua_pushvalue(L, index);
    lua_pushnil(L);
    encoder.offset++;
    // check for space
    if ((encoder.offset + 4) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }

    while (lua_next(L, -2)) {
        lua_pushvalue(L, -1);
        switch (lua_type(L, -1)) {
            case LUA_TSTRING: s_encode_string(L, -1); break;
            case LUA_TNUMBER: s_encode_integer(L, -1); break;
            case LUA_TBOOLEAN: s_encode_boolean(L, -1); break;
            case LUA_TTABLE: s_encode_table(L, -1); break;
            case LUA_TNIL: s_encode_null(L); break;
        }
        buf = encoder.tmp_buffer + encoder.offset;
        buf[0] = ',';
        encoder.offset += 1;
        lua_pop(L, 2);
    }
    lua_pop(L, 1);

    encoder.offset -= 1;
    buf = encoder.tmp_buffer + encoder.offset;
    buf[0] = ']';
    encoder.offset += 1;

    return 1;
}

int s_encode_object(lua_State* L, int index) {
    char* buf = encoder.tmp_buffer + encoder.offset;
    buf[0] = '{';
    lua_pushvalue(L, index);
    lua_pushnil(L);
    // check for space
    if ((encoder.offset + 4) > encoder.size) {
        encoder.size *= 2;
        encoder.tmp_buffer = realloc(encoder.tmp_buffer, encoder.size);
    }
    encoder.offset++;

    while (lua_next(L, -2)) {
        lua_pushvalue(L, -2);
        s_encode_string(L, -1);
        buf = encoder.tmp_buffer + encoder.offset;
        buf[0] = ':';
        encoder.offset += 1;
        switch (lua_type(L, -2)) {
            case LUA_TSTRING: s_encode_string(L, -2); break;
            case LUA_TNUMBER: s_encode_integer(L, -2); break;
            case LUA_TBOOLEAN: s_encode_boolean(L, -2); break;
            case LUA_TTABLE: s_encode_table(L, -2); break;
            case LUA_TNIL: s_encode_null(L); break;
        }
        buf = encoder.tmp_buffer + encoder.offset;
        buf[0] = ',';
        encoder.offset += 1;
        lua_pop(L, 2);
    }
    lua_pop(L, 1);

    encoder.offset -= 1;
    buf = encoder.tmp_buffer + encoder.offset;
    buf[0] = '}';
    encoder.offset += 1;

  return 1;
}

int s_encode_table(lua_State* L, int index) {
    if (lua_rawlen(L, index) > 0) {
      s_encode_array(L, index);
    } else s_encode_object(L, index);
    return 1;
}

static int s_encode_json(lua_State* L) {
    encoder.indent_level = 0;
    encoder.tmp_buffer = malloc(512);
    encoder.size = 512;
    encoder.offset = 0;
    if (!lua_istable(L, 1))
        return luaL_argerror(L, 1, "must be a table");
    s_encode_table(L, 1);
    lua_pushlstring(L, encoder.tmp_buffer, encoder.offset);
    free(encoder.tmp_buffer);
    return 1;
}

/*==============*
 *   Decoder    *
 *==============*/

static int s_error_at(lua_State* L, ljson_token_t* token, const char* message) {
    if (decoder.panic_mode) return 0;
    decoder.panic_mode = 1;
    decoder.hand_error = 1;
    lua_pushfstring(L, "[lua_json]:%d Error", token->line);
    char* msg = malloc(token->length+1);
    if (token->type == LJSON_TOKEN_EOF) lua_pushstring(L, " at end");
    else {
        sprintf(msg, "%.*s", token->length, token->start);
        lua_pushfstring(L, " at %s,", msg);
        free(msg);
    }
    lua_pushfstring(L, " %s", message);
    lua_concat(L, 3);
    return lua_error(L);
}

static int s_parse_number(lua_State* L, ljson_token_t* token) {
    double value = 0;
    if (token->type == LJSON_TOKEN_MINUS) {
        ljson_token_t tnext = s_scan_token();
        value = -strtod(tnext.start, NULL);
    }
    else value = strtod(token->start, NULL);
    lua_pushnumber(L, value);
    return 1;
}

static int s_parse_string(lua_State* L, ljson_token_t* token) {
    int len = token->length;
    lua_pushlstring(L, token->start+1, len-2);
    return 1;
}

static int s_parse_json_token(lua_State* L, ljson_token_t* token);

static int s_parse_object(lua_State* L) {
    lua_newtable(L);
    ljson_token_t token = s_scan_token();
    while (token.type != LJSON_TOKEN_RBRACE) {
        s_parse_string(L, &token);
        token = s_scan_token();
        if (token.type != LJSON_TOKEN_COLON) {
            return s_error_at(L, &token, "missing ':'");
        } else token = s_scan_token();

        s_parse_json_token(L, &token);
        lua_settable(L, -3);

        token = s_scan_token();
        if (token.type == LJSON_TOKEN_COMMA) {
            token = s_scan_token();
            if (token.type == LJSON_TOKEN_RBRACE) {
                return s_error_at(L, &token, "extra ','");
            }
        } else if (token.type != LJSON_TOKEN_RBRACE) {
            return s_error_at(L, &token, "missing ','");
        }
    }
    return 1;
}

static int s_parse_array(lua_State* L) {
    lua_newtable(L);
    ljson_token_t token = s_scan_token();
    int i = 1;
    while (token.type != LJSON_TOKEN_RSQUAR) {
        s_parse_json_token(L, &token);
        lua_rawseti(L, -2, i++);
        token = s_scan_token();
        if (token.type == LJSON_TOKEN_COMMA) {
            token = s_scan_token();
            if (token.type == LJSON_TOKEN_RSQUAR) {
                return s_error_at(L, &token, "extra ','");
            }
        } else if (token.type != LJSON_TOKEN_RSQUAR) {
            return s_error_at(L, &token, "missing ','");
        }
    }
    return 1;
}

int s_parse_json_token(lua_State* L, ljson_token_t *token) {
    switch (token->type) {
    case LJSON_TOKEN_LBRACE:
        return s_parse_object(L);
    case LJSON_TOKEN_LSQUAR:
        return s_parse_array(L);
    case LJSON_TOKEN_MINUS:
    case LJSON_TOKEN_NUMBER:
        return s_parse_number(L, token);
    case LJSON_TOKEN_STRING:
        return s_parse_string(L, token);
    case LJSON_TOKEN_TRUE: {
        lua_pushboolean(L, 1);
        return 1;
    }
    case LJSON_TOKEN_FALSE: {
        lua_pushboolean(L, 0);
        return 1;
    }
    case LJSON_TOKEN_NULL: {
        lua_pushnil(L);
        return 1;
    }
    case LJSON_TOKEN_ERROR:
        return s_error_at(L, token, token->start);
    }
    return s_error_at(L, token, "unkown symbol");
}

int s_decode_json(lua_State* L) {
    const char* json = luaL_checkstring(L, 1);
    s_init_scanner(json);
    decoder.hand_error = 0;
    decoder.panic_mode = 0;
    ljson_token_t token = s_scan_token();
    return s_parse_json_token(L, &token);
}
