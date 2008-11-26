#!/usr/bin/env lua
--[[
Pretty print JSON encoded messages from twitter.com's API
Try

curl 'http://twitter.com/statuses/public_timeline.json' \
   | ./twitprint.lua

or perhaps

curl 'http://<user>:<pass>@twitter.com/statuses/friends_timeline.json' \
   | ./twitprint.lua
--]]

local parse
do
   local M = require 'voorhees'
   parse = M.parse
end

local format = string.format
local date = os.date

local getdate
do
   local month = {
      Jan = 1, Feb = 2, Mar = 3,
      Apr = 4, May = 5, Jun = 6,
      Jul = 7, Aug = 8, Sep = 9,
      Oct =10, Nov =11, Dec =12 }

   local match, time = string.match, os.time
   local mask = '(%a%a%a) (%a%a%a) (%d%d) '..
                '(%d%d):(%d%d):(%d%d) (.)(%d%d)(%d%d) (%d%d%d%d)'
   
   function getdate(str)
      local t = {}
      local wday, mabr, day, hour, min, sec, tzprfx, tzhours, tzmins, year =
         match(str, mask)

      t.month = month[mabr]
      t.day = day
      t.hour = hour
      t.min = min
      t.sec = sec
      t.year = year

      t = time(t)
      if tzprfx == '+' then
         return t - tzhours * 3600 - tzmins * 60
      else
         return t + tzhours * 3600 + tzmins * 60
      end
   end
end

local function format_date(str)
   local time = getdate(str) + 3600
   local diff = os.time() - time

   if diff < 60 then
      return format('%i seconds ago', diff)
   end

   if diff < 3600 then
      return format('%i minutes ago',
         math.floor(diff/60))
   end

   if diff < 18000 then
      return tostring(math.floor(diff/3600))..' hours ago'
   end

   return date('%a %d/%m %H:%M', time)
end

local function print_statuses(statuses, filler)
   for _, s in ipairs(statuses) do
      io.write(filler, s.user.name, ', ', format_date(s.created_at), '\n',
         filler, s.text:gsub('&lt;', '<'):gsub('&gt;', '>'), '\n\n')

      if s.replies then
         print_statuses(s.replies, filler..'  ')
      end
   end
end
   
local data
do
   local file, err
   if arg[1] then
      file, err = io.open(arg[1])
      if not file then
         print('Error opening '..err)
         os.exit(1)
      end
   else
      file = io.stdin
   end

   data, err = parse(function() return file:read(1024) end, 'utf8', 5)
   if not data then
      print('Error parsing JSON file: '..err)
      os.exit(1)
   end

   local index = {}

   for _, msg in ipairs(data) do
      index[msg.id] = msg
   end

   local i = 1
   while data[i] do
      local a = data[i].in_reply_to_status_id
      a = a and index[a]
      if a then
         local msg = table.remove(data, i)
         if a.replies then
            a.replies[#(a.replies) + 1] = msg
         else a.replies = { msg }
         end
      else
         i = i + 1
      end
   end
end

print_statuses(data, '')

-- vi: syntax=lua ts=3 sw=3 et:
