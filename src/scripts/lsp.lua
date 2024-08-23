-- Script for generating C types and de/serializers for those types
-- based on JSON schemas. Currently built specifically for lpp's language
-- server, but it may be useful to pull out and make more general later on.

-- TODO(sushi) elua needs to be able to pass arguments to scripts so 
--             that this isnt hardcoded.

-- index
-- @json
-- @schema_type
-- @type_defs
-- @enum_types
-- @object_type
-- @schema_defs
-- @schema_writing

local output_path = "src/generated/lsp.h"

local buffer = require "string.buffer"
local List = require "list"
local CGen = require "cgen"
local util = require "util"

---@class JSONSchemas
--- Mapping from type names to their definition
---@field map table
--- Ordered list of schemas as they are declared.
---@field list List[Schema]

-- * --------------------------------------------------------------------------
-- @json

---@class json
---@field schemas JSONSchemas
---
---@field Schema function
local json = {}

json.schemas =
{
  map = {},
  list = List{},
}

-- Forward declarations and such
local Schema,
      Object,
      StringedEnum,
      NumericEnum

setmetatable(json,
{
  __index = function(self, key)
    local handlers =
    {
      Schema = function()
        return function(name)
          return Schema.new(self, name)
        end
      end
    }

    local handler = handlers[key]
    if handler then
      return handler()
    end

    print(debug.traceback())
    error("no handler for key '"..key.."'")
  end
})

-- * --------------------------------------------------------------------------
-- @schema_type

--- A json Schema, eg. a named Object that can be used to build other Scehma.
---@class Schema
--- The name of this Schema.
---@field name string
--- The object that defines the structure of this Schema.
---@field obj Object
Schema = {}
Schema.__index = Schema

--- Creates a new schema type and returns its internal Object.
---@param json json
---@return Object
Schema.new = function(json, name)
  ---@type Schema
  local o = setmetatable(
  {
    name = name,
    obj = Object.new(nil, json),
  }, Schema)

  json.schemas.map[name] = o
  json.schemas.list:push { name=name,def=o }

  return o.obj
end

--- Creates a new 'instance' of this schema.
---@param obj Object
Schema.newInstance = function(self, obj)
  return function(name)
    obj:addMember(self, name)
    return obj
  end
end

---@param c CGen
Schema.writeStructMember = function(self, c, name)
  c:appendStructMember(self.name, name, "{}")
end

---@param c CGen
Schema.writeStruct = function(self, c)
  c:beginStruct(self.name)

  self.obj.member.list:each(function(member)
    member.type:writeStructMember(c, member.name)
  end)

  c:beginFunction(
    "b8", "deserialize", "json::Object* obj")
  do
    c:append("using namespace json;")
    self.obj.member.list:each(function(member)
      if member.type.writeDeserializer then
        member.type:writeDeserializer(c, member.name, "this")
      else
        print("no deserializer for member "..member.name.."!")
      end
    end)
    c:append("return true;")
  end
  c:endFunction()

  c:endStruct()
end

local Types = setmetatable({},
{
  __newindex = function(self, key, value)
    assert(not rawget(self, key), "Type with name "..key.."already defined.")

    ---@class T
    ---@field name string
    ---@field obj Object
    local T = {}
    T.__index = T
    T.name = key

    T.new = function(obj)
      return setmetatable({obj=obj}, T)
    end

    T.__call = function(self, name)
      self.obj.member.map[name] = self
      self.obj.member.list:push { name=name, type=self }
      return self.obj
    end

    rawset(self, key, setmetatable(T, { __index = value, }))
  end
})

---@param c CGen
local findValue = function(c, name, kind, f)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = obj->findMember(\""..name.."\"_str)")
  do
    c:beginIf(vname.."->kind == Value::Kind::"..kind)
    do
      f(vname)
    end
    c:beginElse()
    do
      c:append(
        'ERROR("unexpected type for value \''..name..'\' wanted '..kind..
        ', got ", getValueKindString('..vname..'->kind), "\\n");')
      c:append("return false;")
    end
    c:endIf()
  end
  c:endIf()
end

-- @type_defs
Types.Bool =
{
  ---@param c CGen
  writeStructMember = function(self, c, name)
    c:appendStructMember("b8", name, "false")
  end,
  ---@param c CGen
  writeDeserializer = function(self, c, name, out)
    findValue(c, name, "Boolean", function(vname)
      c:append(out,"->",name," = ",vname,"->boolean;")
    end)
  end
}

-- * --------------------------------------------------------------------------
-- @enum_types

local enumCall = function(self, name_or_elem)
  if not self.name then
    self.name = name_or_elem
    self.json.user_schema.map[name_or_elem] = self
    self.json.user_schema.list:push{name=name_or_elem,def=self}
    return self
  end

  -- otherwise collect elements
  self.elems:push(name_or_elem)

  return self
end

local enumNewInstance = function(T, enum, set)
  return function(obj)
    return setmetatable(
    {
      ---@param c CGen
      writeStructMember = function(_, c, name)
        c:appendStructMember(T.name, name, "{}")
      end,
      writeDeserializer = enum.writeDeserializer
    },
    {
      __call = function(self, name)
        obj:addMember(self, name)
        return obj
      end,
      __index = function(_, key)
        local handlers =
        {
          Set = function()
            return function(name)
              return
              {
                writeStructMember = function(_, c, name)
                  c:appendStructMember(T.name.."Flags", name, "{}")
                end,
              }
            end
          end
        }

        local handler = handlers[key]
        if handler then
          return handler()
        end
      end
    })
  end
end

StringedEnum = {}
StringedEnum.__call = enumCall
StringedEnum.new = function(json)
  local o = {}
  o.json = json
  o.elems = List{}

  local getFromStringName = function(name)
    return "get"..name.."FromString"
  end

  local getFromEnumName = function(name)  
    return "getStringFrom"..name
  end

  ---@param c CGen
  o.toCDef = function(self, c)
    c:beginEnum(self.name)
    do
      self.elems:each(function(elem)
        for k in pairs(elem) do
          c:appendEnumElement(k)
        end
      end)
      c:appendEnumElement "COUNT"
    end
    c:endEnum()
    c:typedef("Flags<"..self.name..">", self.name.."Flags")

    local from_string_name = getFromStringName(o.name)
    c:beginStaticFunction(o.name, from_string_name, "str x")
    do
      c:beginSwitch("x.hash()")
      do
        o.elems:each(function(elem)
          for k,v in pairs(elem) do
            c:beginCase('"'..v..'"_hashed')
              c:append("return "..o.name.."::"..k..";")
            c:endCase()
          end
        end)
      end
      c:endSwitch()
      c:append(
        'assert(!"invalid string passed to ', from_string_name, '");')
      c:append("return {};")
    end
    c:endFunction()

    local from_enum_name = getFromEnumName(o.name)
    c:beginStaticFunction("str", from_enum_name, o.name.." x")
    do
      c:beginSwitch("x")
      do
        o.elems:each(function(elem)
          for k,v in pairs(elem) do
            c:beginCase(o.name.."::"..k)
              c:append("return \""..v.."\"_str;")
            c:endCase()
          end
        end)
      end
      c:endSwitch()
      c:append(
        'assert(!"invalid value passed to ', from_enum_name, '");')
      c:append("return {};")
    end
    c:endFunction()
  end

  o.newInstance = enumNewInstance(o,
  {
    ---@param c CGen
    writeDeserializer = function(_, c, name, out)
      findValue(c, name, "String", function(vname)
        c:append(
          out,"->",name," = ",getFromStringName(o.name),"(",vname,
          "->string);")
      end)
    end
  })
end

---@class ObjectMembers
---@field list List
---@field map table

-- * --------------------------------------------------------------------------
-- @object_type

--- A json Object
---@class Object
--- A list and map of the members belonging to this Object.
---@field member ObjectMembers
--- The object this table exists in, if any.
---@field prev Object?
--- A reference to the json module.
---@field json json
--- If this Object instance has been added to 'prev' already.
---@field added boolean
Object = {}
Object.__index = function(self, key)
  local handlers =
  {
    Object = function()
      return Object.new(self, self.json)
    end,
  }

  local handler = handlers[key]
  if handler then
    return handler()
  end

  ---@type T?
  local type = Types[key]
  if type then
    return type.new(self)
  end

  ---@type Schema
  local schema = self.json.schemas.map[key]
  if schema then
    return schema:newInstance(self)
  end

  local member = rawget(Object, key)
  if member then
    return member
  end
end

---@param prev Object?
---@param json json
---@return Object
Object.new = function(prev, json)
  ---@type Object
  local o =
  {
    json = json,
    prev = prev,
    added = false,
    member =
    {
      list = List{},
      map = {}
    }
  }
  return setmetatable(o, Object)
end

Object.addMember = function(self, type, name)
  self.member.list:push { name = name, type = type }
  self.member.map[name] = type
  return self
end

---@param name string
Object.addSelfToPrev = function(self, name)
  assert(not self.added, "this object was called twice")
  self.added = true
  if self.prev then
    self.prev.member.list:push{name=name,type=self}
    self.prev.member.map[name] = self
  end
  return self
end

---@param c CGen
Object.writeStructMember = function(self, c, name)
  local typename = "obj_"..name

  c:beginStruct(typename)
  do

    self.member.list:each(function(member)
      member.type:writeStructMember(c, member.name)
    end)

    c:beginFunction(
      "b8", "deserialize", "json::Object* obj")
    do
      c:append("using namespace json;")
      self.member.list:each(function(member)
        if member.type.writeDeserializer then
          member.type:writeDeserializer(c, member.name, "this")
        else
          print("no deserializer for member "..member.name.."!")
        end
      end)
      c:append("return true;")
    end
    c:endFunction()

  end
  c:endStruct()

  c:appendStructMember(typename, name, "{}")
end

Object.writeDeserializer = function(self, c, name, out)
  findValue(c, name, "Object", function(vname)
    c:append(name,".deserialize(&",vname,"->object);")
  end)
end

Object.__call = function(self, name)
  return self:addSelfToPrev(name)
end

Object.done = function(self)
  return self.prev
end

-- * --------------------------------------------------------------------------
-- @schema_defs

json.Schema "Test"
  .Bool "cool"

json.Schema "Test2"
  .Test "test"
  .Bool "hello"
  .Object "bye"
    .Bool "ugh"
    :done()

-- * --------------------------------------------------------------------------
-- @schema_writing

local c = CGen.new()
json.schemas.list:each(function(schema)
  schema.def:writeStruct(c)
end)

local f = io.open(output_path, "w")
if not f then
  error("failed to open '"..output_path.."' for writing")
end

f:write
[[
/*
  Generated by src/scripts/lsp.lua
*/

#ifndef _lpp_lsp_h
#define _lpp_lsp_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/flags.h"
#include "iro/containers/array.h"
#include "iro/json/types.h"

namespace lsp
{

using namespace iro;

static Logger logger = 
  Logger::create("lpp.lsp"_str, Logger::Verbosity::Trace);

]]

f:write(c.buffer:get())

f:write
[[

}

#endif

]]


f:close()
