Voorhees
======


About
-----

Voorhees is a yet another [Lua][1] library to parse [JSON][2] documents.
It is implemented in ANSI C and have no dependencies other than Lua itself.

It is based on the Pushdown Automaton implemented in the [JSON\_checker][3]
and is faster than any other JSON parser for Lua that I've found
(do correct me if I'm wrong).

Also the encoding of the input JSON text is independent from the strings
it returns. That is, the encoding of the input is auto detected
whereas you specify the encoding of the strings it returns.

Voorhees also enables you to parse big JSON documents by supplying a
generator function instead of having to load the full JSON text in a string.

*Any parser that uses a machete has my vote*

[1]: http://www.lua.org
[2]: http://www.json.org
[3]: http://www.json.org/JSON_checker/


Installation
------------

If you have set up [luarocks][4] just type `luarocks make` in the source
directory.

In a \*nix-like environment you can also do

    make
    make PREFIX=/usr install

This will install `voorhees.so` in `/usr/lib/lua/5.1`.
Have a look at the Makefile if this isn't right for your system.

In a Windows environment with Visual Studio installed you first need to set 
the appropriate environment variables using [vsvars32.bat][5] and then

    nmake -f win32\Makefile.win PREFIX=C:\Program Files\Lua\5.1
    nmake -f win32\Makefile.win install PREFIX=C:\Program Files\Lua\5.1

This will install `voorhees.dll` in `C:\Program Files\Lua\5.1\clibs` (the 
directory layout reflects the defaults of the [Lua for Windows][6] installer)

[4]: http://www.luarocks.org
[5]: http://msdn.microsoft.com/en-us/library/1700bbwd(VS.80).aspx
[6]: http://luaforwindows.luaforge.net/


Usage
-----

Here is a sample script:

    -- import the module
    voorhees = require 'voorhees'

    -- parse a JSON string
    data = voorhees.parse('[{ "key" : "string" }, { "answer" : 42 }]')

    -- open a file to parse
    file, err = io.open('bigfile.json', 'r')
    if not file then
       print('Error opening '..err)
       os.exit(1)
    end

    myNull = {}

    -- parse the file in pieces to avoid storing the whole file in memory,
    -- return latin-1 encoded strings, set the maximum stack size to 30,
    -- and use myNull as the JSON null value
    data = voorhees.parse(function() return file:read(1024) end,
                        'latin1', 30, myNull)

    file:close()

The default encoding of strings is UTF-8 ("utf8"), but also
little endian UTF-16 ("utf16" or "utf16le") is implemented.
There is also a latin-1 ("latin1") mode, which writes code points
less than 256 and maps all other Unicode to '?' characters.

The default maximum stack size is 20. That is you can nest
20 levels of arrays and objects.

The default value to return in place of the JSON `null` keyword is
stored in `voorhees.null`. It is defined as

    local function null()
       return null
    end

so you can reference it as both `voorhees.null` and `voorhees.null()`.


The generator function
----------------------

Instead of passing the whole JSON document in one string to the parser
it is possible to feed the text piece by piece by supplying a generator
function.

This function is called without arguments whenever the parser needs
a new piece of the text and must return the next piece as a string
(or a number which is then converted to a string).
Voorhees does not care how long the strings are as long as they're not empty.

The JSON document is deemed finished when the function returns an
empty string, `nil` or something else not a string, or raises an error.
Also the generator function mustn't yield.


Errors
------

Illegal arguments results in an error being raised whereas
syntax errors and encoding errors makes `voorhees.parse()` return
`nil` followed by a string with the error message.


License
-------

Voorhees is free software. It is distributed under the terms of the
[MIT license][7]

[7]: http://www.opensource.org/licenses/mit-license.php


Contact
-------

Please send bug reports, patches, feature requests, praise and general gossip
to me, Emil Renner Berthing <esmil@mailme.dk>.
