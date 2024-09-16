--
--
-- Metaenvironment construction function.
--
-- This is used to create an environment table that the metaprogram 
-- runs in.
--
--

local ffi = require "ffi"
local C = ffi.C

local log = require "logger" ("metaenv.lua", Verbosity.Info)
local stackcapture = require "stackcapture"
local utils = require "util"
local List = require "list"
local buffer = require "string.buffer"
local util = require "util"

local strtype = ffi.typeof("str")
local makeStr = function(s)
  return strtype(s, #s)
end

return function(ctx)
  -- The metaenvironment table.
  -- All things in this table are accessible as 
  -- global things from within the metaprogram.
  local M = {}
  M.__index = _G

  setmetatable(M, M)

  M.__metaenv = {}
  local menv = M.__metaenv

  local lpp = require "lpp"
  menv.lpp = lpp

  menv.macro_invokers = {}
  menv.macro_invocations = {}

  menv.doc = function(start, str)
    C.metaprogramAddDocumentSection(ctx, start, makeStr(str))
  end

  menv.val = function(start, x)
    if x == nil then
      error("inline lua expression resulted in nil", 2)
    end
    local s = tostring(x)
    if not s then
      error(
        "result of inline lua expression is not convertible to a string", 2)
    end
    C.metaprogramAddDocumentSection(ctx, start, makeStr(s))
  end

  menv.macro = function(start, indent, name, macro, ...)
    local printCall = function(call)
      local proper_line
      local proper_src
      if call.src:sub(1,1) ~= "@" then
        proper_line = 
          C.metaprogramMapMetaprogramLineToInputLine(ctx, call.line)
        proper_src = call.src
      else
        proper_src = call.src:sub(2)
        proper_line = call.line
      end
      log:error(proper_src, ":", proper_line, ": ")
      if call.name then
        log:error("in ", call.name, ": ")
      end
    end

    local getProperInfoString = function(info, mp)
      mp = mp or ctx
      local proper_line
      local proper_src
      if info.source:sub(1,1) ~= "@" then
        proper_line = 
          C.metaprogramMapMetaprogramLineToInputLine(mp, info.currentline)
        proper_src = info.source
      else
        proper_src = info.source:sub(2)
        proper_line = info.currentline
      end
      local out = proper_src..":"..proper_line..": "
      if info.name then
        out = out.." in "..info.name..": "
      end
      return out.."\n"
    end

    -- capture where this macro was invoked in phase 1
    local invoke_capture, pos = stackcapture(1)

    local invocation = debug.getinfo(2)
    table.insert(menv.macro_invocations, invocation)

    local args = {...}
    if not macro then
      for i=1,invoke_capture:len() do
        printCall(invoke_capture[i])
        log:error("\n")
      end
      log:error("macro with name '"..name.."' does not exist\n")
      error({handled=true})
    end

    local invoker
    local errhandler = function(err)
      local out = buffer.new()

      -- Figure out import stack trace.
      local import_lines = List{}
      local mp = ctx
      local midx = #lpp.metaenvs-1
      while midx ~= 0 do
        mp = C.metaprogramGetPrev(mp)
        local me = lpp.metaenvs[midx]

        local scope = C.metaprogramGetCurrentScope(mp)

        local info = 
          me.macro_invocations[tonumber(C.scopeGetInvokingMacroIdx(scope))]

        import_lines:pushFront(getProperInfoString(info, mp))

        midx = midx - 1
      end

      import_lines:each(function(line)
        log:error(line)
      end)

      log:error(getProperInfoString(invocation))

      -- Figure out file local stack trace.
      local scope = C.metaprogramGetCurrentScope(ctx)
      while true do
        scope = C.scopeGetPrev(scope)
        if 0 == scope then
          break
        end

        local invoking_macro_idx = 
          tonumber(C.scopeGetInvokingMacroIdx(scope))

        local info = 
          menv.macro_invocations[invoking_macro_idx]
        if not info then
          break
        end

        log:error(getProperInfoString(info))
      end
      log:error("  in macro invoked here\n")

      local err_lines = List{}

      local infoidx = 4
      local cidx = 0
      while true do
        local info = debug.getinfo(infoidx + cidx)

        if info.func == invoker then
          break
        end

        if not info then break end

        if info.what ~= "C" then
          err_lines:pushFront(getProperInfoString(info))
          infoidx = infoidx + 1
        else
          cidx = cidx + 1
        end
      end

      err_lines:each(function(line)
        log:error(line)
      end)

      if type(err) ~= "string" and not getmetatable(err).__tostring then
        log:fatal(
          "INTERNAL: encountered an err object that is not a string and is",
          "not convertable to a string!\n")
        os.exit(1)
      end

      err = tostring(err)

      if err:startsWith "[string" then
        err = err:match "%[string .+%]:.+: (.*)"
      end

      log:error("  error occured in invocation:\n", err, "\n")
    end

    invoker = function()
      local wrapper = function() -- uuuugh
        return macro(unpack(args))
      end
      local result = {xpcall(wrapper, function(err)
        local result = {pcall(errhandler, err)}

        if not result[1] then
          log:fatal("error in error handler:\n", result[2], "\n")
        end
      end)}

      if not result[1] then
        os.exit(1)
      end

      result = result[2]

      if lpp.MacroExpansion.isTypeOf(result) then
        -- call the expansion table so that it reports how each part 
        -- expands
        return result()
      elseif "string" == type(result) or result == nil then
        return result
      else
        for i=1,invoke_capture:len() do
          printCall(invoke_capture[i])
          log:error("\n")
        end
        log:error("macro returned non-string value\n")
        error({handled=true})
      end
    end

    table.insert(menv.macro_invokers, invoker)
    C.metaprogramAddMacroSection(
      ctx, 
      makeStr(indent), 
      start, 
      #menv.macro_invokers)
  end

  return M
end
