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

local inst = {}

-- debug.sethook(function(event, line)
--   local i = debug.getinfo(2)
--   local s = i.short_src
--   local f = i.name
--   local id = s..":"..line
--   print(id, f)
--   if not inst[id] then 
--     inst[id] = 1 
--   else
--     inst[id] = inst[id] + 1
--   end
-- end, "l")

-- * --------------------------------------------------------------------------
-- @json

---@class json
---@field schemas JSONSchemas
---
---@field Schema function
---@field StringedEnum function
---@field NumericEnum function
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
      NumericEnum,
      Variant,
      SchemaArray

setmetatable(json,
{
  __index = function(self, key)
    local handlers =
    {
      Schema = function()
        return function(name)
          return Schema.new(self, name)
        end
      end,
      StringedEnum = function()
        return StringedEnum.new(self)
      end,
      NumericEnum = function()
        return NumericEnum.new(self)
      end,
    }

    local handler = handlers[key]
    if handler then
      return handler()
    end

    error("no handler for key '"..key.."'")
  end
})

---@param c CGen
local findValue = function(c, name, kind, f)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = obj->findMember(\""..name.."\"_str)")
  do
    if kind then
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
    else
      f(vname)
    end
  end
  c:endIf()
end

---@param c CGen
local newValue = function(c, name, kind, obj, f)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = json->newValue(Value::Kind::"..kind..")")
  do
    c:beginIf("!"..vname.."->init()")
      c:append('ERROR("failed to init value ',vname,'\\n");')
      c:append("return false;")
    c:endIf()
    c:append(obj,"->addMember(\"",name,"\"_str, ",vname,");")
    f(vname)
  end
  c:beginElse()
  do
    c:append("ERROR(\"failed to create value for ",name,"\\n\");")
    c:append("return false;")
  end
  c:endIf()
end

---@param c CGen
local newArrayValue = function(c, name, obj, init_space, allocator, f)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = json->newValue(Value::Kind::Array)")
  do
    c:beginIf("!"..vname.."->array.init("..init_space..","..allocator..")")
    do
      c:append('ERROR("failed to init value ',vname,'\\n");')
      c:append("return false;")
    end
    c:endIf()
    c:append(obj,'->addMember("',name,'"_str, ',vname,');')
    f(vname)
  end
  c:beginElse()
  do
    c:append("ERROR(\"failed to create value for ",name,"\\n\");")
    c:append("return false;")
  end
  c:endIf()
end

---@param c CGen
local newArrayElemValue = function(c, name, kind, arr, f)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = json->newValue(Value::Kind::"..kind..")")
  do
    c:beginIf("!"..vname.."->init()")
    do
      c:append('ERROR("failed to init value ',vname,'\\n");')
      c:append("return false;")
    end
    c:endIf()
    c:append(arr.."->push(",vname,");")
    f(vname)
  end
  c:beginElse()
  do
    c:append("ERROR(\"failed to create value for ",name,"\\n\");")
    c:append("return false;")
  end
  c:endIf()
end

-- * --------------------------------------------------------------------------
-- @variant

Variant = {}
Variant.handlers = {}

Variant.new = function(prev, json)
  return setmetatable(
  {
    json = json,
    prev = prev,
    primitives = {},
    objects = List{},
    schemas = {}
  }, Variant)
end

