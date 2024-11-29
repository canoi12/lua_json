CC = gcc
SOURCE = ljson.c
LUA_VERSION=5.4
INCLUDE = -I/usr/include/lua$(LUA_VERSION)
LFLAGS = -llua$(LUA_VERSION)

json.so: $(SOURCE)
	$(CC) -shared -fPIC $(SOURCE) -o $@ $(LFLAGS) $(INCLUDE)

json.a: json.o
	$(AR) rcs json.a json.o

json.o: $(SOURCE)
	$(CC) -c $(SOURCE) $(INCLUDE)
