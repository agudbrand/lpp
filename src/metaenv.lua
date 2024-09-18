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

  menv.prev = lpp.metaenv
  menv.ctx = ctx
  menv.macro_invokers = {}
  menv.macro_invocations = {}
  menv.current_macro_arg_offsets = nil

  menv.exited = false

  local assertNotExited = function(fname)
    if not menv.exited then return end
    -- TODO(sushi) show proper error locations here or add support for this :)
    log:fatal(debug.traceback(), "\n\n")
    log:fatal(
      "__metaenv.", fname, " called for a metaenvironment that has already ",
      "exited!\nThis may have occured because you managed to call a \n",
      "function defined in one lpp file from a different lpp file. This is \n",
      "currently not supported.\n")
    os.exit(1)
  end

  menv.doc = function(start, str)
    -- str could be nil in the case of immediate macro invocations that don't
    -- return anything.
    assertNotExited "doc"
    if str then
      C.metaprogramAddDocumentSection(ctx, start, makeStr(str))
    end
  end

  menv.val = function(start, x)
    assertNotExited "val"
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

  local resolveImportStackTrace = function()
    -- Figure out import stack trace.
    -- TODO(sushi) this needs to consider the call stack up to the 
    --             previous import for each file. This is probably ok
    --             for now, though.
    local import_lines = List{}
    local mp = ctx
    local miter = menv.prev
    while miter do
      mp = miter.ctx

      local scope = C.metaprogramGetCurrentScope(mp)

      local info = 
        miter.macro_invocations[tonumber(C.scopeGetInvokingMacroIdx(scope))]

      if not info then break end

      import_lines:pushFront(getProperInfoString(info, mp))

      miter = miter.prev
    end
    return import_lines
  end

  local resolveLocalStackTrace = function(offset, stopCallback)
    local lines = List{}

    local infoidx = offset or 4
    local cidx = 0
    while true do
      local info = debug.getinfo(infoidx + cidx)

      if not info then break end

      if stopCallback and stopCallback(info) then break end

      if info.what ~= "C" then
        lines:pushFront(getProperInfoString(info))
        infoidx = infoidx + 1
      else
        cidx = cidx + 1
      end
    end

    return lines
  end

  local printMacroInvocationCallStack = function()
    -- Print the stack until we leave the current file.
    -- This SHOULD make sense for this specific case as I don't BELIEVE
    -- any sort of macro invocation would be able to call into another
    -- file that also calls macros.
    -- Actually, its possible for macros to cross file boundries if 
    -- two lpp files require the same lua file, one attaches a function
    -- containing a macro invocation to the module and then the other
    -- calls that function via the module. I have absolutely no idea 
    -- how I can handle this. Need some way to mark the call stack 
    -- with where an import occurs 
    local filename
    resolveLocalStackTrace(4,
      function(info)
        if not filename then
          filename = info.source
        else
          if filename ~= info.source then
            return false
          end
        end
      end)
      :each(
      function(line)
        log:error(line)
      end)
  end

  menv.macro = function(start, indent, name, macro, ...)
    assertNotExited "macro"
    -- capture where this macro was invoked in phase 1
    local invoke_capture, pos = stackcapture(1)

    local invocation = debug.getinfo(2)
    table.insert(menv.macro_invocations, invocation)

    if not macro then
      printMacroInvocationCallStack()
      log:error("macro with name '"..name.."' does not exist\n")
      error({handled=true})
    end

    local args = List{...}

    -- Extract the location information from the passed MacroParts and
    -- replace each arg with its actual string.
    -- This probably sucks performance-wise but whatever.
    local arg_offsets = {}
    args = args:map(function(arg)
      table.insert(arg_offsets, arg.start)
      return arg.text
    end).arr

    local invoker
    local errhandler = function(err)
      local out = buffer.new()

      resolveImportStackTrace():each(function(line)
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
     
      resolveLocalStackTrace(5, 
        function(info) 
          return info.func == invoker
        end)
        :each(function(line)
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
      local prev_macro_arg_offsets = menv.current_macro_arg_offsets
      menv.current_macro_arg_offsets = arg_offsets

      local wrapper = function() -- uuuugh
        return macro(unpack(args))
      end
      local result = {xpcall(wrapper, function(err)
        local result = {pcall(errhandler, err)}

        if not result[1] then
          log:fatal("error in error handler:\n", result[2], "\n")
        end
      end)}

      menv.current_macro_arg_offsets = prev_macro_arg_offsets

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

  menv.macro_immediate = function(start, indent, name, macro, ...)
    assertNotExited "macro_immediate"
    -- TODO(sushi) replace this with the better error reporting.
    -- capture where this macro was invoked in phase 1
    local invoke_capture, pos = stackcapture(1)

    local invocation = debug.getinfo(2)

    if not macro then
      printMacroInvocationCallStack()
      log:error("macro with name '"..name.."' does not exist\n")
      error({handled=true})
    end

    local args = List{...}
    local arg_offsets = {}
    args = args:map(function(arg)
      table.insert(arg_offsets, arg.start)
      return arg.text
    end).arr

    local errhandler = function(err)
      resolveImportStackTrace():each(function(line)
        log:error(line)
      end)

      log:error(getProperInfoString(invocation))

      resolveLocalStackTrace(5, 
        function(info) 
          return info.func == menv.macro_immediate
        end)
        :each(function(line)
          log:error(line)
        end)

      err = tostring(err)

      if err:startsWith "[string" then
        err = err:match "%[string .+%]:.+: (.*)"
      end

      log:error("  error occured in invocation:\n", err, "\n")
    end

    local prev_macro_arg_offsets = menv.current_macro_arg_offsets
    menv.current_macro_arg_offsets = arg_offsets

    local result = {xpcall(function()
      return macro(unpack(args))
    end, function(err)
      local result = {pcall(errhandler, err)}
      if not result[1] then
        log:fatal("error in error handler:\n", result[2], "\n")
      end
    end)}

    menv.current_macro_arg_offsets = prev_macro_arg_offsets

    if not result[1] then
      error({handled=true})
    end

    result = result[2]

    if lpp.MacroExpansion.isTypeOf(result) then
      return result()
    elseif "string" == type(result) or nil == result then
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

  return M
end
