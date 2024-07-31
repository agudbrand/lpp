-- OK so I managed to completely forget how unordered lua tables are 
-- and so the structures these schemas will generate will be completely 
-- insane! I'll have to find a nice way to write them out that preserves 
-- order later.
--
-- Though, the way the JSON schema orders stuff is not very efficient anyways,
-- and ideally later on this can generate structures that are packed more 
-- efficiently, eg. pack all booleans together and such.

-- TODO(sushi) remove
package.path = package.path..";./iro/iro/lua/?.lua"

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
    types = List{}
  }, Union)
end

---@param c CGen
Union.toC = function(self, c, name)
  c:beginUnion()
  do
    -- TODO(sushi) this is kinda scuffed, but should work for now. Maybe 
    --             later on make it so that we name each member manually 
    --             in the schema def.
    self.types:eachWithIndex(function(type, i)
      type:toC(c, "v"..(i-1))
    end)
  end
  c:endUnion(name)
end

Union.writeDeserializer = function(self, c, name, obj, out)
  
end

Union.__call = function(self, name)
  self.name = name
  if self.prev then
    local member = { name=name,type=self }
    self.prev.member.list:push(member)
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
    T.__index = T
    Union.handlers[key] = function(union)
      union.types:push(T)
      return union
    end
    rawset(self, key, T)
  end
})

TableType.Bool =
{
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
      c:beginIf(vname.."->kind == Value::Kind::True")
      do
        c:append(out.."->"..name.." = true;")
      end
      c:beginElseIf(vname.."->kind == Value::Kind::False")
      do
        c:append(out.."->"..name.." = false;")
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
  ---@param c CGen
  toC = function(_, c, name)
    c:appendStructMember("Array<str>", name, "nil")
  end
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
            "!deserialize"..o.name.."(&"..vname.."->object,&"..out.."->"..
            name..")")
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
    c:beginFunction(
      "b8",
      "deserialize"..self.name,
      "json::Object* root", self.name.."* out")
    do
      c:append("using namespace json;")

      self.tbl.member.list:each(function(member)
        if member.type.writeDeserializer then
          member.type:writeDeserializer(c, member.name, "root", "out")
        end
      end)
    end
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

local EnumSet = {}
EnumSet.__index = EnumSet

---@param c CGen
EnumSet.toC = function(self, c, name)
  c:appendStructMember(self.of.name.."Flags", name, "{}")
end

local enumNew = function(T, writeDeserializer)
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
  o.new = enumNew(o, function(_, c, name, obj, out)
    local vname = name.."Value"
    c:beginIf(
      "Value* "..vname.." = "..obj.."->findValue(\""..name.."\"_str)")
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
        out.."->"..name.." = ".."get"..o.name.."FromString("..vname..
        "->string);")
    end
    c:endIf()
  end)
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

    c:beginFunction(o.name, "get"..o.name.."FromString", "str x")
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
    end
    c:endFunction()

    c:beginFunction("str", "getStringFrom"..o.name, o.name.." x")
    do
      c:beginSwitch("x")
      do
        o.elems:each(function(elem)
          for k,v in pairs(elem) do
            c:beginCase(o.name.."::"..k)
              c:append("return \""..v.."\";")
            c:endCase()
          end
        end)
      end
      c:endSwitch()
    end
    c:endFunction()
  end
  return setmetatable(o, StringedEnum)
end
StringedEnum.__call = enumCall

NumericEnum = {}
NumericEnum.new = function(json)
  local o = {}
  o.json = json
  o.elems = List{}
  o.new = enumNew(o)
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
  union.types:push(t)
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

    local nuobj = name.."Obj"

    c:append("Object* "..nuobj.." = &"..vname.."->object;")
    local nuout = "(&"..out.."->"..name..")"

    self.member.list:each(function(member)
      if member.type.writeDeserializer then
        member.type:writeDeserializer(c, member.name, nuobj, nuout)
      end
    end)
  end
  c:endIf()
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
  .ResourceOperationKind "resourceOperations"
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

json.Schema "InitializeParams"
  .Int "processId"
  .Table "clientInfo"
    .String "name"
    .String "version"
    :done()
  .String "locale"
  .String "rootUri"
  .ClientCapabilities "capabilities"
  :done()

local c = CGen.new()
json.user_schema.list:each(function(schema)
  schema.def:toCDef(c)
  if schema.def.writeDeserializer then
    schema.def:writeDeserializer(c)
  end
end)
print(c.buffer)

