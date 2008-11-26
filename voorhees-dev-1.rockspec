package = "voorhees"
version = "dev-1"
source = {
   url = "git://github.com/esmil/voorhees.git"
}

description = {
   summary = "Another JSON parser for Lua",
   detailed = [[
Vorhees is a very fast JSON parser based on a Pushdown
Automaton implemented in ANSI C. It has no dependencies other
than Lua itself, supports many encodings and can output strings
encoded differently. Also big documents can be passed piece by
piece by means of a generator function.]],
   homepage = "http://github.com/esmil/voorhees",
   maintainer = "Emil Renner Berthing <esmil@mailme.dk>",
   license = "MIT"
}

build = {
   platforms = {
      linux = {
         type = "make",
         install_pass = false,
         install = { lib = { "voorhees.so" } }
      }
   },
   type = "builtin",
   modules = {
      voorhees = {
         sources = "voorhees.c",
      }
   }
}

-- vi: syntax=lua ts=3 sw=3 et:
