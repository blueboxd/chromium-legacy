// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is implementation of a clang tool that rewrites raw pointer fields into
// CheckedPtr<T>:
//     Pointee* field_
// becomes:
//     CheckedPtr<Pointee> field_
//
// For more details, see the doc here:
// https://docs.google.com/document/d/1chTvr3fSofQNV_PDPEHRyUgcJCQBgTDOOBriW9gIm9M

#include <assert.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/MacroArgs.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;

namespace {

// Include path that needs to be added to all the files where CheckedPtr<...>
// replaces a raw pointer.
const char kIncludePath[] = "base/memory/checked_ptr.h";

// Name of a cmdline parameter that can be used to specify a file listing fields
// that should not be rewritten to use CheckedPtr<T>.
const char kExcludeFieldsParamName[] = "exclude-fields";

// Output format is documented in //docs/clang_tool_refactoring.md
class ReplacementsPrinter : public clang::tooling::SourceFileCallbacks {
 public:
  ReplacementsPrinter() = default;
  ~ReplacementsPrinter() = default;

  void PrintReplacement(const clang::SourceManager& source_manager,
                        const clang::SourceRange& replacement_range,
                        std::string replacement_text,
                        bool should_add_include = false) {
    if (ShouldSuppressOutput())
      return;

    clang::tooling::Replacement replacement(
        source_manager, clang::CharSourceRange::getCharRange(replacement_range),
        replacement_text);
    llvm::StringRef file_path = replacement.getFilePath();
    assert(!file_path.empty());

    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << file_path << ":::" << replacement.getOffset()
                 << ":::" << replacement.getLength()
                 << ":::" << replacement_text << "\n";

    if (should_add_include) {
      bool was_inserted = false;
      std::tie(std::ignore, was_inserted) =
          files_with_already_added_includes_.insert(file_path.str());
      if (was_inserted)
        llvm::outs() << "include-user-header:::" << file_path
                     << ":::-1:::-1:::" << kIncludePath << "\n";
    }
  }

 private:
  // clang::tooling::SourceFileCallbacks override:
  bool handleBeginSource(clang::CompilerInstance& compiler) override {
    const clang::FrontendOptions& frontend_options = compiler.getFrontendOpts();

    assert((frontend_options.Inputs.size() == 1) &&
           "run_tool.py should invoke the rewriter one file at a time");
    const clang::FrontendInputFile& input_file = frontend_options.Inputs[0];
    assert(input_file.isFile() &&
           "run_tool.py should invoke the rewriter on actual files");

    current_language_ = input_file.getKind().getLanguage();

    if (!ShouldSuppressOutput())
      llvm::outs() << "==== BEGIN EDITS ====\n";

    return true;  // Report that |handleBeginSource| succeeded.
  }

  // clang::tooling::SourceFileCallbacks override:
  void handleEndSource() override {
    if (!ShouldSuppressOutput())
      llvm::outs() << "==== END EDITS ====\n";
  }

  bool ShouldSuppressOutput() {
    switch (current_language_) {
      case clang::Language::Unknown:
      case clang::Language::Asm:
      case clang::Language::LLVM_IR:
      case clang::Language::OpenCL:
      case clang::Language::CUDA:
      case clang::Language::RenderScript:
      case clang::Language::HIP:
        // Rewriter can't handle rewriting the current input language.
        return true;

      case clang::Language::C:
      case clang::Language::ObjC:
        // CheckedPtr requires C++.  In particular, attempting to #include
        // "base/memory/checked_ptr.h" from C-only compilation units will lead
        // to compilation errors.
        return true;

      case clang::Language::CXX:
      case clang::Language::ObjCXX:
        return false;
    }

    assert(false && "Unrecognized clang::Language");
    return true;
  }

  std::set<std::string> files_with_already_added_includes_;
  clang::Language current_language_ = clang::Language::Unknown;
};

AST_MATCHER(clang::TagDecl, isNotFreeStandingTagDecl) {
  const clang::TagDecl* tag_decl = Node.getCanonicalDecl();
  return !tag_decl->isFreeStanding();
}

llvm::StringRef GetFilePath(const clang::SourceManager& source_manager,
                            const clang::FieldDecl& field_decl) {
  clang::SourceLocation loc = field_decl.getSourceRange().getBegin();
  if (loc.isInvalid() || !loc.isFileID())
    return llvm::StringRef();

  clang::FileID file_id = source_manager.getDecomposedLoc(loc).first;
  const clang::FileEntry* file_entry =
      source_manager.getFileEntryForID(file_id);
  if (!file_entry)
    return llvm::StringRef();

  return file_entry->getName();
}

AST_MATCHER(clang::FieldDecl, isInThirdPartyLocation) {
  llvm::StringRef file_path =
      GetFilePath(Finder->getASTContext().getSourceManager(), Node);

  // Blink is part of the Chromium git repo, even though it contains
  // "third_party" in its path.
  if (file_path.contains("third_party/blink/"))
    return false;

  // V8 needs to be considered "third party", even though its paths do not
  // contain the "third_party" substring.  In particular, the rewriter should
  // not append |.get()| to references to |v8::RegisterState::pc|, because
  // //v8/include/v8.h will *not* get rewritten.
  if (file_path.contains("v8/include/"))
    return true;

  // Otherwise, just check if the paths contains the "third_party" substring.
  return file_path.contains("third_party");
}

class FieldDeclFilterFile {
 public:
  explicit FieldDeclFilterFile(const std::string& filepath) {
    if (!filepath.empty())
      ParseInputFile(filepath);
  }

  bool Contains(const clang::FieldDecl& field_decl) const {
    std::string qualified_name = field_decl.getQualifiedNameAsString();
    auto it = fields_to_filter_.find(qualified_name);
    return it != fields_to_filter_.end();
  }

 private:
  // Expected file format:
  // - '#' character starts a comment (which gets ignored).
  // - Blank or whitespace-only or comment-only lines are ignored.
  // - Other lines are expected to contain a fully-qualified name of a field
  //   like:
  //       autofill::AddressField::address1_ # some comment
  // - Templates are represented without template arguments, like:
  //       WTF::HashTable::table_ # some comment
  void ParseInputFile(const std::string& filepath) {
    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> file_or_err =
        llvm::MemoryBuffer::getFile(filepath);
    if (std::error_code err = file_or_err.getError()) {
      llvm::errs() << "ERROR: Cannot open the file specified in --"
                   << kExcludeFieldsParamName << " argument: " << filepath
                   << ": " << err.message() << "\n";
      assert(false);
      return;
    }

    llvm::line_iterator it(**file_or_err, true /* SkipBlanks */, '#');
    for (; !it.is_at_eof(); ++it) {
      llvm::StringRef line = *it;

      // Remove trailing comments.
      size_t comment_start_pos = line.find('#');
      if (comment_start_pos != llvm::StringRef::npos)
        line = line.substr(0, comment_start_pos);
      line = line.trim();

      if (line.empty())
        continue;

      fields_to_filter_.insert(line);
    }
  }

