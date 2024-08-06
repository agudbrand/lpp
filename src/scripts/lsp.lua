
-- Script for generating C types and de/serializers for those types
-- based on JSON schemas. Currently built specifically for lpp's language
-- server, but it may be useful to pull out and make more general later on.

-- TODO(sushi) elua needs to be able to pass arguments to scripts so 
--             that this isnt hardcoded.
local output_path = "src/generated/lsp.h"

local buffer = require "string.buffer"
local List = require "list"
local CGen = require "cgen"

local Schema,
      StringedEnum,
      NumericEnum,
      Table,
      Member

local json = {}
json.user_schema =
{
  map = {},
  list = List{},
}

-- debug.sethook(function(event, line)
--   local s = debug.getinfo(2).short_src
--   print(s..":"..line)
-- end, "l")

local Union = {}

Union.handlers = {}

Union.new = function(prev, json)
  return setmetatable(
  {
    json = json,
    prev = prev,
    types = {}
  }, Union)
end

---@param c CGen
Union.toC = function(self, c, name)
  c:beginUnion()
  do
    for value_kind,type in pairs(self.types) do
      type:toC(c, name.."_"..value_kind)
    end
  end
  c:endUnion()
end

---@param c CGen
Union.writeDeserializer = function(self, c, name, obj, out)
  local vname = name.."Value"
  c:beginIf("Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
  do
    c:beginSwitch(vname.."->kind")
    do
      for value_kind,type in pairs(self.types) do
        local handlers =
        {
          boolean = function()
            c:beginCase("Value::Kind::Boolean")
            do
              type:toUnionCase(c, name.."_boolean", vname, out)
            end
            c:endCase()
          end,
          number = function()
            c:beginCase("Value::Kind::Number")
            do
              type:toUnionCase(c, name.."_number", vname, out)
            end
            c:endCase()
          end,
          table = function()
            c:beginCase("Value::Kind::Object")
            do
              c:beginScope()
              type:toUnionCase(c, name.."_table", vname, out)
              c:endScope()
            end
            c:endCase()
          end,
        }

        local handler = handlers[value_kind]
        if handler then
          handler()
        else
          error("no handler for union value kind '"..value_kind.."'")
        end
      end
    end
    c:endSwitch()
  end
  c:endIf()
end

Union.__call = function(self, name)
  self.name = name
  if self.prev then
    self.prev.member.list:push{ name=name,type=self }
    self.prev.member.map[name] = self
  end
  return self
end

Union.__index = function(self, key)
  local handler = Union.handlers[key]
  if handler then
    return handler(self)
  end

  local member = rawget(Union, key)
  if member then
    return member
  end
end

Union.done = function(self)
  return self.prev
end

local TableType = setmetatable({},
{
  __newindex = function(self, key, value)
    local T = {}
    T.name = key
    T.new = function(tbl)
      return setmetatable({tbl=tbl}, T)
    end
    T.__call = function(self, name)
      self.tbl.member.list:push{name=name,type=self}
      self.tbl.member.map[name] = self
      return self.tbl
    end
    T.isTypeOf = function(x)
      return T == getmetatable(x)
    end
    T.toC = value.toC
    T.writeDeserializer = value.writeDeserializer
    T.toUnionCase = value.toUnionCase
    T.__index = T
    Union.handlers[key] = function(union)
      if union.types[value.union_json_kind] then
        error(
          "the json kind "..value.union_json_kind.." has already been "..
          "assigned to this union")
      end
      union.types[value.union_json_kind] = T
      return union
    end
    rawset(self, key, T)
  end
})

TableType.Bool =
{
  union_json_kind = "boolean",
  ---@param c CGen
  toUnionCase = function(self, c, name, obj, out)
    c:append(out.."->"..name.." = "..obj.."->boolean;")
  end,
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("b8", name, "false")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
    do
      c:beginIf(vname.."->kind == Value::Kind::Boolean")
      do
        c:append(out.."->"..name.." = "..vname.."->boolean;")
      end
      c:beginElse()
      do
        c:append(
          "ERROR(\"unexpected type for value '"..name.."' "..
          "wanted boolean, got \", getValueKindString("..
          vname.."->kind), \"\\n\");")
        c:append("return false;")
      end
      c:endIf()
    end
    c:endIf()
  end
}
TableType.Int =
{
  union_json_kind = "number",
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("s32", name, "0")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
    do
      c:beginIf(vname.."->kind != Value::Kind::Number")
      do
        c:append(
          "ERROR(\"unexpected type for value '"..name.."' "..
          "wanted number, got \", getValueKindString("..
          vname.."->kind), \"\\n\");")
        c:append("return false;")
      end
      c:endIf()
      c:append(out.."->"..name.." = (s32)"..vname.."->number;")
    end
    c:endIf()
  end
}
TableType.UInt =
{
  union_json_kind = "number",
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("u32", name, "0")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
    do
      c:beginIf(vname.."->kind != Value::Kind::Number")
      do
        c:append(
          "ERROR(\"unexpected type for value '"..name.."' "..
          "wanted number, got \", getValueKindString("..
          vname.."->kind), \"\\n\");")
        c:append("return false;")
      end
      c:endIf()
      c:append(out.."->"..name.." = (u32)"..vname.."->number;")
    end
    c:endIf()
  end
}
TableType.String =
{
  union_json_kind = "string",
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("str", name, "nil")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
    do
      c:beginIf(vname.."->kind != Value::Kind::String")
      do
        c:append(
          "ERROR(\"unexpected type for value '"..name.."' "..
          "wanted string, got \", getValueKindString("..
          vname.."->kind), \"\\n\");")
        c:append("return false;")
      end
      c:endIf()
      c:append(out.."->"..name.." = "..vname.."->string;")
    end
    c:endIf()
  end
}
TableType.StringArray =
{
  union_json_kind = "array",
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("Array<str>", name, "nil")
  end,
  ---@param c CGen
  writeDeserializer = function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
    do
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
    end
    c:endIf()
  end,
}

Schema = {}
Schema.__index = Schema
Schema.new = function(json)
  local o = {}
  o.json = json
  o.tbl = Table.new(nil,json)

  -- constructor for a new instance of this schema 
  o.new = function(tbl)
    return setmetatable(
    {
      tbl=tbl,
      ---@param c CGen
      toC = function(_, c, name)
        c:appendStructMember(o.name, name, "{}")
      end,

      writeDeserializer = function(_, c, name, obj, out)
        local vname = name.."Value"
        c:beginIf(
          "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
        do
          c:beginIf(vname.."->kind != Value::Kind::Object")
          do
            c:append(
              "ERROR(\"unexpected type for value '"..name.."' expected a "..
              "table but got \", getValueKindString("..vname..
              "->kind), \"\\n\");")
            c:append("return false;")
          end
          c:endIf()

          c:beginIf(
            "!deserialize"..o.name.."(allocator, &"..vname.."->object,&"..
            out.."->"..name..")")
          do
            c:append("return false;")
          end
          c:endIf()
        end
        c:endIf()
      end
    }, o)
  end

  o.__call = function(self, name)
    self.tbl.member.list:push{name=name,type=self}
    self.tbl.member.map[name] = self
    return self.tbl
  end

  ---@param c CGen
  o.toCDef = function(self, c)
    c:beginStruct(self.name)
    self.tbl:toC(c)
    c:endStruct()
  end

  ---@param c CGen
  o.writeDeserializer = function(self, c)
    c:beginStaticFunction(
      "b8",
      "deserialize"..self.name, "mem::Allocator* allocator",
      "json::Object* root", self.name.."* out")
    do
      c:append("using namespace json;")

      self.tbl.member.list:each(function(member)
        if member.type.writeDeserializer then
          member.type:writeDeserializer(c, member.name, "root", "out")
        else
          print("missing deserializer for member "..member.name.."!")
        end
      end)
    end
    c:append("return true;")
    c:endFunction()
  end

  setmetatable(o,Schema)
  return o
end

Schema.__call = function(self, name)
  self.name = name
  self.json.user_schema.map[name] = self
  self.json.user_schema.list:push{name=name,def=self}
  return self.tbl
end

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

local enumNew = function(T, writeDeserializer, writeSetDeserializer)
  return function(tbl)
    return setmetatable(
    {
      ---@param c CGen
      toC = function(self, c, name)
        c:appendStructMember(T.name, name, "{}")
      end,
      writeDeserializer = writeDeserializer,
    },
    {
      __call = function(self, name)
        tbl.member.list:push{name=name,type=self}
        tbl.member.map[name] = self
        return tbl
      end,
      __index = function(self, key)
        if key == "Set" then
          local EnumSet = {}
          EnumSet.__index = EnumSet

          ---@param c CGen
          EnumSet.toC = function(self, c, name)
            c:appendStructMember(self.of.name.."Flags", name, "{}")
          end

          EnumSet.writeDeserializer = writeSetDeserializer

          local s = setmetatable({of=T}, EnumSet)
          return function(name)
            tbl.member.list:push{name=name,type=s}
            tbl.member.map[name] = s
            return tbl
          end
        end
      end,
    })
  end
end

StringedEnum = {}
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

  o.new = enumNew(o,
    ---@param c CGen
    -- deserializer
    function(_, c, name, obj, out)
      local vname = name.."Value"
      c:beginIf(
        "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
      do
        c:beginIf(vname.."->kind != Value::Kind::String")
        do
          c:append(
            "ERROR(\"unexpected type for value '"..name.."' wanted string "..
            "but got\", getValueKindString("..vname.."->kind), \"\\n\");")
          c:append("return false;")
        end
        c:endIf()

        c:append(
          out.."->"..name.." = "..getFromStringName(o.name).."("..vname..
          "->string);")
      end
      c:endIf()
    end,
    ---@param c CGen
    -- set deserializer
    function(self, c, name, obj, out)
      local vname = name.."Value"
      c:beginIf(
        "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
      do
        c:beginIf(vname.."->kind != Value::Kind::Array")
        do
          c:append(
            "ERROR(\"unexpected type for value '"..name.."' wanted array "..
            "but got \", getValueKindString("..vname.."->kind), \"\\n\");")
          c:append("return false;")
        end
        c:endIf()

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
      end
      c:endIf()
    end)

  return setmetatable(o, StringedEnum)
end
StringedEnum.__call = enumCall

NumericEnum = {}
NumericEnum.new = function(json)
  local o = {}
  o.json = json
  o.elems = List{}
  o.new = enumNew(o,
    ---@param c CGen
    -- deserializer
    function(_, c, name, obj, out)
      local vname = name.."Value"
      c:beginIf(
        "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
      do
        c:beginIf(vname.."->kind != Value::Kind::Number")
        do
          c:append(
            "ERROR(\"unexpected type for value '"..name.."' wanted number "..
            "but got\", getValueKindString("..vname.."->kind), \"\\n\");")
          c:append("return false;")
        end
        c:endIf()

        c:append(out.."->"..name.." = ("..o.name..")("..vname.."->number);")
      end
      c:endIf()
    end,
    ---@param c CGen
    -- set deserializer
    function(_, c, name, obj, out)
      local vname = name.."Value"
      c:beginIf(
        "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
      do
        c:beginIf(vname.."->kind != Value::Kind::Array")
        do
          c:append(
            "ERROR(\"unexpected type for value '"..name.."' wanted array "..
            "but got \", getValueKindString("..vname.."->kind), \"\\n\");")
          c:append("return false;")
        end
        c:endIf()

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
      end
      c:endIf()
    end)
  ---@param c CGen
  o.toCDef = function(self, c)
    c:beginEnum(self.name)
    do
      self.elems:each(function(elem)
        c:appendEnumElement(elem)
      end)
    end
    c:endEnum()
    c:typedef("Flags<"..self.name..">", self.name.."Flags")
  end
  return setmetatable(o, NumericEnum)
end
NumericEnum.__call = enumCall

--- An element of a Table, which has a name and a Type associated with it.
Member = {}

---@class TableMembers
---@field list List
---@field map table

--- A collection of members each with some Type.
---@class Table
--- A list and map of the members belonging to this Table.
---@field member TableMembers
--- The table this one is nested in, if any.
---@field prev Table?
--- The json module all of this is stored in.
---@field json json
Table = {}
Table.new = function(prev, json)
  return setmetatable(
  {
    json = json,
    prev = prev,
    member =
    {
      list = List{},
      map = {}
    }
  }, Table)
end

Table.__call = function(self, name)
  if self.prev then
    local member = { name=name,type=self }
    self.prev.member.list:push(member)
    self.prev.member.map[name] = self
  end
  return self
end

Table.done = function(self)
  return self.prev
end

Union.handlers["Table"] = function(union)
  local t = Table.new(union, union.json)
  union.types["table"] = t
  return t
end

---@param c CGen
Table.toC = function(self, c, name)
  if name then c:beginStruct() end
  do
    self.member.list:each(function(member)
      member.type:toC(c, member.name)
    end)
  end
  if name then c:endStruct(name) end
end

---@param c CGen
local writeTableDeserializer = function(self, c, name, vname, out)

  local nuobj = name.."Obj"

  c:append("Object* "..nuobj.." = &"..vname.."->object;")
  local nuout = "(&"..out.."->"..name..")"

  self.member.list:each(function(member)
    if member.type.writeDeserializer then
      member.type:writeDeserializer(c, member.name, nuobj, nuout)
    else
      print("missing deserializer for member '"..member.name.."' of table "..
            "'"..name.."'!")
    end
  end)
end

---@param c CGen
Table.writeDeserializer = function(self, c, name, obj, out)
  local vname = name.."Value"
  c:beginIf(
    "Value* "..vname.." = "..obj.."->findMember(\""..name.."\"_str)")
  do
    c:beginIf(vname.."->kind != Value::Kind::Object")
    do
      c:append(
        "ERROR(\"unexpected type for value '"..name.."' expected a table "..
        "but got \", getValueKindString("..vname.."->kind), \"\\n\");")
      c:append("return false;")
    end
    c:endIf()
    writeTableDeserializer(self, c, name, vname, out)
  end
  c:endIf()
end

---@param c CGen
Table.toUnionCase = function(self, c, name, obj, out)
  writeTableDeserializer(self, c, name, obj, out)
end

Table.__index = function(self, key)
  local handlers =
  {
    Table = function()
      return Table.new(self, self.json)
    end,
    Union = function()
      return Union.new(self, self.json)
    end,
  }

  local handler = handlers[key]
  if handler then
    return handler()
  end

  local table_type = TableType[key]
  if table_type then
    return table_type.new(self)
  end

  local user_schema = rawget(self, "json").user_schema.map[key]
  if user_schema then
    return user_schema.new(self)
  end

  local member = rawget(Table, key)
  if member then
    return member
  end
end

local jsonIndex = function(self, key)
  local handlers =
  {
    Schema = function()
      return Schema.new(self)
    end,
    StringedEnum = function()
      return StringedEnum.new(self)
    end,
    NumericEnum = function()
      return NumericEnum.new(self)
    end
  }

  local handler = handlers[key]
  if not handler then
    return rawget(self, key)
  end

  return handler()
end

setmetatable(json, {__index = jsonIndex})

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
  .Table "changeAnnotationSupport"
    .Bool "groupsOnLabel"
    :done()
  .ResourceOperationKind.Set "resourceOperations"
  .FailureHandlingKind "failureHandling"
  :done()

CCdynReg "DidChangeConfiguration"

CCdynReg "DidChangeWatchedFiles"
  .Bool "relativePathSupport"

CCdynReg "WorkspaceSymbol"
  .Table "symbolKind"
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
  .Table "completionItem"
    .Bool "snippetSupport"
    .Bool "commitCharactersSupport"
    .Bool "deprecatedSupport"
    .Bool "preselectSupport"
    .Bool "insertReplaceSupport"
    .Bool "labelDetailsSupport"
    .MarkupKind.Set "documentationFormat"
    .Table "resolveSupport"
      .StringArray "properties"
      :done()
    .Table "insertTextModeSupport"
      .InsertTextMode.Set "valueSet"
      :done()
    :done()
  .Table "completionItemKind"
    .CompletionItemKind.Set "valueSet"
    :done()
  .Table "completionList"
    .StringArray "itemDefaults"
    :done()

CCdynReg "Hover"
  .MarkupKind.Set "contentFormat"

CCdynReg "SignatureHelp"
  .Table "signatureInformation"
    .MarkupKind.Set "documentationFormat"
    .Table "parameterInformation"
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
  .Table "symbolKind"
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
  .Table "codeActionLiteralSupport"
    .Table "codeActionKind"
      .CodeActionKind.Set "valueSet"
      :done()
    :done()
  .Table "resolveSupport"
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
  .Table "tagSupport"
    .DiagnosticTag.Set "valueSet"
    :done()

json.StringedEnum "FoldingRangeKind"
  { Comment = "comment" }
  { Imports = "imports" }
  { Region = "region" }

CCdynReg "FoldingRange"
  .UInt "rangeLimit"
  .Bool "lineFoldingOnly"
  .Table "foldingRangeKind"
    .FoldingRangeKind.Set "valueSet"
    :done()
  .Table "foldingRange"
    .Bool "collapsedText"
    :done()

CCdynReg "SelectionRange"
CCdynReg "LinkedEditingRange"
CCdynReg "CallHierarchy"

json.StringedEnum "TokenFormat"
  { Relative = "relative" }

CCdynReg "SemanticTokens"
  .Table "requests"
    .Union "range"
      .Bool
      .Table
        :done()
      :done()
    .Union "full"
      .Bool
      .Table
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
  .Table "messageActionItem"
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
  .Table "workspace"
    .Bool "applyEdit"
    .Bool "workspaceFolders"
    .Bool "configuration"
    .Table "fileOperations"
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
  .Table "window"
    .Bool "workDoneProgress"
    .ShowMessageRequestClientCapabilities "showMessage"
    .ShowDocumentClientCapabilities "showDocument"
    :done()
  .Table "general"
    .Table "staleRequestSupport"
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
  .Table "clientInfo"
    .String "name"
    .String "version"
    :done()
  .String "locale"
  .String "rootUri"
  .String "rootPath"
  .ClientCapabilities "capabilities"
  :done()

local c = CGen.new()
json.user_schema.list:each(function(schema)
  schema.def:toCDef(c)
  if schema.def.writeDeserializer then
    schema.def:writeDeserializer(c)
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