---@param c CGen
Variant.writeStructMember = function(self, c, name)
  local typename = "variant_"..name

  c:beginStruct(typename)
  do
    c:beginEnum("Kind")
    do
      c:appendEnumElement("Unknown")
          
      for kind in pairs(self.primitives) do
        c:appendEnumElement(kind) 
      end

      self.objects:each(function(o)
        c:appendEnumElement(o.name)
      end)

      for kind in pairs(self.schemas) do
        c:appendEnumElement(kind)
      end
    end
    c:endEnum()

    c:appendStructMember("Kind", "kind", "Kind::Unknown")

    -- Place object typedefs
    self.objects:each(function(o)
      o.def:writeStructSubtype(c, "as"..o.name)
    end)

    -- Will break if there's a nested union for whatever reason
    -- ugh! add a thing that tracks nesting eventually
    c.disable_struct_member_defaults = true

    c:beginUnion()
    do
      for kind,type in pairs(self.primitives) do
        type:writeStructMember(c, "as"..kind)
      end

      self.objects:each(function(o)
        o.def:writeStructMember(c, "as"..o.name)
      end)

      for kind,type in pairs(self.schemas) do
        type:writeStructMember(c, "as"..kind)
      end
    end
    c:endUnion()

    c.disable_struct_member_defaults = false

    c:beginFunction(
      "b8", "deserialize", "json::Value* val", "mem::Allocator* allocator")
    do
      c:append("using namespace json;")

      c:beginSwitch("val->kind")
      do
        for kind,type in pairs(self.primitives) do
          c:beginCase("Value::Kind::"..kind)
          type:writeVariantDeserializerCase(c, "as"..kind, "val", "this")
          c:append("this->kind = Kind::",kind,";")
          c:endCase()
        end

        c:beginCase("Value::Kind::Object")
        c:beginScope()
        do
          -- We have to discern what object or schema this is.
          -- Currently just matching against all members of each object
          -- starting from the one with most members to the one with least.
          -- This is probably quite bad and should be handled better later on.

          c:append("Object* obj = &val->object;")

          -- Collect each object type and sort them by number of members.
          local types = List{}

          self.objects:each(function(o)
            types:push { o.def.member.list:len(), {o.name, o.def} }
          end)

          for kind,type in pairs(self.schemas) do
            types:push { type.obj.member.list:len(), {kind, type.obj} }
          end

          table.sort(types.arr, function(a,b) return a[1] > b[1] end)

          c:append("Kind best_kind = Kind::Unknown;")
          c:append("u32 best_kind_match_count = 0;")

          types:each(function(type)
            local typename = type[2][1]
            local typedef = type[2][2]

            c:beginScope()
            do
              c:append("u32 match_count = 0;")
              typedef.member.list:eachWithIndex(function(member)
                c:beginIf('obj->findMember("'..member.name..'"_str)')
                do
                  c:append("match_count += 1;")
                end
                c:endIf()
              end)

              c:beginIf("match_count > best_kind_match_count")
              do
                c:append("best_kind_match_count = match_count;")
                c:append("best_kind = Kind::",typename,";")
              end
              c:endIf()
            end
            c:endScope()
          end)

          c:beginIf("best_kind == Kind::Unknown")
          do
            c:append(
              'ERROR("could not resolve type of variant \'',name,'\'");')
            c:append("return false;")
          end
          c:endIf()

          c:append("this->kind = best_kind;")

          c:beginSwitch("this->kind")
          do
            types:each(function(type)
              local typename = type[2][1]
              local typedef = type[2][2]
              c:beginCase("Kind::"..typename)
              c:beginIf("!as"..typename..".deserialize(obj,allocator)")
              do
                c:append(
                  'ERROR("failed to deserialize \'',name,'\' as \'',typename,
                  '\'");')
                c:append("return false;")
              end
              c:endIf()
              c:endCase()
            end)
          end
          c:endSwitch()
        end
        c:endScope()
        c:endCase()
      end
      c:endSwitch()

      c:append("return true;")
    end
    c:endFunction()

    c:beginFunction(
      "b8", "serialize", 
      "json::JSON* json", "json::Object* obj", "mem::Allocator* allocator")
    do
      c:append("using namespace json;")

      c:beginSwitch("this->kind")
      do
        local primHandler = function(kind, vmem, mem)
          return function()
            c:beginCase("Kind::"..kind)
            newValue(c, name, kind, "obj", function(vname)
              c:append(vname,"->",vmem," = ",mem,";")
            end)
            c:endCase()
          end
        end

        local handlers =
        {
          Boolean = primHandler("Boolean", "boolean", "asBoolean"),
          Number = primHandler("Number", "number", "(f32)asNumber"),
          String = primHandler("String", "string", "asString")
        }

        for kind in pairs(self.primitives) do
          local handler = handlers[kind]
          if handler then
            handler()
          else
            error("no handler for primitive kind "..kind)
          end
        end

        self.objects:each(function(o)
          c:beginCase("Kind::"..o.name)
          newValue(c, name, "Object", "obj", function(vname)
            c:beginIf(
              "!as"..o.name..".serialize(json,&"..vname.."->object,allocator)")
            do
              c:append(
                'ERROR("failed to serialize \'',name,'\' as \'', o.name, 
                '\'");')
              c:append("return false;")
            end
            c:endIf()
          end)
          c:endCase()
        end)

        for kind in pairs(self.schemas) do
          c:beginCase("Kind::"..kind)
          newValue(c, name, "Object", "obj", function(vname)
            c:beginIf(
              "!as"..kind..".serialize(json,&"..vname.."->object,allocator)")
            do
              c:append(
                'ERROR("failed to serialize \'',name,'\' as \'', kind, 
                '\'");')
              c:append("return false;")
            end
            c:endIf()
          end)
          c:endCase()
        end
      end
      c:endSwitch()

      c:append("return true;")
    end
    c:endFunction()

    for kind in pairs(self.primitives) do
      c:beginFunction(
        "decltype(as"..kind..")", "getAs"..kind)
      do
        c:append("assert(kind == Kind::",kind,");")
        c:append("return as",kind,";")
      end
      c:endFunction()
      c:beginFunction("void", "setAs"..kind, "decltype(as"..kind..") v")
      do
        c:append("kind = Kind::",kind,";")
        c:append("as",kind," = v;")
      end
      c:endFunction()
    end

    self.objects:each(function(o)
      c:beginFunction("decltype(as"..o.name..")*", "getAs"..o.name)
      do
        c:append("assert(kind == Kind::",o.name,");")
        c:append("return &as",o.name,";")
      end
      c:endFunction()
      c:beginFunction("decltype(as"..o.name..")*", "setAs"..o.name)
      do
        c:append("kind = Kind::",o.name,";")
        c:append("return &as",o.name,";")
      end
      c:endFunction()
    end)

    for kind,type in pairs(self.schemas) do
      c:beginFunction("decltype(as"..kind..")*", "getAs"..kind)
      do
        c:append("assert(kind == Kind::",kind,");")
        c:append("return &as",kind,";")
      end
      c:endFunction()
      c:beginFunction("decltype(as"..kind..")*", "setAs"..kind)
      do
        c:append("kind = Kind::",kind,";")
        c:append("return &as",kind,";")
      end
      c:endFunction()
    end
  end
  c:endStruct()

  c:append("using ",name,"Kind = ",typename,"::Kind;")

  c:appendStructMember(typename, name, "{}")
end

Variant.__call = function(self, name)
  self.name = name
  if self.prev then
    self.prev:addMember(self, name)
  end
  return self
end

Variant.__index = function(self, key)
  local handler = Variant.handlers[key]
  if handler then 
    return handler(self)
  end

  local member = rawget(Variant, key)
  if member then
    return member
  end
end

Variant.done = function(self)
  return self.prev
end

---@param c CGen
Variant.writeDeserializer = function(self, c, name, out)
  findValue(c, name, nil, function(vname)
    c:beginIf("!"..name..".deserialize("..vname..", allocator)")
    do
      c:append('ERROR("failed to deserialize \'',name,'\'");')
      c:append("return false;")
    end
    c:endIf()
  end)
end

---@param c CGen
Variant.writeSerializer = function(self, c, name)
  c:beginIf("!"..name..".serialize(json,obj,allocator)")
  do
    c:append('ERROR("failed to serialize \'',name,'\'\\n");')
    c:append("return false;")
  end
  c:endIf()
