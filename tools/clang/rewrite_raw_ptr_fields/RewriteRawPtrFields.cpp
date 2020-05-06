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

// Output format is documented in //docs/clang_tool_refactoring.md
class ReplacementsPrinter {
 public:
  ReplacementsPrinter() { llvm::outs() << "==== BEGIN EDITS ====\n"; }

  ~ReplacementsPrinter() { llvm::outs() << "==== END EDITS ====\n"; }

  void PrintReplacement(const clang::SourceManager& source_manager,
                        const clang::SourceRange& replacement_range,
                        std::string replacement_text) {
    clang::tooling::Replacement replacement(
        source_manager, clang::CharSourceRange::getCharRange(replacement_range),
        replacement_text);
    llvm::StringRef file_path = replacement.getFilePath();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << file_path << ":::" << replacement.getOffset()
                 << ":::" << replacement.getLength()
                 << ":::" << replacement_text << "\n";

    bool was_inserted = false;
    std::tie(std::ignore, was_inserted) =
        files_with_already_added_includes_.insert(file_path.str());
    if (was_inserted)
      llvm::outs() << "include-user-header:::" << file_path
                   << ":::-1:::-1:::" << kIncludePath << "\n";
  }

 private:
  std::set<std::string> files_with_already_added_includes_;
};

class FieldDeclRewriter : public MatchFinder::MatchCallback {
 public:
  explicit FieldDeclRewriter(ReplacementsPrinter* replacements_printer)
      : replacements_printer_(replacements_printer) {}

  void run(const MatchFinder::MatchResult& result) override {
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
    //      ^-------------------^   = |replacement_range|
    //                           ^  = |field_decl->getLocation()|
    //      ^                       = |field_decl->getBeginLoc()|
    //                   ^          = PointerTypeLoc::getStarLoc
    //            ^------^          = TypeLoc::getSourceRange
    //
    // We get the |replacement_range| in a bit clumsy way, because clang docs
    // for QualifiedTypeLoc explicitly say that these objects "intentionally do
    // not provide source location for type qualifiers".
    clang::SourceRange replacement_range(
        field_decl->getBeginLoc(),
        field_decl->getLocation().getLocWithOffset(-1));

    // Calculate |replacement_text|.
    std::string replacement_text = GenerateNewText(pointer_type);
    if (field_decl->isMutable())
      replacement_text.insert(0, "mutable ");

    // Generate and print a replacement.
    replacements_printer_->PrintReplacement(source_manager, replacement_range,
                                            replacement_text);
  }

 private:
  std::string GenerateNewText(const clang::QualType& pointer_type) {
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
    clang::LangOptions lang_options;
    clang::PrintingPolicy printing_policy(lang_options);
    printing_policy.SuppressTagKeyword = 1;  // s/class Pointee/Pointee/
    std::string pointee_type_as_string =
        pointee_type.getAsString(printing_policy);
    result += llvm::formatv("CheckedPtr<{0}>", pointee_type_as_string);

    return result;
  }

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
  clang::tooling::CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  ReplacementsPrinter replacements_printer;

  // Field declarations =========
  // Given
  //   struct S {
  //     int* y;
  //   };
  // matches |int* y|.
  auto field_decl_matcher = fieldDecl(hasType(pointerType())).bind("fieldDecl");
  FieldDeclRewriter field_decl_rewriter(&replacements_printer);
  match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

  // Prepare and run the tool.
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  return 0;
}