do return end


local makeType = function()
  local T = {}
  T.isTypeOf = function(x)
    return getmetatable(x) == T
  end
  return T
end

local Int = makeType()
local UInt = makeType()
local Bool = makeType()
local String = makeType()

local Union = makeType()
local union = function(...)
  return setmetatable({...}, Union)
end

local Array = makeType()
local array = function(T)
  return setmetatable({T=T}, Array)
end
local StrArray = makeType()

local Enum = makeType()
Enum.Set = makeType()
local makeEnumType = function(k, v)
  local o = setmetatable(
  {
    kind = k,
    val = v,
  }, Enum)
  o.Set = setmetatable({enum = o}, Enum.Set)
  return o
end

local stringedEnum = function(v)
  return makeEnumType("stringed", v)
end

local numericEnum = function(v)
  return makeEnumType("numeric", v)
end

local JSONSchema = makeType()
local jsonSchema = function(v)
  return setmetatable(v, JSONSchema)
end

local lsp = {}
lsp.types = List{}
lsp.typenames = {}
setmetatable(lsp,
{
  __newindex = function(self, key, val)
    self.types:push{name = key, def = val}
    self.typenames[val] = key
    rawset(self, key, val)
  end
})

lsp.ResourceOperationKind = stringedEnum
{
  Create = "create",
  Rename = "rename",
  Delete = "delete",
}

lsp.FailureHandlingKind = stringedEnum
{
  Abort = "abort",
  Transactional = "transactional",
  TextOnlyTransactional = "textOnlyTransactional",
  Undo = "undo",
}

lsp.SymbolKind = numericEnum
{
  "File",
  "Module",
  "Namespace",
  "Package",
  "Class",
  "Method",
  "Property",
  "Field",
  "Constructor",
  "Enum",
  "Interface",
  "Function",
  "Variable",
  "Constant",
  "String",
  "Number",
  "Boolean",
  "Array",
  "Object",
  "Key",
  "Null",
  "EnumMember",
  "Struct",
  "Event",
  "Operator",
  "TypeParameter",
}

lsp.SymbolTag = numericEnum
{
  "Deprecated",
}

lsp.MarkupKind = stringedEnum
{
  PlainText = "plaintext",
  Markdown = "markdown",
}

lsp.CompletionItemTab = numericEnum
{
  "Deprecated",
}

lsp.InsertTextMode = numericEnum
{
  "asIs",
  "adjustIndentation",
}

lsp.CompletionItemKind = numericEnum
{
  "Text",
  "Method",
  "Function",
  "Constructor",
  "Field",
  "Variable",
  "Class",
  "Interface",
  "Module",
  "Property",
  "Unit",
  "Value",
  "Enum",
  "Keyword",
  "Snippet",
  "Color",
  "File",
  "Reference",
  "Folder",
  "EnumMember",
  "Constant",
  "Struct",
  "Event",
  "Operator",
  "TypeParameter"
}

lsp.PrepareSupportDefaultBehavior = numericEnum
{
  "Identifier"
}

lsp.DiagnosticTag = numericEnum
{
  "Unecessary",
  "Deprecated",
}

lsp.CodeActionKind = stringedEnum
{
  Empty = "",
  QuickFix = "quickfix",
  Refactor = "refactor",
  RefactorExtract = "refactor.extract",
  RefactorInline = "refactor.inline",
  RefactorRewrite = "refactor.rewrite",
  Source = "source",
  SourceOrganizeImports = "source.organizeImports",
  SourceFixAll = "source.fixAll",
}

lsp.FoldingRangeKind = stringedEnum
{
  Comment = "comment",
  Imports = "imports",
  Region = "region",
}

lsp.TokenFormat = stringedEnum
{
  Relative = "relative",
}

lsp.PositionEncodingKind = stringedEnum
{
  UTF8 = "utf-8",
  UTF16 = "utf-16",
  UTF32 = "utf-32",
}

lsp.WorkspaceEditClientCapabilities = jsonSchema
{
  documentChanges = Bool,
  normalizesLineEndings = Bool,
  resourceOperations = lsp.ResourceOperationKind.Set,
  failureHandling = lsp.FailureHandlingKind.Set,
  changeAnnotationSupport =
  {
    groupsOnLabel = Bool,
  }
}

lsp.DidChangeConfigurationClientCapabilities = jsonSchema
{
  dynamicRegistration = Bool,
}

lsp.DidChangeWatchedFileClientCapabilities = jsonSchema
{
  dynamicRegistration = Bool,
  relativePathSupport = Bool,
}

lsp.WorkspaceSymbolClientCapabilities = jsonSchema
{
  dynamicRegistration = Bool,
  symbolKind =
  {
    valueSet = lsp.SymbolKind.Set
  },
  tagSupport =
  {
    valueSet = lsp.SymbolTag.Set
  },
  resolveSupport =
  {
    properties = StrArray,
  }
}

lsp.ExecuteCommandClientCapabilities = jsonSchema
{
  dynamicRegistration = Bool,
}

lsp.SemanticTokensWorkspaceClientCapabilities = jsonSchema
{
  refreshSupport = Bool,
}

lsp.CodeLensWorkspaceClientCapabilities = jsonSchema
{
  refreshSupport = Bool,
}

lsp.InlineValueWorkspaceClientCapabilities = jsonSchema
{
  refreshSupport = Bool,
}

lsp.InlayHintWorkspaceClientCapabilities = jsonSchema
{
  refreshSupport = Bool,
}

lsp.DiagnosticWorkspaceClientCapabilities = jsonSchema
{
  refreshSupport = Bool
}

local cc = setmetatable({},
{
  __newindex = function(self, key, val)
    lsp[key.."ClientCapabilities"] = val
    rawset(self, key, val)
  end
})

cc.TextDocumentSync = jsonSchema
{
  dynamicRegistration = Bool,
  willSave = Bool,
  willSaveWaitUntil = Bool,
  didSave = Bool,
}

cc.Completion = jsonSchema
{
  dynamicRegistration = Bool,
  completionItem =
  {
    snippetSupport = Bool,
    commitCharactersSupport = Bool,
    documentationFormat = lsp.MarkupKind.Set,
    deprecatedSupport = Bool,
    preselectSupport = Bool,
    tagSupport =
    {
      valueSet = lsp.CompletionItemTab.Set
    },
    insertReplaceSupport = Bool,
    resolveSupport =
    {
      properties = StrArray,
    },
    insertTextModeSupport =
    {
      valueSet = lsp.InsertTextMode.Set,
    },
    labelDetailsSupport = Bool,
  },
  completionItemKind =
  {
    valueSet = lsp.CompletionItemKind.Set
  },
  contextSupport = Bool,
  insertTextMode = lsp.InsertTextMode,
  completionList =
  {
    itemDefaults = StrArray,
  }
}

cc.Hover = jsonSchema
{
  dynamicRegistration = Bool,
  contentFormat = lsp.MarkupKind,
}

cc.SignatureHelp = jsonSchema
{
  dynamicRegistration = Bool,
  signatureInformation =
  {
    documentationFormat = lsp.MarkupKind,
    parameterInformation =
    {
      labelOffsetSupport = Bool,
    },
    activeParameterSupport = Bool,
  },
  contextSupport = Bool,
}

cc.Declaration = jsonSchema
{
  dynamicRegistration = Bool,
  linkSupport = Bool,
}

cc.Definition = jsonSchema
{
  dynamicRegistration = Bool,
  linkSupport = Bool,
}

cc.TypeDefinition = jsonSchema
{
  dynamicRegistration = Bool,
  linkSupport = Bool,
}

cc.Implementation = jsonSchema
{
  dynamicRegistration = Bool,
  linkSupport = Bool,
}

cc.Reference = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentHighlight = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentSymbol = jsonSchema
{
  dynamicRegistration = Bool,
  symbolKind =
  {
    valueSet = lsp.SymbolKind.Set,
  },
  hierarchicalDocumentSymbolSupport = Bool,
  tagSupport =
  {
    valueSet = lsp.SymbolTag.Set
  },
  labelSupport = Bool,
}

cc.CodeAction = jsonSchema
{
  dynamicRegistration = Bool,
  codeActionLiteralSupport =
  {
    codeActionKind =
    {
      valueSet = lsp.CodeActionKind.Set,
    }
  },
  isPreferredSupport = Bool,
  disabledSupport = Bool,
  dataSupport = Bool,
  resolveSupport =
  {
    properties = StrArray,
  },
  honorsChangeAnnotations = Bool,
}

cc.CodeLens = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentLink = jsonSchema
{
  dynamicRegistration = Bool,
  tooltipSupport = Bool,
}

cc.DocumentColor = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentFormatting = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentRangeFormatting = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.DocumentOnTypeFormatting = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.Rename = jsonSchema
{
  dynamicRegistration = Bool,
  prepareSupport = Bool,
  prepareSupportDefaultBehavior = lsp.PrepareSupportDefaultBehavior,
  honorsChangeAnnotations = Bool,
}

cc.PublishDiagnostics = jsonSchema
{
  relatedInformation = Bool,
  tagSupport =
  {
    valueSet = lsp.DiagnosticTag.Set
  },
  versionSupport = Bool,
  codeDescriptionSupport = Bool,
  dataSupport = Bool,
}

cc.FoldingRange = jsonSchema
{
  dynamicRegistration = Bool,
  rangeLimit = UInt,
  lineFoldingOnly = Bool,
  foldingRangeKind =
  {
    valueSet = lsp.FoldingRangeKind.Set
  },
  foldingRange =
  {
    collapsedText = Bool,
  }
}

cc.SelectionRange = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.LinkedEditingRange = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.CallHierarchy = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.SemanticTokens = jsonSchema
{
  dynamicRegistration = Bool,
  requests =
  {
    range = Bool,
    full = union(Bool,
    {
      delta = Bool,
    })
  },
  tokenTypes = StrArray,
  tokenModifiers = StrArray,
  formats = array(lsp.TokenFormat),
  overlappingTokenSupport = Bool,
  multilineTokenSupport = Bool,
  serverCancelSupport = Bool,
  augmentsSyntaxToken = Bool,
}

cc.Moniker = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.TypeHierarchy = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.InlineValue = jsonSchema
{
  dynamicRegistration = Bool,
}

cc.InlayHint = jsonSchema
{
  dynamicRegistration = Bool,
  resolveSupport =
  {
    properties = StrArray,
  }
}

cc.Diagnostic = jsonSchema
{
  dynamicRegistration = Bool,
  relatedDocumentSupport = Bool,
}

lsp.TextDocumentClientCapabilities = jsonSchema
{
  synchronization = cc.TextDocumentSync,
  completion = cc.Completion,
  hover = cc.Hover,
  signatureHelp = cc.SignatureHelp,
  declaration = cc.Declaration,
  definition = cc.Definition,
  typeDefinition = cc.TypeDefinition,
  implementation = cc.Implementation,
  references = cc.Reference,
  documentHighlight = cc.DocumentHighlight,
  documentSymbol = cc.DocumentSymbol,
  codeAction = cc.CodeAction,
  codeLens = cc.CodeLens,
  documentLink = cc.DocumentLink,
  colorProvider = cc.DocumentColor,
  formatting = cc.DocumentFormatting,
  rangeFormatting = cc.DocumentRangeFormatting,
  onTypeFormatting = cc.DocumentOnTypeFormatting,
  rename = cc.Rename,
  publishDiagnostics = cc.PublishDiagnostics,
  foldingRange = cc.FoldingRange,
  selectionRange = cc.SelectionRange,
  linkedEditingRange = cc.LinkedEditingRange,
  callHierarchy = cc.CallHierarchy,
  semanticTokens = cc.SemanticTokens,
  moniker = cc.Moniker,
  typeHierarchy = cc.TypeHierarchy,
  inlineValue = cc.InlineValue,
  inlayHint = cc.InlayHint,
  diagnostic = cc.Diagnostic,
}

cc.ShowMessageRequest = jsonSchema
{
  messageActionItem =
  {
    additionalPropertySupport = Bool,
  }
}

cc.ShowDocument = jsonSchema
{
  support = Bool,
}

cc.RegularExpressions = jsonSchema
{
  engine = String,
  version = String,
}

cc.Markdown = jsonSchema
{
  parser = String,
  version = String,
  allowedTags = StrArray,
}

