#!/usr/bin/env lua

local parse, null
do
   local M = require 'voorhees'
   parse, null = M.parse, M.null
end

local function dump_result(header, r, msg)
   print(header..':')

   if not r then
      print(msg)
   else
      for k, v in pairs(r) do
         print(k, type(v), tostring(v))
      end
   end

   print ''
end

local files = {
   'fail01.json',
   'fail02.json',
   'fail03.json',
   'fail04.json',
   'fail05.json',
   'fail06.json',
   'fail07.json',
   'fail08.json',
   'fail09.json',
   'fail10.json',
   'fail11.json',
   'fail12.json',
   'fail13.json',
   'fail14.json',
   'fail15.json',
   'fail16.json',
   'fail17.json',
   'fail18.json',
   'fail19.json',
   'fail20.json',
   'fail21.json',
   'fail22.json',
   'fail23.json',
   'fail24.json',
   'fail25.json',
   'fail26.json',
   'fail27.json',
   'fail28.json',
   'fail29.json',
   'fail30.json',
   'fail31.json',
   'fail32.json',
   'fail33.json',
   'fail34.json',
   'fail_isolated_surrogate_marker.json',
   'fail_invalid_utf8.json',
   'pass01.json',
   'pass02.json',
   'pass03.json',
   'pass_codepoints_from_unicode_org.json',
   'pass_windows_unicode.json',
   'pass_windows_unicode_bigendian.json',
   --'pass_long.json',
}

---[[
for _, filename in ipairs(files) do
   local file, msg = io.open('test/'..filename)
   if not file then
      print(msg)
      break
   end

   dump_result(filename,
      parse(function() return file:read(1024) end, 'utf8', 20))

   file:close()
end
--]]

local tests = {
   ['empty object'] = '{}',
   ['empty array']  = '[]',
   ['null']         = '{ "null" : null }',
   ['string']       = [[
{ "test\u0020string" : "newline\r\nPython\rLua rocks!\t\t\"\\o\/\"" }]],
}

for k, v in pairs(tests) do
   dump_result(k, parse(v, 'utf8', 20))
end

print('null = '..tostring(null))
print('null() = '..tostring(null()))

-- vi: syntax=lua ts=3 sw=3 et:
