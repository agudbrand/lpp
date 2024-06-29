local options = assert(lake.getOptions())

local List = require "list"

local mode = options.mode or "debug"
local build_dir = lake.cwd().."/build/"..mode.."/"

options.registerCleaner(function()
  lake.rm(build_dir, {recursive = true, force = options.force_clean})
end)

options.dependsOnProj "iro"

lake.mkdir(build_dir, {make_parents = true})

local report  = assert(options.report)
local reports = assert(options.reports)
local recipes = assert(options.recipes)

assert(reports.iro.objFiles, "lpp depends on iro's object files but they were not found in the "..
                             "reports table. Was iro's module imported? Maybe it was imported "..
               "after this one.")

local lpp = lake.target(build_dir.."lpp")

report.executable(tostring(lpp))

local cflags = List
{
  "-Isrc",
  options.getDependencyCompilerFlags(),
  "-fvisibility=hidden",
}

if mode == "debug" then
  cflags:push { "-O0", "-ggdb3", "-DLPP_DEBUG=1" }
else
  cflags:push "-O2"
end

-- TODO(sushi) once i get around to more stuff using this (if ever, eg. if it is replaced by lpp)
--             setup communicating these defines between projects instead of hardcoding it 
--             like this
if lake.os() == "Linux" then
  cflags:push "-DIRO_LINUX=1"
end

local lflags = List
{
  options.getDependencyLinkerFlags(),
  options.getProjLibs "luajit",
  "-ldeflate",
  "-lunistring",
  "-lncurses",
  "-Wl,-E",
}

-- if reports.lppclang then
--   lflags:push(options.getProjLibs "lppclang")
-- end

local c_files = lake.find("src/**/*.cpp")

for c_file in c_files:each() do
  local o_file = c_file:gsub("(.-)%.cpp", build_dir.."%1.o")
  local d_file = o_file:gsub("(.-)%.o", "%1.d")

  report.objFile(o_file)

  lake.target(o_file)
    :dependsOn { c_file, options.this_file }
    :recipe(recipes.compiler(c_file, o_file, cflags))

  lake.target(d_file)
    :dependsOn(c_file)
    :recipe(recipes.depfile(c_file, d_file, o_file, cflags))
end

local ofiles = lake.flatten { reports.lpp.objFiles, reports.iro.objFiles }

lake.find("src/*.lua"):each(function(lfile)
  local ofile = lfile:gsub("(.-)%.lua", build_dir.."%1.lua.o")
  report.objFile(ofile)

  ofiles:push(ofile)

  lpp:dependsOn(ofile)
  
  lake.target(ofile)
    :dependsOn { lfile, options.this_file }
    :recipe(recipes.luaToObj(lfile, ofile))
end)

lpp:dependsOn{ofiles, options.this_file}:recipe(recipes.linker(ofiles, lpp, lflags))

options.getProjLibs("luajit"):each(function(e)
  lpp:dependsOn(e)
end)

if reports.lppclang then
  options.getProjLibs("lppclang"):each(function(e)
    lpp:dependsOn(e)
  end)
end