end

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

  Variant.handlers[name] = function(variant)
    if variant.schemas[name] then
      error("the schema "..name.." has already been used in this variant!")
    end
    variant.schemas[name] = o
    return variant
  end

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
    if member.type.writeStructSubtype then
      member.type:writeStructSubtype(c, member.name)
    end

    member.type:writeStructMember(c, member.name)
  end)

  c:beginFunction(
    "b8", "deserialize", "json::Object* obj", "mem::Allocator* allocator")
  do
    c:append("using namespace json;")
    self.obj.member.list:each(function(member)
      if member.type.writeDeserializer then
        member.type:writeDeserializer(c, member.name, "this")
      else
        print("missing deserializer for member "..member.name.."!")
      end
    end)
    c:append("return true;")
  end
  c:endFunction()

  c:beginFunction(
    "b8", "serialize", 
    "json::JSON* json", "json::Object* obj","mem::Allocator* allocator")
  do
    c:append("using namespace json;")
    self.obj.member.list:each(function(member)
      if member.type.writeSerializer then
        member.type:writeSerializer(c, member.name)
      else
        print("missing serializer for member "..member.name.."!")
      end
    end)
    c:append("return true;")
  end
  c:endFunction()

  c:endStruct()
end

Schema.writeDeserializer = function(self, c, name, out)
  findValue(c, name, "Object", function(vname)
    c:beginIf("!"..name..".deserialize(&"..vname.."->object, allocator)")
    do
      c:append('ERROR("failed to deserialize \'',name,'\'\\n");')
      c:append("return false;")
    end
    c:endIf()
  end)
end

Schema.writeSerializer = function(self, c, name)
  newValue(c, name, "Object", "obj", function(vname)
    c:beginIf("!"..name..".serialize(json,&"..vname.."->object,allocator)")
    do
      c:append('ERROR("failed to serialize \'',name,'\'\\n");')
      c:append("return false;")
    end
    c:endIf()
  end)
end

-- * --------------------------------------------------------------------------
-- @type_defs

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

    Variant.handlers[key] = function(variant)
      if variant.primitives[value.variant_kind] then
        error(
          "the json kind "..value.variant_kind.." has already been "..
          "assigned to this variant")
      end
      variant.primitives[value.variant_kind] = T
      return variant
    end

    rawset(self, key, setmetatable(T, { __index = value, }))
  end
})

Types.Bool =
{
  variant_kind = "Boolean",
  ---@param c CGen
  writeStructMember = function(_, c, name)
    c:appendStructMember("b8", name, "false")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, out)
    findValue(c, name, "Boolean", function(vname)
      c:append(out,"->",name," = ",vname,"->boolean;")
    end)
  end,
  ---@param c CGen
  writeSerializer = function(_, c, name)
    newValue(c, name, "Boolean", "obj", function(vname)
      c:append(vname,"->boolean = this->",name,";")
    end)
  end,
  ---@param c CGen
  writeVariantDeserializerCase = function(_, c, name, vname, out)
    c:append(out,"->",name," = ",vname,"->boolean;")
  end,
}

Types.Int = 
{
  variant_kind = "Number",
  ---@param c CGen
  writeStructMember = function(_, c, name)
    c:appendStructMember("s32", name, "0")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, out)
    findValue(c, name, "Number", function(vname)
      c:append(out,"->",name," = (s32)",vname,"->number;")
    end)
  end,
  ---@param c CGen
  writeSerializer = function(_, c, name)
    newValue(c, name, "Number", "obj", function(vname)
      c:append(vname,"->number = this->",name,";")
    end)
  end,
  ---@param c CGen
  writeVariantDeserializerCase = function(_, c, name, vname, out)
    c:append(out,"->",name," = (s32)",vname,"->number;")
  end,
}

Types.UInt = 
{
  variant_kind = "Number",
  ---@param c CGen
  writeStructMember = function(_, c, name)
    c:appendStructMember("u32", name, "0")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, out)
    findValue(c, name, "Number", function(vname)
      c:append(out,"->",name," = (u32)",vname,"->number;")
    end)
  end,
  ---@param c CGen
  writeSerializer = function(_, c, name)
    newValue(c, name, "Number", "obj", function(vname)
      c:append(vname,"->number = this->",name,";")
    end)
  end,
  ---@param c CGen
  writeVariantDeserializerCase = function(_, c, name, vname, out)
    c:append(out,"->",name," = (u32)",vname,"->number;")
  end,
}

Types.String = 
{
  variant_kind = "String",
  ---@param c CGen
  writeStructMember = function(_, c, name)
    c:appendStructMember("str", name, "nil")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, out)
    findValue(c, name, "String", function(vname)
      c:append(out,"->",name," = ",vname,"->string.allocateCopy(allocator);")
    end)
  end,
  ---@param c CGen
  writeSerializer = function(_, c, name)
    newValue(c, name, "String", "obj", function(vname)
      c:append(vname,"->string = this->",name,".allocateCopy(allocator);")
    end)
  end,
  ---@param c CGen
  writeVariantDeserializerCase = function(_, c, name, vname, out)
    c:append(out,"->",name," = ",vname,"->string;")
  end,
}

Types.StringArray = 
{
  variant_kind = "Array",
  ---@param c CGen
  writeStructMember = function(_, c, name)
    c:appendStructMember("Array<str>", name, "nil")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, out)
    findValue(c, name, "Array", function(vname)
      local len = name.."_len"
      local values = vname.."->array.values"

      c:append("s32 "..len.." = "..values..".len();")

      c:beginIf("!"..out.."->"..name..".init("..len..", allocator)")
      do
        c:append(
          "ERROR(\"failed to initialize array for value '"..name.."'\");")
        c:append("return false;")
      end
      c:endIf()

      local idx = name.."_idx"
      local elem = name.."_elem"
      c:beginForLoop(
        "s32 "..idx.." = 0",
        idx.." < "..len,
        "++"..idx)
      do
        c:append("Value* ", elem, " = ", values, "[", idx, "];")
        c:beginIf(elem.."->kind != Value::Kind::String")
        do
          c:append(
            "ERROR(\"unexpected type in string array, got \", ",
            "getValueKindString(", elem, "->kind), \"\\n\");")
          c:append("return false;")
        end
        c:endIf()
        c:append(
          out,"->",name,".push(",elem,"->string.allocateCopy(allocator));")
      end
      c:endForLoop()
    end)
  end,
  ---@param c CGen
  writeSerializer = function(_, c, name)
    c:beginIf("notnil(this->"..name..")")
    do
      newArrayValue(
          c, name, "obj", 
          "this->"..name..".len()", "allocator",
      function(vname)
        c:append("json::Array* arr = &",vname,"->array;")
        c:beginForEachLoop("str& s", "this->"..name)
        do
          newArrayElemValue(c, "elem", "String", "arr", function(vname)
            c:append(vname.."->string = s.allocateCopy(&json->string_buffer);")
          end)
        end
        c:endForLoop()
      end)
    end
    c:endIf()
  end,
}

