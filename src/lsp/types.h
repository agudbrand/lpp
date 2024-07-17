/*
 *  Types representing LSP data structures.
 */

#ifndef _lpp_lsp_types_h
#define _lpp_lsp_types_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/flags.h"
#include "iro/containers/array.h"

namespace lsp
{

using namespace iro;

using StrArray = Array<str>;

/* ============================================================================
 */
enum class PositionEncodingKind
{
  UTF8,
  UTF16,
  UTF32,
};

/* ============================================================================
 */
struct TextDocumentSyncClientCapabilities
{
  b8 dynamic_registration;
  b8 will_save;
  b8 will_save_wait_until;
  b8 did_save;
};

/* ============================================================================
 */
enum class MarkupKind
{
  PlainText,
  Markdown,
  Both,
};

/* ============================================================================
 */
enum class InsertTextMode
{
  AsIs,
  AdjustIndentation,
  Both,
};

/* ============================================================================
 */
enum class CompletionItemKind
{
  Text,
  Method,
  Function,
  Constructor,
  Field,
  Variable,
  Class,
  Interface,
  Module,
  Property,
  Unit,
  Value,
  Enum,
  Keyword,
  Snippet,
  Color,
  File,
  Reference,
  Folder,
  EnumMember,
  Constant,
  Struct,
  Event,
  Operator,
  TypeParameter,
};

typedef Flags<CompletionItemKind> CompletionItemKindFlags;

/* ============================================================================
 */
enum class SymbolKind
{
  File,
  Module,
  Namespace,
  Package,
  Class,
  Method,
  Property,
  Field,
  Constructor,
  Enum,
  Interface,
  Function,
  Variable,
  Constant,
  String,
  Number,
  Boolean,
  Array,
  Object,
  Key,
  Null,
  EnumMember,
  Struct,
  Event,
  Operator,
  TypeParameter,
};

typedef Flags<SymbolKind> SymbolKindFlags;

/* ============================================================================
 */
struct CompletionClientCapabilities
{
  b8 dynamic_registration;
  b8 snippet_support;
  b8 commit_characters_support;
  b8 deprecated_support;
  b8 preselect_support;
  b8 insert_replace_support;
  b8 label_details_support;
  b8 context_support;

  StrArray resolve_support_properties;
  MarkupKind documentation_format;
  InsertTextMode completion_item_insert_text_mode_support;

  InsertTextMode default_insert_text_mode;

  CompletionItemKindFlags completion_item_kind_flags;
  StrArray completion_list_item_defaults;
};

/* ============================================================================
 */
struct HoverClientCapabilities
{
  b8 dynamic_registration;
  MarkupKind content_format;
};

/* ============================================================================
 */
struct SignatureHelpClientCapabilities
{
  b8 dynamic_registration;
  MarkupKind documentation_format;
  b8 parameter_label_offset_support;
  b8 active_parameter_support;
  b8 context_support;
};

/* ============================================================================
 */
struct DeclarationClientCapabilities
{
  b8 dynamic_registration;
  b8 link_support;
};

/* ============================================================================
 */
struct DefinitionClientCapabilities
{
  b8 dynamic_registration;
  b8 link_support;
};

/* ============================================================================
 */
struct TypeDefinitionClientCapabilities
{
  b8 dynamic_registration;
  b8 link_support;
};

/* ============================================================================
 */
struct ImplementationClientCapabilities
{
  b8 dynamic_registration;
  b8 link_support;
};

/* ============================================================================
 */
struct ReferenceClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentHighlightClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentSymbolClientCapabilities
{
  b8 dynamic_registration;
  b8 hierarchical_document_symbol_support;
  b8 label_support;

  // default File -> Array
  SymbolKindFlags symbol_kind_support;
};

/* ============================================================================
 */
enum class CodeActionKind
{
  Empty,
  QuickFix,
  Refactor,
  RefactorExtract,
  RefactorInline,
  RefactorRewrite,
  Source,
  SourceOrganizeImports,
  SourceFixAll,
};

typedef Flags<CodeActionKind> CodeActionKindFlags;

/* ----------------------------------------------------------------------------
 */
static inline CodeActionKind getCodeActionKind(str s)
{
  if (s.isEmpty())
    return CodeActionKind::Empty;

  switch (s.hash())
  {
#define map(x,y) case GLUE(x,_hashed): return CodeActionKind::y;
  map("quickfix",               QuickFix);
  map("refactor",               Refactor);
  map("refactor.extract",       RefactorExtract);
  map("refactor.inline",        RefactorInline);
  map("refactor.rewrite",       RefactorRewrite);
  map("source",                 Source);
  map("source.organizeImports", SourceOrganizeImports);
  map("source.fixAll",          SourceFixAll);
#undef map
  }
  assert(!"unhandled CodeActionKind");
  return CodeActionKind::Empty;
}

/* ============================================================================
 */
struct CodeActionClientCapabilities
{
  b8 dynamic_registration;
  b8 is_preferred_support;
  b8 disabled_support;
  b8 resolve_support;
  b8 honors_change_annotations;