  // Stores fully-namespace-qualified names of fields matched by the filter.
  llvm::StringSet<> fields_to_filter_;
};

AST_MATCHER_P(clang::FieldDecl,
              isListedInFilterFile,
              FieldDeclFilterFile,
              Filter) {
  return Filter.Contains(Node);
}

AST_MATCHER(clang::Decl, isInExternCContext) {
  return Node.getLexicalDeclContext()->isExternCContext();
}

AST_MATCHER(clang::ClassTemplateSpecializationDecl, isImplicitSpecialization) {
  return !Node.isExplicitSpecialization();
}

AST_MATCHER(clang::Type, anyCharType) {
  return Node.isAnyCharacterType();
}

AST_POLYMORPHIC_MATCHER(isInMacroLocation,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(clang::Decl,
                                                        clang::Stmt,
                                                        clang::TypeLoc)) {
  return Node.getBeginLoc().isMacroID();
}

// Matcher for FieldDecl that has a TypeLoc with a unique start location
// (i.e. has a TypeLoc that is not shared with any other FieldDecl).
//
// Given
//   struct MyStrict {
//     int f;
//     int f2, f3;
//   };
// matches |int f|, but does not match declarations of |f2| and |f3|.
AST_MATCHER(clang::FieldDecl, hasUniqueTypeLoc) {
  const clang::FieldDecl& self = Node;
  const clang::RecordDecl* record_decl = self.getParent();
  clang::SourceLocation self_type_loc =
      self.getTypeSourceInfo()->getTypeLoc().getBeginLoc();

  bool has_sibling_with_same_type_loc =
      std::any_of(record_decl->field_begin(), record_decl->field_end(),
                  [&](const clang::FieldDecl* f) {
                    // Is |f| a real sibling?
                    if (f == &self)
                      return false;  // Not a sibling.

                    clang::SourceLocation sibling_type_loc =
                        f->getTypeSourceInfo()->getTypeLoc().getBeginLoc();
                    return self_type_loc == sibling_type_loc;
                  });

  return !has_sibling_with_same_type_loc;
}

// Rewrites |SomeClass* field| (matched as "fieldDecl") into
// |CheckedPtr<SomeClass> field| and for each file rewritten in such way adds an
// |#include "base/memory/checked_ptr.h"|.
class FieldDeclRewriter : public MatchFinder::MatchCallback {
 public:
  explicit FieldDeclRewriter(ReplacementsPrinter* replacements_printer)
      : replacements_printer_(replacements_printer) {}

  void run(const MatchFinder::MatchResult& result) override {
    const clang::ASTContext& ast_context = *result.Context;
    const clang::SourceManager& source_manager = *result.SourceManager;

    const clang::FieldDecl* field_decl =
        result.Nodes.getNodeAs<clang::FieldDecl>("fieldDecl");
    assert(field_decl && "matcher should bind 'fieldDecl'");

    const clang::TypeSourceInfo* type_source_info =
        field_decl->getTypeSourceInfo();
    assert(type_source_info && "assuming |type_source_info| is always present");

    clang::QualType pointer_type = type_source_info->getType();
    assert(type_source_info->getType()->isPointerType() &&
           "matcher should only match pointer types");

    // Calculate the |replacement_range|.
    //
    // Consider the following example:
    //      const Pointee* const field_name_;
    //      ^--------------------^  = |replacement_range|
    //                           ^  = |field_decl->getLocation()|
    //      ^                       = |field_decl->getBeginLoc()|
    //                   ^          = PointerTypeLoc::getStarLoc
    //            ^------^          = TypeLoc::getSourceRange
    //
    // We get the |replacement_range| in a bit clumsy way, because clang docs
    // for QualifiedTypeLoc explicitly say that these objects "intentionally do
    // not provide source location for type qualifiers".
    clang::SourceRange replacement_range(field_decl->getBeginLoc(),
                                         field_decl->getLocation());

    // Calculate |replacement_text|.
    std::string replacement_text = GenerateNewText(ast_context, pointer_type);
    if (field_decl->isMutable())
      replacement_text.insert(0, "mutable ");

    // Generate and print a replacement.
    replacements_printer_->PrintReplacement(source_manager, replacement_range,
                                            replacement_text,
                                            true /* should_add_include */);
  }

 private:
  std::string GenerateNewText(const clang::ASTContext& ast_context,
                              const clang::QualType& pointer_type) {
    std::string result;

    assert(pointer_type->isPointerType() && "caller must pass a pointer type!");
    clang::QualType pointee_type = pointer_type->getPointeeType();

    // Preserve qualifiers.
    assert(!pointer_type.isRestrictQualified() &&
           "|restrict| is a C-only qualifier and CheckedPtr<T> needs C++");
    if (pointer_type.isConstQualified())
      result += "const ";
    if (pointer_type.isVolatileQualified())
      result += "volatile ";

    // Convert pointee type to string.
    clang::PrintingPolicy printing_policy(ast_context.getLangOpts());
    printing_policy.SuppressScope = 1;  // s/blink::Pointee/Pointee/
    std::string pointee_type_as_string =
        pointee_type.getAsString(printing_policy);
    result += llvm::formatv("CheckedPtr<{0}> ", pointee_type_as_string);

    return result;
  }