lsp.ClientCapabilities = jsonSchema
{
  workspace =
  {
    applyEdit = Bool,
    workspaceEdit = lsp.WorkspaceEditClientCapabilities,
    didChangeConfiguration = lsp.DidChangeConfigurationClientCapabilities,
    didChangeWatchedFiles = lsp.DidChangeWatchedFileClientCapabilities,
    symbol = lsp.WorkspaceSymbolClientCapabilities,
    executeCommand = lsp.ExecuteCommandClientCapabilities,
    workspaceFolders = Bool,
    configuration = Bool,
    semanticTokens = lsp.SemanticTokensWorkspaceClientCapabilities,
    codeLens = lsp.CodeLensWorkspaceClientCapabilities,
    fileOperations =
    {
      dynamicRegistration = Bool,
      didCreate = Bool,
      willCreate = Bool,
      didRename = Bool,
      willRename = Bool,
      didDelete = Bool,
      willDelete = Bool,
    },
    inlineValue = lsp.InlineValueWorkspaceClientCapabilities,
    inlayHint = lsp.InlayHintWorkspaceClientCapabilities,
    diagnostics = lsp.DiagnosticWorkspaceClientCapabilities,
  },
  textDocument = cc.TextDocumentClientCapabilities,
  window =
  {
    workDoneProgress = Bool,
    showMessage = cc.ShowMessageRequest,
    showDocument = cc.ShowDocument,
  },
  general =
  {
    stateRequestSupport =
    {
      cancel = Bool,
      retryOnContentModified = StrArray,
    },
    regularExpressions = cc.RegularExpressions,
    markdown = cc.Markdown,
  },
  positionEncodings = lsp.PositionEncodingKind.Set,
}

lsp.InitializeParams = jsonSchema
{
  processId = union(Int, nil),

  clientInfo =
  {
    name = String,
    version = String,
  },

  locale = String,
  rootUri = String,
  capabilities = lsp.ClientCapabilities,
}

local buffer = require "string.buffer"

local code = buffer.new()

local makeEnum = function(type)
  local code = buffer.new()

  local forEachElement = function(f)
    if type.def.kind == "numeric" then
      List(type.def.val):eachWithIndex(function(val,i)
        f(val, i)
      end)
    elseif type.def.kind == "stringed" then
      for k,v in pairs(type.def.val) do
        f(k,v)
      end
    else
      error("unhandled enum type: "..type.def.kind)
    end
  end

  code:put("enum class "..type.name.."\n{\n")
  forEachElement(function(e)
    code:put("  ", e, ",\n")
  end)
  code:put("};\n\n")

  code:put("str getJSONEnumName(", type.name, " x)\n{\n",
           "  switch(x)\n  {\n")
  forEachElement(function(e)
    code:put("    case ", type.name, "::", e, ": return \"", e, "\"_str;\n")
  end)
  code:put("  }\n",
           "  return \"** invalid ", type.name, " enum value **\"_str;\n",
           "}\n\n")

  if type.def.kind == "stringed" then
    code:put("str getJSONStringedEnumString(",type.name," x)\n{\n",
             "  switch(x)\n  {\n")
    forEachElement(function(e,s)
      code:put("    case ",type.name,"::",e,": return \"",s,"\"_str;\n")
    end)
    code:put("  }\n",
             "  return \"** invalid ",type.name," enum value **\"_str;\n",
             "}\n\n")

    code:put(type.name, " getJSONStringedEnumValue(str x)\n{\n",
             "  switch(x.hash())\n{\n")
    forEachElement(function(e, s)
      code:put("    case \"",s,"\"_hashed: return ",type.name,"::",e,";\n")
    end)
    code:put("  }\n",
             "  return {};\n",
             "}\n\n")
  end

  print(tostring(code))
  return code
end

local makeJSONSchema = function(type)
  local code = buffer.new()

  code:put("struct ",type.name,"\n{\n")
  local typeHandlers =
  {
    [Int] = function(name)
      code:put("  s32 ",name," = 0;\n")
    end,

    [UInt] = function(name)
      code:put("  u32 ",name," = 0;\n")
    end,

    [String] = function(name)
      code:put("  str ",name," = nil;\n")
    end,

    [Bool] = function(name)
      code:put("  b8 ",name," = false;\n")
    end,

    [StrArray] = function(name)
      code:put("  Array<str> ",name," = nil;\n")
    end,

    [Array] = function(name, type)
      code:put()
    end
  }

  local nestedTable = function(name, tbl)

  end

  for k,v in pairs(type.def) do
    print(k, v.name)
    local handler = typeHandlers[v]
    if handler then
      handler(k, type)
    else
    end
  end
  print(tostring(code))
end

lsp.types:each(function(type)
  if Enum.isTypeOf(type.def) then
    makeEnum(type)
  elseif JSONSchema.isTypeOf(type.def) then
    makeJSONSchema(type)
  else
    error(type.name.." has unrecognized kind of type")
  end
end)
