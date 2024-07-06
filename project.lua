local enosi = require "enosi"
local recipes = require "recipes"
local Driver = require "driver"
local proj = enosi.thisProject()

local List = require "list"

local mode = enosi.mode
local cwd = lake.cwd()

local builddir = cwd.."/build/"..mode.."/"
lake.mkdir(builddir, {make_parents=true})

proj:setCleaner(function()
  lake.rm(builddir, {recursive = true, force = true})
end)

proj:dependsOn("iro")

local lpp = lake.target(builddir.."lpp")

proj:reportExecutable(lpp.path)

local cpp_driver = Driver.Cpp.new()

cpp_driver.include_dirs:push{ "src", proj:collectIncludeDirs() }

if mode == "debug" then
  cpp_driver.opt = "none"
  cpp_driver.debug_info = true
  proj:reportDefine { "LPP_DEBUG", "1" }
else
  cpp_driver.opt = "speed"
end

cpp_driver.defines = proj:collectDefines()

lake.find("src/**/*.cpp"):each(function(cfile)
  local ofile = builddir..cfile..".o"
  local dfile = builddir..cfile..".d"
  cfile = cwd.."/"..cfile

  proj:reportObjFile(ofile)

  cpp_driver.input = cfile
  cpp_driver.output = ofile

  lake.target(ofile):dependsOn(cfile)
      :recipe(recipes.objFile(cpp_driver, proj))

  lake.target(dfile):dependsOn(cfile)
      :recipe(recipes.depfile(
        Driver.Depfile.fromCpp(cpp_driver),
        proj, ofile, dfile))
end)

local lua_driver = Driver.LuaObj.new()

lake.find("src/*.lua"):each(function(lfile)
  local ofile = builddir..lfile..".o"
  lfile = cwd.."/"..lfile

  proj:reportObjFile(ofile)

  lua_driver.input = lfile
  lua_driver.output = ofile

  lake.target(ofile):dependsOn { lfile, proj.path }
      :recipe(recipes.luaObjFile(lua_driver, proj))
end)

lpp:dependsOn(proj:collectObjFiles())

local link_driver = Driver.Linker.new()

local inputs = proj:collectObjFiles()

local lppclang = enosi.getProject("lppclang")
if lppclang then
  lpp:dependsOn(lppclang:getLuaObjFiles())
  inputs:push(lppclang:getLuaObjFiles())
end

link_driver.static_libs = proj:collectStaticLibs()
link_driver.shared_libs = proj:collectSharedLibs()
link_driver.inputs = inputs
link_driver.libdirs = proj:collectLibDirs()
link_driver.output = lpp.path

lpp:recipe(recipes.linker(link_driver, proj))