  ReplacementsPrinter* const replacements_printer_;
};

// Rewrites |my_struct.ptr_field| (matched as "affectedMemberExpr") into
// |my_struct.ptr_field.get()|.
class AffectedExprRewriter : public MatchFinder::MatchCallback {
 public:
  AffectedExprRewriter(ReplacementsPrinter* replacements_printer)
      : replacements_printer_(replacements_printer) {}

  void run(const MatchFinder::MatchResult& result) override {
    const clang::SourceManager& source_manager = *result.SourceManager;

    const clang::MemberExpr* member_expr =
        result.Nodes.getNodeAs<clang::MemberExpr>("affectedMemberExpr");
    assert(member_expr && "matcher should bind 'affectedMemberExpr'");

    clang::SourceLocation member_name_start = member_expr->getMemberLoc();
    size_t member_name_length = member_expr->getMemberDecl()->getName().size();
    clang::SourceLocation insertion_loc =
        member_name_start.getLocWithOffset(member_name_length);

    clang::SourceRange replacement_range(insertion_loc, insertion_loc);

    replacements_printer_->PrintReplacement(source_manager, replacement_range,
                                            ".get()");
  }

 private:
  ReplacementsPrinter* const replacements_printer_;
};

}  // namespace

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_raw_ptr_fields: changes |T* field_| to |CheckedPtr<T> field_|.");
  llvm::cl::opt<std::string> exclude_fields_param(
      kExcludeFieldsParamName, llvm::cl::value_desc("filepath"),
      llvm::cl::desc("file listing fields to be blocked (not rewritten)"));
  clang::tooling::CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  ReplacementsPrinter replacements_printer;

  // Supported pointer types =========
  // Given
  //   struct MyStrict {
  //     int* int_ptr;
  //     int i;
  //     char* char_ptr;
  //     int (*func_ptr)();
  //     int (MyStruct::* member_func_ptr)(char);
  //     int (*ptr_to_array_of_ints)[123]
  //     StructOrClassWithDeletedOperatorNew* stack_or_gc_ptr;
  //     struct { int i }* ptr_to_non_free_standing_record_or_union_or_class;
  //   };
  // matches |int*|, but not the other types.
  auto record_with_deleted_allocation_operator_type_matcher =
      recordType(hasDeclaration(cxxRecordDecl(
          hasMethod(allOf(hasOverloadedOperatorName("new"), isDeleted())))));
  auto non_free_standing_tag_type =
      tagType(hasDeclaration(tagDecl(isNotFreeStandingTagDecl())));
  auto supported_pointer_types_matcher =
      pointerType(unless(pointee(hasUnqualifiedDesugaredType(
          anyOf(record_with_deleted_allocation_operator_type_matcher,
                non_free_standing_tag_type, functionType(), memberPointerType(),
                anyCharType(), arrayType())))));

  // Implicit field declarations =========
  // Matches field declarations that do not explicitly appear in the source
  // code:
  // 1. fields of classes generated by the compiler to back capturing lambdas,
  // 2. fields within an implicit class template specialization (e.g. when a
  //    template is instantiated by a bit of code and there's no explicit
  //    specialization for it).
  auto implicit_field_decl_matcher = fieldDecl(hasParent(cxxRecordDecl(anyOf(
      isLambda(), classTemplateSpecializationDecl(isImplicitSpecialization()),
      hasAncestor(
          classTemplateSpecializationDecl(isImplicitSpecialization()))))));

  // Field declarations =========
  // Given
  //   struct S {
  //     int* y;
  //   };
  // matches |int* y|.  Doesn't match:
  // - non-pointer types
  // - fields of lambda-supporting classes
  // - fields listed in the --exclude-fields cmdline param
  // - "implicit" fields (i.e. field decls that are not explicitly present in
  //   the source code)
  FieldDeclFilterFile fields_to_exclude(exclude_fields_param);
  auto field_decl_matcher =
      fieldDecl(allOf(hasType(supported_pointer_types_matcher),
                      hasUniqueTypeLoc(),
                      unless(anyOf(isInThirdPartyLocation(),
                                   isInMacroLocation(), isInExternCContext(),
                                   isListedInFilterFile(fields_to_exclude),
                                   implicit_field_decl_matcher))))
          .bind("fieldDecl");
  FieldDeclRewriter field_decl_rewriter(&replacements_printer);
  match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

  // Matches expressions that used to return a value of type |SomeClass*|
  // but after the rewrite return an instance of |CheckedPtr<SomeClass>|.
  // Many such expressions might need additional changes after the rewrite:
  // - Some expressions (printf args, const_cast args, etc.) might need |.get()|
  //   appended.
  // - Using such expressions in specific contexts (e.g. as in-out arguments or
  //   as a return value of a function returning references) may require
  //   additional work and should cause related fields to be emitted as
  //   candidates for the --field-filter-file parameter.
  auto affected_member_expr_matcher =
      memberExpr(member(field_decl_matcher)).bind("affectedMemberExpr");
  auto affected_implicit_expr_matcher = implicitCastExpr(has(expr(anyOf(
      // Only single implicitCastExpr is present in case of:
      // |auto* v = s.ptr_field;|
      expr(affected_member_expr_matcher),
      // 2nd nested implicitCastExpr is present in case of:
      // |const auto* v = s.ptr_field;|
      expr(implicitCastExpr(has(affected_member_expr_matcher)))))));
  auto affected_expr_matcher =
      expr(anyOf(affected_member_expr_matcher, affected_implicit_expr_matcher));

  // Places where |.get()| needs to be appended =========
  // Given
  //   void foo(const S& s) {
  //     printf("%p", s.y);
  //     const_cast<...>(s.y)
  //     reinterpret_cast<...>(s.y)
  //   }
  // matches the |s.y| expr if it matches the |affected_expr_matcher| above.
  auto affected_expr_that_needs_fixing_matcher = expr(allOf(
      affected_expr_matcher,
      hasParent(expr(anyOf(callExpr(callee(functionDecl(isVariadic()))),
                           cxxConstCastExpr(), cxxReinterpretCastExpr())))));
  AffectedExprRewriter affected_expr_rewriter(&replacements_printer);
  match_finder.addMatcher(affected_expr_that_needs_fixing_matcher,
                          &affected_expr_rewriter);

  // Affected ternary operator args =========
  // Given
  //   void foo(const S& s) {
  //     cond ? s.y : ...
  //   }
  // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
  auto affected_ternary_operator_arg_matcher =
      conditionalOperator(eachOf(hasTrueExpression(affected_expr_matcher),
                                 hasFalseExpression(affected_expr_matcher)));
  match_finder.addMatcher(affected_ternary_operator_arg_matcher,
                          &affected_expr_rewriter);

  // |auto| type declarations =========
  // Given
  //   struct S { int* y; };
  //   void foo(const S& s) {
  //     auto* p = s.y;
  //   }
  // binds the |s.y| expr if it matches the |affected_expr_matcher| above.
  auto auto_var_decl_matcher = declStmt(forEach(varDecl(
      allOf(hasType(pointerType(pointee(autoType()))),
            hasInitializer(anyOf(
                affected_implicit_expr_matcher,
                initListExpr(hasInit(0, affected_implicit_expr_matcher))))))));
  match_finder.addMatcher(auto_var_decl_matcher, &affected_expr_rewriter);

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder,
                                               &replacements_printer);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  return 0;
}
