# Lua JSON

C lib for encode(WIP) and decode JSON to Lua.

```lua
local json = require('json')
-- open file and read string
local content = json.decode(json_str)
```

By now it's just an adaptation of my [tinyjson](https://github.com/canoi12/tinyjson) lib for convert directly to Lua.
The main focus is to directly embedd in a C-Lua project, but you can compile easily compile as a shared or static library.

As I don't use any Lua version specific feature, I think the lib is compatible with all Lua versions (from 5.1 to 5.4 and jit).
