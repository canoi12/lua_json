# Lua JSON

C lib for encode(WIP) and decode JSON to Lua.

```lua
local json = require('json')
-- open file and read string
local content = json.decode('{"a": "Hello", "b": "World", "c": ["from", "JSON"]}')
print(content.a, content.b, content.c[1], content.c[2])
```

```lua
local json = require('json')
local str = json.encode({a='Hello', b='World', c={'from', 'Lua'}})
print(str)
```

By now it's just an adaptation of my [tinyjson](https://github.com/canoi12/tinyjson) lib to convert directly to Lua.
This lib was made with the idea to be directly embedded in my C-Lua projects as a third-party dependency, but you can compile easily compile as a shared or static library.

As I don't use any Lua version specific feature, I think the lib is compatible with all Lua versions (from 5.1 to 5.4 and jit).
