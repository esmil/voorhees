--[[
This is an overly simple and probably silly benchmark and
should not be trusted.

My apologies to the authors of Json.lua, JSON4lua and luajson
if this still offends you
--]]


local parse
---[[ Voorhees
do
   local M = require 'voorhees'
   parse = M.parse
end
--]]
--[[ Json.lua
do
   require 'Json'
   parse = Json.Decode
end
--]]
--[[ JSON4lua
do
   M = require 'json'
   parse = M.decode
end
--]]
--[[ luajson
do
   local M = require 'json.decode'
   parse = M.getDecoder(false)
end
--]]

local file = io.open('test/pass_long.json')
local JSONtext = file:read('*a')
file:close()

for i = 1, 2000 do
   local data = parse(JSONtext)
end

-- vi: syntax=lua ts=3 sw=3 et:
