CC = gcc
SOURCE = ljson.c
LUA_VERSION=5.4
CFLAGS = -std=gnu99
INCLUDE = -I/usr/include/lua$(LUA_VERSION)
LFLAGS = -llua$(LUA_VERSION)
ifeq ($(LUA_VERSION), jit)
	INCLUDE = -I/usr/include/luajit-2.1
	LFLAGS = -lluajit-5.1
endif

.PHONY: json.so

json.so: $(SOURCE)
	@echo "Compiling $@ for lua$(LUA_VERSION)"
	$(CC) -shared -fPIC $(SOURCE) -o $@ $(CFLAGS) $(LFLAGS) $(INCLUDE)

json.a: json.o
	@echo "Packing $@ for lua$(LUA_VERSION)"
	$(AR) rcs json.a json.o

json.o: $(SOURCE)
	@echo "Compiling $@ for lua$(LUA_VERSION)"
	$(CC) -c $(SOURCE) $(INCLUDE)
