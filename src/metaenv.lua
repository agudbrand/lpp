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

local log = require "logger" ("metaenv.lua", Verbosity.Notice)
local stackcapture = require "stackcapture"

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

  menv.lpp = require "lpp"

  menv.macro_table = {}

  menv.doc = function(start, str)
    C.metaprogramAddDocumentSection(ctx, start, makeStr(str))
  end

  menv.val = function(start, x)
    if x == nil then
      error("inline lua expression resulted in nil", 2)
    end
    local s = tostring(x)
    if not s then
      error("result of inline lua expression is not convertible to a string", 2)
    end
    C.metaprogramAddDocumentSection(ctx, start, makeStr(s))
  end

  menv.macro = function(start, indent, name, macro, ...)
    -- capture where this macro was invoked in phase 1
    local invoke_capture = stackcapture(1)

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

    local args = {...}
    if not macro then
      for i=1,invoke_capture:len() do
        printCall(invoke_capture[i])
        log:error("\n")
      end
      log:error("macro with name "..name.." does not exist\n")
      error({handled=true})
    end

    local invoker = function()
      local wrapper = function() -- uuuugh
        return macro(unpack(args))
      end
      local result = {xpcall(wrapper, function(err)
        local error_capture = stackcapture(3)
        for i=1,invoke_capture:len() do
          printCall(invoke_capture[i])
          log:error("\n")
        end
        log:error("in macro specified here\n")
        for i=1,error_capture:len()-1 do
          printCall(error_capture[i])
          log:error("\n")
        end
        log:error("error occured in invocation:\n", err, "\n")
        error({handled=true})
      end)}

      if not result[1] then
        error({handled=true})
      end

      result = result[2]

      if menv.lpp.MacroExpansion.isTypeOf(result) then
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

    table.insert(menv.macro_table, invoker)
    C.metaprogramAddMacroSection(
      ctx, 
      makeStr(indent), 
      start, 
      #menv.macro_table)
  end

  return M
end