-- * --------------------------------------------------------------------------
-- @schema_array

-- TODO(sushi) make a generic array thing 

SchemaArray = {}
SchemaArray.new = function(json, tbl)
  return function(schema_name)
    local o = {}
    local T = assert(json.schemas.map[schema_name], 
      "no schema named "..schema_name)

    -- local instance = T.new()

    ---@param c CGen
    o.writeStructMember = function(_, c, name)
      c:appendStructMember("Array<"..schema_name..">", name)
    end

    ---@param c CGen
    o.writeDeserializer = function(_, c, name, out)
      findValue(c, name, "Array", function(vname)
        local len = name.."_len"
        local values = vname.."->array.values"

        c:append("s32 "..len.." = "..values..".len();")

        c:beginIf("!"..out.."->"..name..".init("..len..", allocator)")
        do
          c:append(
            "ERROR(\"failed to initialize array for value '"..name.."'\");")
          c:append("return false;")
        end
        c:endIf()

        local idx = name.."_idx"
        local elem = name.."Elem"
        c:beginForLoop(
          "s32 "..idx.." = 0",
          idx.." < "..len,
          "++"..idx)
        do
          c:append("Value* ", elem, " = ", values, "[", idx, "];")
          c:append(schema_name,"* out_elem = this->",name,".push();")

          c:beginIf(
            "!out_elem->deserialize(&"..elem.."->object,allocator)")
          do
            c:append(
              'ERROR("failed to deserialize element ", ',idx,', "\\n");')
            c:append("return false;")
          end
          c:endIf()
        end
        c:endForLoop()
      end)
    end

    ---@param c CGen
    o.writeSerializer = function(_, c, name, obj, invar)
      newArrayValue(
          c, name, "obj", 
          "this->"..name..".len()", "allocator",
      function(vname)
        c:append("json::Array* arr = &",vname,"->array;")

        local len = "arr->values.len()"
        local idx = name.."_idx"
        local elem = name.."Elem"
        c:beginForLoop(
          "s32 "..idx.." = 0",
          idx.." < "..len,
          "++"..idx)
        newArrayElemValue(c, name, "Object", "arr", function(vname)
          c:beginIf(
            "!this->"..name.."["..idx.."].serialize(json,&"..vname..
            "->object,allocator)")
          do
            c:append(
              'ERROR("failed to serialize ',name,
              '[",',idx,',"]\\n");')
            c:append("return false;")
          end
          c:endIf()
        end)
        c:endForLoop()
      end)
    end

    return setmetatable(o,
    {
      __call = function(self, name)
        tbl.member.list:push{name=name,type=self}
        tbl.member.map[name] = self
        return tbl
      end,
    })
  end
end


-- * --------------------------------------------------------------------------
-- @enum_types

local enumCall = function(self, name_or_elem)
  if not self.name then
    self.name = name_or_elem
    self.json.schemas.map[name_or_elem] = self
    self.json.schemas.list:push{name=name_or_elem,def=self}
    return self
  end

  -- otherwise collect elements
  self.elems:push(name_or_elem)

  return self
end

local enumNewInstance = function(T, enum, set)
  ---@param obj Object
  return function(self, obj)
    return setmetatable(
    {
      ---@param c CGen
      writeStructMember = function(_, c, name)
        c:appendStructMember(T.name, name, "{}")
      end,
      writeDeserializer = enum.writeDeserializer,
      writeSerializer = enum.writeSerializer,
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
              local s = 
              {
                writeStructMember = function(_, c, name)
                  c:appendStructMember(T.name.."Flags", name, "{}")
                end,
                writeDeserializer = set.writeDeserializer,
                writeSerializer = set.writeSerializer,
              }
              obj:addMember(s, name)
              return obj
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
  o.writeEnum = function(self, c)
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
    end,
    ---@param c CGen
    writeSerializer = function(_, c, name)
      local from_enum_name = getFromEnumName(o.name)
      newValue(c, name, "String", "obj", function(vname)
        c:append(
          vname,"->string = ",from_enum_name,"(this->",name,
          ").allocateCopy(&json->string_buffer);")
      end)
    end,
  },
  -- set
  {
    ---@param c CGen
    writeDeserializer = function(_, c, name, out)
      findValue(c, name, "Array", function(vname)
        local values = vname.."->array.values"
        local idx = name.."_idx"
        local elem = name.."_elem"
        local from_string_name = getFromStringName(o.name)
        c:beginForLoop(
          "s32 "..idx.." = 0",
          idx.." < "..values..".len()",
          "++"..idx)
        do
          c:append("Value* "..elem.." = "..values.."["..idx.."];")
          c:beginIf(elem.."->kind != Value::Kind::String")
          do
            c:append(
              "ERROR(\"unexpected type in string enum set, wanted string "..
              "got \", getValueKindString("..elem.."->kind), \"\\n\");")
            c:append("return false;")
          end
          c:endIf()
          c:append(
            out.."->"..name..".set("..from_string_name.."("..elem..
            "->string));")
        end
        c:endForLoop()
      end)
    end,
    ---@param c CGen
    writeSerializer = function(_, c, name)
      newValue(c, name, "Array", "obj", function(vname)
        local from_enum_name = getFromEnumName(o.name)
        c:append("json::Array* arr = &",vname,"->array;")
        c:beginForLoop(
          o.name.." kind = ("..o.name..")0",
          "(u32)kind < (u32)"..o.name.."::COUNT",
          "kind = ("..o.name..")((u32)kind + 1)")
        do
          newArrayElemValue(c, name, "String", "arr", function(vname)
            c:append(
              vname,"->string = ",from_enum_name,
              "(kind).allocateCopy(&json->string_buffer);")
          end)
        end
        c:endForLoop()
      end)
    end,
  })

  return setmetatable(o, StringedEnum)
