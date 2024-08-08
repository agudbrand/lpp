-- Script for generating C types and de/serializers for those types
-- based on JSON schemas. Currently built specifically for lpp's language
-- server, but it may be useful to pull out and make more general later on.

-- TODO(sushi) elua needs to be able to pass arguments to scripts so 
--             that this isnt hardcoded.

local output_path = "src/generated/lsp.h"

local buffer = require "string.buffer"
local List = require "list"
local CGen = require "cgen"

---@class JSONSchemas
--- Mapping from type names to their definition
---@field map table
--- Ordered list of schemas as they are declared.
---@field list List

---@class json
---@field schemas JSONSchemas
local json = {}

json.schemas =
{
  map = {},
  list = List{},
}

local Schema,
      Object

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
  local o =
  {
    name = name,
    obj = Object.new(json),
  }

  json.schemas.map[name] = o
  json.schemas.list:push(o)

  return o.obj
end

--- Creates a new 'instance' of this schema to be 
Schema.newInstance = function()

end

---@class ObjectMembers
---@field list List
---@field map table

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

Object.__call = function(self, name)
  assert(not self.added, "this object was called twice")
  self.added = true
  if self.prev then
    self.prev.member.list:push{name=name,type=self}
    self.prev.member.map[name] = self
  end
  return self
end

Object.done = function(self)
  return self.prev
end
