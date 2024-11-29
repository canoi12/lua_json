NAME = json
CC = gcc
SOURCE = ljson.c
LUA_VERSION=5.4
CFLAGS = -std=gnu99 -g -O2
INCLUDE = -I/usr/include/lua$(LUA_VERSION)
LFLAGS = -llua$(LUA_VERSION)
ifeq ($(LUA_VERSION), jit)
	INCLUDE = -I/usr/include/luajit-2.1
	LFLAGS = -lluajit-5.1
endif

.PHONY: $(NAME).so

$(NAME).so: $(SOURCE)
	@echo "Compiling $@ for lua$(LUA_VERSION)"
	$(CC) -shared -fPIC $(SOURCE) -o $@ $(CFLAGS) $(LFLAGS) $(INCLUDE)

$(NAME).a: $(NAME).o
	@echo "Packing $@ for lua$(LUA_VERSION)"
	$(AR) rcs json.a json.o

$(NAME).o: $(SOURCE)
	@echo "Compiling $@ for lua$(LUA_VERSION)"
	$(CC) -c $(SOURCE) $(INCLUDE)