end

NumericEnum = {}
NumericEnum.__call = enumCall
NumericEnum.new = function(json)
  local o = {}
  o.json = json
  o.elems = List{}
  o.newInstance = enumNewInstance(o, 
  {
    ---@param c CGen
    writeDeserializer = function(_, c, name, out)
      findValue(c, name, "Number", function(vname)
        c:append(out,"->",name," = (",o.name,")(",vname,"->number);")
      end)
    end,
    ---@param c CGen
    writeSerializer = function(_, c, name)
      newValue(c, name, "Number", "obj", function(vname)
        c:append(vname,"->number = (s32)this->",name,";")
      end)
    end,
  },
  {
    ---@param c CGen
    writeDeserializer = function(_, c, name, out)
      findValue(c, name, "Array", function(vname)
         local values = vname.."->array.values"
        local idx = name.."_idx"
        local elem = name.."_elem"
        c:beginForLoop(
          "s32 "..idx.." = 0",
          idx.." < "..values..".len()",
          "++"..idx)
        do
          c:append("Value* "..elem.." = "..values.."["..idx.."];")
          c:beginIf(elem.."->kind != Value::Kind::Number")
          do
            c:append(
              "ERROR(\"unexpected type in numeric enum set, wanted number "..
              "got \", getValueKindString("..elem.."->kind), \"\\n\");")
            c:append("return false;")
          end
          c:endIf()
          c:append(out,"->",name,".set((",o.name,")(",elem,"->number));")
        end
        c:endForLoop()
      end)
    end,
    ---@param c CGen
    writeSerializer = function(_, c, name)
      newValue(c, name, "Array", "obj", function(vname)
        c:append("json::Array* arr = &",vname,"->array;")
        c:beginForLoop(
          o.name.." kind = ("..o.name..")0",
          "(u32)kind < (u32)"..o.name.."::COUNT",
          "kind = ("..o.name..")((u32)kind + 1)")
        do
          c:beginIf("this->"..name..".test(kind)")
          do
            newArrayElemValue(c, "elem", "Number", "arr", function(vname)
              c:append(vname,"->number = (s32)kind;")
            end)
          end
          c:endIf()
        end
        c:endForLoop()
      end)
    end,
  })

  ---@param c CGen
  o.writeEnum = function(self, c)
    c:beginEnum(self.name)
    do
      self.elems:each(function(elem)
        c:appendEnumElement(elem)
      end)
      c:appendEnumElement "COUNT"
    end
    c:endEnum()
    c:typedef("Flags<"..self.name..">", self.name.."Flags")
  end

  return setmetatable(o, NumericEnum)
end

-- * --------------------------------------------------------------------------
-- @object_type

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
  local json = self.json
  local handlers =
  {
    Object = function()
      return Object.new(self, self.json)
    end,
    Variant = function()
      return Variant.new(self, self.json)
    end,
    SchemaArray = function()
      return SchemaArray.new(self.json, self)
    end,
    extends = function()
      return setmetatable(
      {
        bases = List{},
        done = function()
          return self
        end,
      },
      {
        __index = function(self, key)
          local schema = json.schemas.map[key]
          if not schema then
            error("'extends' expects a schema name")
          end
          self.bases:push(schema)
        end
      })
    end
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
Object.writeStructSubtype = function(self, c, name)
  local typename = "obj_"..name

  c:beginStruct(typename)
  do
    self.member.list:each(function(member)
      if member.type.writeStructSubtype then
        member.type:writeStructSubtype(c, member.name)
      end

      member.type:writeStructMember(c, member.name)
    end)

    c:beginFunction(
      "b8", "deserialize", "json::Object* obj", "mem::Allocator* allocator")
    do
      c:append("using namespace json;")
      self.member.list:each(function(member)
        if member.type.writeDeserializer then
          member.type:writeDeserializer(c, member.name, "this")
        else
          print("missing deserializer for member "..member.name.."!")
        end
      end)
      c:append("return true;")
    end
    c:endFunction()

    c:beginFunction(
      "b8", "serialize", 
      "json::JSON* json", "json::Object* obj", "mem::Allocator* allocator")
    do
      c:append("using namespace json;")
      self.member.list:each(function(member)
        if member.type.writeSerializer then
          member.type:writeSerializer(c, member.name)
        else
          print("missing serializer for member "..member.name.."!")
        end
      end)
      c:append("return true;")
    end
    c:endFunction()
  end
  c:endStruct()
end

---@param c CGen
Object.writeStructMember = function(self, c, name)
  local typename = "obj_"..name

  c:appendStructMember(typename, name, "{}")
end

Object.writeDeserializer = function(self, c, name, out)
  findValue(c, name, "Object", function(vname)
    c:beginIf("!"..name..".deserialize(&"..vname.."->object, allocator)")
    do
      c:append('ERROR("failed to deserialize \'',name,'\'\\n");')
      c:append("return false;")
    end
    c:endIf()
  end)
end

Object.writeSerializer = function(self, c, name)
  newValue(c, name, "Object", "obj", function(vname)
    c:beginIf("!"..name..".serialize(json,&"..vname.."->object,allocator)")
    do
      c:append('ERROR("failed to serialize \'',name,'\'\\n");')
      c:append("return false;")
    end
    c:endIf()
  end)
end

Object.__call = function(self, name)
  return self:addSelfToPrev(name)
end

Object.done = function(self)
  return self.prev
end

Variant.handlers["Object"] = function(variant)
  local o = Object.new(variant, variant.json)
  return function(name)
    variant.objects:push { name=name, def=o }
    return o
  end
end

-- * --------------------------------------------------------------------------
-- @schema_defs

json.Schema "Position"
  .UInt "line"
  .UInt "character"

json.Schema "Range"
  .Position "start"
  .Position "end"

json.Schema "Test"
  .Variant "v"
    .Position
    .Object "Obj"
      .Bool "hi"
      .Variant "erg"
        .Bool
        .UInt
        :done()
      :done()
    .Bool
    .UInt
    .Range
    :done()

-- helper that just appends 'ClientCapabilities' to the end
-- of the given name bc i am lazy
local CC = function(name)
  return json.Schema(name.."ClientCapabilities")
end

-- helper for client capability schemas that only define dynamicRegistration
local CCdynReg = function(name)
  return json.Schema(name.."ClientCapabilities")
    .Bool "dynamicRegistration"
end

json.StringedEnum "ResourceOperationKind"
  { Create = "create" }
  { Rename = "rename" }
  { Delete = "delete" }

json.StringedEnum "FailureHandlingKind"
  { Abort = "abort" }
  { Transactional = "transactional" }
  { TextOnlyTransactional = "textOnlyTransactional" }
  { Undo = "undo" }

json.StringedEnum "MarkupKind"
  { PlainText = "plaintext" }
  { Markdown = "markdown" }

json.NumericEnum "SymbolKind"
  "File"
  "Module"
  "Namespace"
  "Package"
  "Class"
  "Method"
  "Property"
  "Field"
  "Constructor"
  "Enum"
  "Interface"
  "Function"
  "Variable"
  "Constant"
  "String"
  "Number"
  "Boolean"
  "Array"
  "Object"
  "Key"
  "Null"
  "EnumMember"
  "Struct"
  "Event"
  "Operator"
  "TypeParameter"

json.NumericEnum "InsertTextMode"
  "AsIs"
  "AdjustIndentation"

json.NumericEnum "CompletionItemKind"
  "Text"
  "Method"
  "Function"
  "Constructor"
  "Field"
  "Variable"
  "Class"
  "Interface"
  "Module"
  "Property"
  "Unit"
  "Value"
  "Enum"
  "Keyword"
  "Snippet"
  "Color"
  "File"
  "Reference"
  "Folder"
  "EnumMember"
  "Constant"
  "Struct"
  "Event"
  "Operator"
  "TypeParameter"

CC "WorkspaceEdit"
  .Bool "documentChanges"
  .Bool "normalizesLineEndings"
  .Object "changeAnnotationSupport"
    .Bool "groupsOnLabel"
    :done()
  .ResourceOperationKind.Set "resourceOperations"
  .FailureHandlingKind "failureHandling"
  :done()

CCdynReg "DidChangeConfiguration"

CCdynReg "DidChangeWatchedFiles"
  .Bool "relativePathSupport"

CCdynReg "WorkspaceSymbol"
  .Object "symbolKind"
    .SymbolKind.Set "valueSet"

CCdynReg "ExecuteCommand"

local CCrefreshSupport = function(name)
  return json.Schema(name.."ClientCapabilities")
    .Bool "refreshSupport"
end

CCrefreshSupport "SemanticTokensWorkspace"
CCrefreshSupport "CodeLensWorkspace"
CCrefreshSupport "InlineValueWorkspace"
CCrefreshSupport "InlayHintWorkspace"
CCrefreshSupport "DiagnosticWorkspace"

CCdynReg "TextDocumentSync"
  .Bool "willSave"
  .Bool "willSaveWaitUntil"
  .Bool "didSave"

CCdynReg "Completion"
  .Bool "contextSupport"
  .Bool "insertTextMode"
  .Object "completionItem"
    .Bool "snippetSupport"
    .Bool "commitCharactersSupport"
    .Bool "deprecatedSupport"
    .Bool "preselectSupport"
    .Bool "insertReplaceSupport"
    .Bool "labelDetailsSupport"
    .MarkupKind.Set "documentationFormat"
    .Object "resolveSupport"
      .StringArray "properties"
      :done()
    .Object "insertTextModeSupport"
      .InsertTextMode.Set "valueSet"
      :done()
    :done()
  .Object "completionItemKind"
    .CompletionItemKind.Set "valueSet"
    :done()
  .Object "completionList"
    .StringArray "itemDefaults"
    :done()

CCdynReg "Hover"
  .MarkupKind.Set "contentFormat"

CCdynReg "SignatureHelp"
  .Object "signatureInformation"
    .MarkupKind.Set "documentationFormat"
    .Object "parameterInformation"
      .Bool "labelOffsetSupport"
      :done()
    .Bool "activeParameterSupport"
    :done()
  .Bool "contextSupport"

local CClinkSupport = function(name)
  return CCdynReg(name).Bool "linkSupport"
end

CClinkSupport "Declaration"
CClinkSupport "Definition"
CClinkSupport "TypeDefinition"
CClinkSupport "Implementation"

CCdynReg "Reference"
CCdynReg "DocumentHighlight"

CCdynReg "DocumentSymbol"
  .Bool "hierarchicalDocumentSymbolSupport"
  .Bool "labelSupport"
  .Object "symbolKind"
    .SymbolKind.Set "valueSet"
    :done()

json.StringedEnum "CodeActionKind"
  { Empty = "" }
  { QuickFix = "quickfix" }
  { Refactor = "refactor" }
  { RefactorExtract = "refactor.extract" }
  { RefactorInline = "refactor.inline" }
  { RefactorRewrite = "refactor.rewrite" }
  { Source = "source" }
  { SourceOrganizeImports = "source.organizeImports" }
  { SourceFixAll = "source.fixAll" }

CCdynReg "CodeAction"
  .Bool "isPreferredSupport"
  .Bool "disabledSupport"
  .Bool "dataSupport"
  .Bool "honorsChangeAnnotations"
  .Object "codeActionLiteralSupport"
    .Object "codeActionKind"
      .CodeActionKind.Set "valueSet"
      :done()
    :done()
  .Object "resolveSupport"
    .StringArray "properties"
    :done()

CCdynReg "CodeLens"

CCdynReg "DocumentLink"
  .Bool "tooltipSupport"

CCdynReg "DocumentColor"
CCdynReg "DocumentFormatting"
CCdynReg "DocumentRangeFormatting"
CCdynReg "DocumentOnTypeFormatting"

CCdynReg "Rename"
  .Bool "prepareSupport"
  .Bool "honorsChangeAnnotations"

json.NumericEnum "DiagnosticTag"
  "Unnecessary"
  "Deprecated"

CC "PublishDiagnostics"
  .Bool "relatedInformation"
  .Bool "versionSupport"
  .Bool "codeDescriptionSupport"
  .Bool "dataSupport"
  .Object "tagSupport"
    .DiagnosticTag.Set "valueSet"
    :done()

json.StringedEnum "FoldingRangeKind"
  { Comment = "comment" }
  { Imports = "imports" }
  { Region  = "region" }

CCdynReg "FoldingRange"
  .UInt "rangeLimit"
  .Bool "lineFoldingOnly"
  .Object "foldingRangeKind"
    .FoldingRangeKind.Set "valueSet"
    :done()
  .Object "foldingRange"
    .Bool "collapsedText"
    :done()

CCdynReg "SelectionRange"
CCdynReg "LinkedEditingRange"
CCdynReg "CallHierarchy"

json.StringedEnum "TokenFormat"
  { Relative = "relative" }

CCdynReg "SemanticTokens"
  .Object "requests"
    .Variant "range"
      .Bool
      .Object "Empty"
        :done()
      :done()
    .Variant "full"
      .Bool
      .Object "WithDelta"
        .Bool "delta"
        :done()
      :done()
    :done()
  .StringArray "tokenTypes"
  .StringArray "tokenModifiers"
  .TokenFormat.Set "formats"
  .Bool "overlappingTokenSupport"
  .Bool "multilineTokenSupport"
  .Bool "serverCancelSupport"
  .Bool "augmentsSyntaxTokens"

CCdynReg "Moniker"
CCdynReg "TypeHierarchy"
CCdynReg "InlineValue"
CCdynReg "InlayHint"

CCdynReg "Diagnostic"
  .Bool "relatedDocumentSupport"

CC "TextDocument"
  .TextDocumentSyncClientCapabilities "synchronization"
  .CompletionClientCapabilities "completion"
  .HoverClientCapabilities "hover"
  .SignatureHelpClientCapabilities "signatureHelp"
  .DeclarationClientCapabilities "declaration"
  .DefinitionClientCapabilities "definition"
  .TypeDefinitionClientCapabilities "typeDefinition"
  .ImplementationClientCapabilities "implementation"
  .ReferenceClientCapabilities "references"
  .DocumentHighlightClientCapabilities "documentHighlight"
  .DocumentSymbolClientCapabilities "documentSymbol"
  .CodeActionClientCapabilities "codeAction"
  .CodeLensClientCapabilities "codeLens"
  .DocumentLinkClientCapabilities "documentLink"
  .DocumentColorClientCapabilities "colorProvider"
  .DocumentFormattingClientCapabilities "formatting"
  .DocumentRangeFormattingClientCapabilities "rangeFormatting"
  .DocumentOnTypeFormattingClientCapabilities "onTypeFormatting"
  .RenameClientCapabilities "rename"
  .PublishDiagnosticsClientCapabilities "publishDiagnostics"
  .FoldingRangeClientCapabilities "foldingRange"
  .SelectionRangeClientCapabilities "selectionRange"
  .LinkedEditingRangeClientCapabilities "linkedEditingRange"
  .CallHierarchyClientCapabilities "callHierarchy"
  .SemanticTokensClientCapabilities "semanticTokens"
  .MonikerClientCapabilities "moniker"
  .TypeHierarchyClientCapabilities "typeHierarchy"
  .InlineValueClientCapabilities "inlineValue"
  .InlayHintClientCapabilities "inlayHint"
  .DiagnosticClientCapabilities "diagnostic"

CC "ShowMessageRequest"
  .Object "messageActionItem"
    .Bool "additionalPropertiesSupport"
    :done()

CC "ShowDocument"
  .Bool "support"

CC "RegularExpressions"
  .String "engine"
  .String "version"

CC "Markdown"
  .String "parser"
  .String "version"
  .StringArray "allowedTags"

json.StringedEnum "PositionEncodingKind"
  { UTF8  = "utf-8"  }
  { UTF16 = "utf-16" }
  { UTF32 = "utf-32" }

json.Schema "ClientCapabilities"
  .Object "workspace"
    .Bool "applyEdit"
    .Bool "workspaceFolders"
    .Bool "configuration"
    .Object "fileOperations"
      .Bool "dynamicRegistration"
      .Bool "didCreate"
      .Bool "willCreate"
      .Bool "didRename"
      .Bool "willRename"
      .Bool "didDelete"
      .Bool "willDelete"
      :done()
    .WorkspaceEditClientCapabilities "workspaceEdit"
    .DidChangeConfigurationClientCapabilities "didChangeConfiguration"
    .DidChangeWatchedFilesClientCapabilities "didChangeWatchedFiles"
    .WorkspaceSymbolClientCapabilities "symbol"
    .ExecuteCommandClientCapabilities "executeCommand"
    .SemanticTokensWorkspaceClientCapabilities "semanticTokens"
    .CodeLensWorkspaceClientCapabilities "codeLens"
    .InlineValueWorkspaceClientCapabilities "inlineValue"
    .InlayHintWorkspaceClientCapabilities "inlayHint"
    .DiagnosticWorkspaceClientCapabilities "diagnostics"
    :done()
  .Object "window"
    .Bool "workDoneProgress"
    .ShowMessageRequestClientCapabilities "showMessage"
    .ShowDocumentClientCapabilities "showDocument"
    :done()
  .Object "general"
    .Object "staleRequestSupport"
      .Bool "cancal"
      .StringArray "retryOnContentModified"
      :done()
    .RegularExpressionsClientCapabilities "regularExpressions"
    .MarkdownClientCapabilities "markdown"
    .PositionEncodingKind.Set "positionEncodings"
    :done()
  .TextDocumentClientCapabilities "textDocument"

json.Schema "InitializeParams"
  .Int "processId"
  .Object "clientInfo"
    .String "name"
    .String "version"
    :done()
  .String "locale"
  .String "rootUri"
  .String "rootPath"
  .ClientCapabilities "capabilities"
  :done()

json.NumericEnum "TextDocumentSyncKind"
  "None"
  "Full"
  "Incremental"

json.Schema "CompletionOptions"
  .StringArray "triggerCharacters"
  .StringArray "allCommitCharacters"
  .Bool "resolveProvider"
  .Object "completionItem"
    .Bool "labelDetailsSupport"
    :done()

json.Schema "SignatureHelpOptions"
  .StringArray "triggerCharacters"
  .StringArray "retriggerCharacters"

json.Schema "CodeLensOptions"
  .Bool "resolveProvider"

json.Schema "DocumentLinkOptions"
  .Bool "resolveProvider"

json.Schema "DocumentOnTypeFormattingOptions"
  .String "firstTriggerCharacter"
  .StringArray "moreTriggerCharacter"

json.Schema "ExecuteCommandOptions"
  .StringArray "commands"

json.Schema "SemanticTokensLegend"
  .StringArray "tokenTypes"
  .StringArray "tokenModifiers"

json.Schema "SemanticTokensOptions"
  .SemanticTokensLegend "legend"
  .Variant "range"
    .Bool
    .Object "Empty"
      :done()
    :done()
  .Variant "full"
    .Bool
    .Object "WithDelta"
      .Bool "delta"
      :done()
    :done()

json.Schema "DiagnosticOptions"
  .String "identifier"
  .Bool "interFileDependencies"
  .Bool "workspaceDiagnostics"

json.Schema "WorksapceFoldersServerCapabilities"
  .Bool "supported"
  .Variant "changeNotifications"
    .String
    .Bool
    :done()

json.StringedEnum "FileOperationPatternKind"
  { File = "file" }
  { Folder = "folder" }

json.Schema "FileOperationPatternOptions"
  .Bool "ignoreCase"

json.Schema "FileOperationPattern"
  .String "glob"
  .FileOperationPatternKind "matches"
  .FileOperationPatternOptions "options"

json.Schema "FileOperationFilter"
  .String "scheme"
  .FileOperationPattern "pattern"

json.Schema "FileOperationRegistrationOptions"
  .SchemaArray "FileOperationFilter" "filters"

json.Schema "ServerCapabilities"
  .PositionEncodingKind "positionEncoding"
  .TextDocumentSyncKind "textDocumentSync"
  .CompletionOptions "completionProvider"
  .SignatureHelpOptions "signatureHelpProvider"
  .CodeLensOptions "codeLensProvider"
  .DocumentLinkOptions "documentLinkProvider"
  .DocumentOnTypeFormattingOptions "documentOnTypeFormattingProvider"
  .ExecuteCommandOptions "executeCommandProvider"
  .SemanticTokensOptions "semanticTokensProvider"
  .DiagnosticOptions "diagnosticProvider"
  .Bool "hoverProvider"
  .Bool "declarationProvider"
  .Bool "definitionProvider"
  .Bool "typeDefinitionProvider"
  .Bool "implementationProvider"
  .Bool "referencesProvider"
  .Bool "documentHighlightProvider"
  .Bool "documentSymbolProvider"
  .Bool "codeActionProvider"
  .Bool "colorProvider"
  .Bool "documentFormattingProvider"
  .Bool "documentRangeFormattingProvider"
  .Bool "renameProvider"
  .Bool "foldingRangeProvider"
  .Bool "selectionRangeProvider"
  .Bool "linkedEditingRangeProvider"
  .Bool "callHierarchyProvider"
  .Bool "monikerProvider"
  .Bool "typeHierarchyProvider"
  .Bool "inlineValueProvider"
  .Bool "inlayHintProvider"
  .Bool "workspaceSymbolProvider"
  .Object "workspace"
    .WorksapceFoldersServerCapabilities "workspaceFolders"
    .Object "fileOperations"
      .FileOperationRegistrationOptions "didCreate"
      .FileOperationRegistrationOptions "willCreate"
      .FileOperationRegistrationOptions "didRename"
      .FileOperationRegistrationOptions "willRename"
      .FileOperationRegistrationOptions "didDelete"
      .FileOperationRegistrationOptions "willDelete"
      :done()
    :done()

json.Schema "InitializeResult"
  .ServerCapabilities "capabilities"
  .Object "serverInfo"
    .String "name"
    .String "version"
    :done()

json.Schema "TextDocumentItem"
  .String "uri"
  .String "languageId"
  .Int "version"
  .String "text"

json.Schema "DidOpenTextDocumentParams"
  .TextDocumentItem "textDocument"

-- * --------------------------------------------------------------------------
-- @schema_writing

local c = CGen.new()
json.schemas.list:each(function(schema)
  local def = schema.def
  if def.writeStruct then
    def:writeStruct(c)
  elseif def.writeEnum then
    def:writeEnum(c)
  else
    error("no known write function for schema '"..schema.name.."'")
  end
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

local inst_list = {}

for k,v in pairs(inst) do
  table.insert(inst_list, {k,v})
end

table.sort(inst_list, function(a, b) return a[2] < b[2] end)

for _,v in ipairs(inst_list) do
  print(v[1], v[2])
end
