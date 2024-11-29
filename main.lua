local json = require('json')
print(json)
print("helloooooo")

local f = io.open("test.json", "r")
local str = f:read("*a")
local j = json.decode(str)
f:close()

print(j)
for i,v in pairs(j) do
  print(i, v)
end

for i,v in ipairs(j.list) do
  print(i, v)
end
