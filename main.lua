local json = require('json')
print(json)
print("helloooooo")

local f = io.open("test.json", "r")
local str = f:read("*a")
local j = json.decode(str)
f:close()

print(json.encode({"aah", "beeh", "ceeh", 10, 20, -5}))
print(json.encode({a = "ola", b = "oler", c = { "new", "table", true, false, nil }, d = 'corda bamba'}))

print(j)
for i,v in pairs(j) do
  print(i, v)
end

for i,v in ipairs(j.list) do
  print(i, v)
end

f = io.open('myfile.json', 'w')
local str = json.encode(j)
print(str)
f:write(str)
f:close()
