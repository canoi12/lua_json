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

struct ljson_parser_s {
    ljson_token_t current;
    ljson_token_t previous;
    int hand_error;
    int panic_mode;
};

typedef struct ljson_parser_s ljson_parser_t;

struct ljson_encoder_s {
  int indent_level;
};
typedef struct ljson_encoder_s ljson_encoder_t;

static ljson_scanner_t scanner;
static ljson_parser_t parser;
static ljson_encoder_t encoder;

/* scanner */
static int s_parse_json(lua_State* L);

/* utils */
static char* s_file_read(const char* filename);

// ljson_t* ljson_parse(const char* json_str) { return s_parse_json(json_str); }

static int l_json_encode(lua_State* L) {
  encoder.indent_level = 0;
  return 1;
}

static int l_json_decode(lua_State* L) {
    s_parse_json(L);
    return 1;
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
 *    Parser    *
 *==============*/

static void s_error_at(ljson_token_t* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = 1;
    fprintf(stderr, "[lua_json]:%d Error", token->line);
    if (token->type == LJSON_TOKEN_EOF) fprintf(stderr, " at end");
    else fprintf(stderr, " at %.*s,", token->length, token->start);
    fprintf(stderr, " %s\n", message);
    parser.hand_error = 1;
}

#if 0
static void s_error_at_current(const char* message) {
    s_error_at(&parser.current, message);
}
#endif

static char* s_parse_cstring(ljson_token_t* token) {
    int len = token->length;
    char* string = (char*)malloc(len-1);
    memcpy(string, token->start+1, len-2);
    string[len-2] = '\0';
    return string;
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
    // return ljson_create_number(value);
}

static int s_parse_string(lua_State* L, ljson_token_t* token) {
    char* str = s_parse_cstring(token);
    lua_pushstring(L, str);
    return 1;
}

static int s_parse_json_token(lua_State* L, ljson_token_t* token);

static int s_parse_object(lua_State* L) {
    // ljson_t* obj = ljson_create_object();
    lua_newtable(L);
    ljson_token_t token = s_scan_token();
    while (token.type != LJSON_TOKEN_RBRACE) {
        char* name = s_parse_cstring(&token);
        token = s_scan_token();
        if (token.type != LJSON_TOKEN_COLON) {
            s_error_at(&token, "missing ':'");
            exit(1);
        } else token = s_scan_token();

        s_parse_json_token(L, &token);
        // ljson_object_set(obj, name, val);
        lua_setfield(L, -2, name);

        token = s_scan_token();
        if (token.type == LJSON_TOKEN_COMMA) {
            token = s_scan_token();
            if (token.type == LJSON_TOKEN_RBRACE) {
                s_error_at(&token, "extra ','");
                exit(1);
            }
        } else if (token.type != LJSON_TOKEN_RBRACE) {
            s_error_at(&token, "missing ','");
            exit(1);
        }
    }
    return 1;
}

static int s_parse_array(lua_State* L) {
    // ljson_t* array = ljson_create_array();
    lua_newtable(L);
    ljson_token_t token = s_scan_token();
    int i = 1;
    while (token.type != LJSON_TOKEN_RSQUAR) {
        s_parse_json_token(L, &token);
        lua_rawseti(L, -2, i++);
        // ljson_array_push(array, val);
        token = s_scan_token();
        if (token.type == LJSON_TOKEN_COMMA) {
            token = s_scan_token();
            if (token.type == LJSON_TOKEN_RSQUAR) {
                s_error_at(&token, "extra ','");
                exit(1);
            }
        } else if (token.type != LJSON_TOKEN_RSQUAR) {
            s_error_at(&token, "missing ','");
            exit(1);
        }
    }
    return 0;
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
        {
        s_error_at(token, token->start);
        exit(1);
        }
    }
    s_error_at(token, "unkown symbol");
    exit(1);
    return 0;
}

int s_parse_json(lua_State* L) {
    const char* json = luaL_checkstring(L, 1);
    s_init_scanner(json);
    parser.hand_error = 0;
    parser.panic_mode = 0;
    ljson_token_t token = s_scan_token();
    return s_parse_json_token(L, &token);
}