  CodeActionKindFlags code_action_literal_support;
};

/* ============================================================================
 */
struct CodeLensClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentLinkClientCapabilities
{
  b8 dynamic_registration;
  b8 tooltip_support;
};

/* ============================================================================
 */
struct DocumentColorClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentFormattingClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentRangeFormattingClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct DocumentOnTypeFormattingClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct RenameClientCapabilities
{
  b8 dynamic_registration;
  b8 prepare_support;
  b8 honors_change_annotations;
};

/* ============================================================================
 */
enum class DiagnosticSeverity
{
  Error,
  Warning,
  Information,
  Hint,
};

typedef Flags<DiagnosticSeverity> DiagnosticSeverityFlags;

/* ============================================================================
 */
enum class DiagnosticTag
{
  Unecessary,
  Deprecated,
};

typedef Flags<DiagnosticTag> DiagnosticTagFlags;

/* ============================================================================
 */
struct PublishDiagnosticsClientCapabilities
{
  b8 related_information;
  b8 version_support;
  b8 code_description_support;
  b8 data_support;

  DiagnosticTagFlags tag_support;
};

/* ============================================================================
 */
enum class FoldingRangeKind
{
  Comment,
  Imports,
  Region,
};

typedef Flags<FoldingRangeKind> FoldingRangeKindFlags;

/* ----------------------------------------------------------------------------
 */
static inline FoldingRangeKind getFoldingRangeKind(str s)
{
  switch (s.hash())
  {
#define map(x,y) case GLUE(x,_hashed): return FoldingRangeKind::y;
  map("comment", Comment);
  map("imports", Imports);
  map("region",  Region);
#undef map
  }
  assert(!"unhandled FoldingRangeKind");
  return {};
}

/* ============================================================================
 */
struct FoldingRangeClientCapabilities
{
  b8 dynamic_registration;
  b8 line_folding_only;
  b8 can_collapse_text;

  u32 range_limit;
  
  FoldingRangeKindFlags folding_range_kind_support;
};

/* ============================================================================
 */
struct SelectionRangeClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct LinkedEditingRangeClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct CallHierarchyClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct SemanticTokensClientCapabilities
{
  b8 dynamic_registration;
  b8 requests_range;
  b8 requests_full;
  b8 requests_full_delta;
  b8 overlapping_tokens_support;
  b8 multiline_token_support;
  b8 server_cancel_support;
  b8 augments_syntax_tokens;

  StrArray token_types;
  StrArray token_modifier;
};

/* ============================================================================
 */
struct MonikerClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct TypeHierarchyClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct InlineValueClientCapabilities
{
  b8 dynamic_registration;
};

/* ============================================================================
 */
struct InlayHintClientCapabilities
{
  b8 dynamic_registration;

  StrArray resolve_support;
};

/* ============================================================================
 */
struct DiagnosticClientCapabilities
{
  b8 dynamic_registration;
  b8 related_document_support;
};

/* ============================================================================
 */
struct TextDocumentCapabilities
{
  TextDocumentSyncClientCapabilities synchronization;
  CompletionClientCapabilities completion;
  HoverClientCapabilities hover;
  SignatureHelpClientCapabilities signature_help;
  DeclarationClientCapabilities declaration;
  DefinitionClientCapabilities definition;
  TypeDefinitionClientCapabilities type_definition;
  ImplementationClientCapabilities implementation;
  ReferenceClientCapabilities references;
  DocumentHighlightClientCapabilities document_highlight;
  DocumentSymbolClientCapabilities document_symbol;
  CodeActionClientCapabilities code_action;
  CodeLensClientCapabilities code_len;
  DocumentLinkClientCapabilities document_link;
  DocumentColorClientCapabilities color_provider;
  DocumentRangeFormattingClientCapabilities range_formatting;
  DocumentOnTypeFormattingClientCapabilities on_type_formatting;
  RenameClientCapabilities rename;
  PublishDiagnosticsClientCapabilities publish_diagnostics;
  FoldingRangeClientCapabilities folding_range;
  SelectionRangeClientCapabilities selection_range;
  LinkedEditingRangeClientCapabilities linked_editing_range;
  CallHierarchyClientCapabilities call_hierarchy;
  SemanticTokensClientCapabilities semantic_tokens;
  MonikerClientCapabilities moniker;
  TypeHierarchyClientCapabilities type_hierarchy;
  InlineValueClientCapabilities inline_value;
  InlayHintClientCapabilities inlay_hint;
  DiagnosticClientCapabilities diagnostic;
};

/* ============================================================================
 */
struct MarkdownClientCapabilities
{
  str parser;
  str version;
  StrArray allowed_tags;
};

/* ============================================================================
 */
struct ClientCapabilities
{
  // The utf encoding used for document positions.
  PositionEncodingKind position_encoding;

  // Support for the server requesting the client force a refresh of 
  // various things.
  struct 
  {
    b8 semantic_tokens;
    b8 inline_value;
    b8 inlay_hint;
    b8 diagnostics;
    b8 code_lens;
  } refresh_support;

  // Support for different file operations
  struct
  {
    b8  did_create;
    b8 will_create;
    b8  did_rename;
    b8 will_rename;
    b8  did_delete;
    b8 will_delete;
    b8 dynamic_registration;
  } fileop;

  TextDocumentCapabilities text_document_capabilities;
  MarkdownClientCapabilities markdown_capabilities;
};

} // namespace lsp

#endif // _lpp_lsp_types_h
