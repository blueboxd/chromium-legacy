// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "BadPatternFinder.h"
#include "DiagnosticsReporter.h"

#include <algorithm>
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang::ast_matchers;

namespace {

TypeMatcher GarbageCollectedType() {
  auto has_gc_base = hasCanonicalType(hasDeclaration(
      cxxRecordDecl(isDerivedFrom(hasAnyName("::blink::GarbageCollected",
                                             "::blink::GarbageCollectedMixin")))
          .bind("gctype")));
  return anyOf(has_gc_base,
               hasCanonicalType(arrayType(hasElementType(has_gc_base))));
}

class UniquePtrGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit UniquePtrGarbageCollectedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches any application of make_unique where the template argument is
    // known to refer to a garbage-collected type.
    auto make_unique_matcher =
        callExpr(
            callee(functionDecl(
                       hasAnyName("::std::make_unique", "::base::WrapUnique"),
                       hasTemplateArgument(
                           0, refersToType(GarbageCollectedType())))
                       .bind("badfunc")))
            .bind("bad");
    match_finder.addDynamicMatcher(make_unique_matcher, this);
  }

  void run(const MatchFinder::MatchResult& result) {
    auto* bad_use = result.Nodes.getNodeAs<clang::Expr>("bad");
    auto* bad_function = result.Nodes.getNodeAs<clang::FunctionDecl>("badfunc");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    diagnostics_.UniquePtrUsedWithGC(bad_use, bad_function, gc_type);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

class OptionalGarbageCollectedMatcher : public MatchFinder::MatchCallback {
 public:
  explicit OptionalGarbageCollectedMatcher(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    // Matches any application of make_unique where the template argument is
    // known to refer to a garbage-collected type.
    auto optional_construction =
        cxxConstructExpr(hasDeclaration(cxxConstructorDecl(ofClass(
                             classTemplateSpecializationDecl(
                                 hasName("::base::Optional"),
                                 hasTemplateArgument(
                                     0, refersToType(GarbageCollectedType())))
                                 .bind("optional")))))
            .bind("bad");
    match_finder.addDynamicMatcher(optional_construction, this);
  }

  void run(const MatchFinder::MatchResult& result) {
    auto* bad_use = result.Nodes.getNodeAs<clang::Expr>("bad");
    auto* optional = result.Nodes.getNodeAs<clang::CXXRecordDecl>("optional");
    auto* gc_type = result.Nodes.getNodeAs<clang::CXXRecordDecl>("gctype");
    diagnostics_.OptionalUsedWithGC(bad_use, optional, gc_type);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

class MissingMixinMarker : public MatchFinder::MatchCallback {
 public:
  explicit MissingMixinMarker(DiagnosticsReporter& diagnostics)
      : diagnostics_(diagnostics) {}

  void Register(MatchFinder& match_finder) {
    auto class_missing_mixin_marker = cxxRecordDecl(
        decl().bind("bad_class"),
        // Definition of a garbage-collected class
        isDefinition(),
        isDerivedFrom(cxxRecordDecl(decl().bind("gc_base_class"),
                                    hasName("::blink::GarbageCollected"))),
        // ...which derives some mixin...
        isDerivedFrom(cxxRecordDecl(decl().bind("mixin_base_class"),
                                    hasName("::blink::GarbageCollectedMixin"))),
        // ...and doesn't use USING_GARBAGE_COLLECTED_MIXIN
        unless(anyOf(isSameOrDerivedFrom(has(typedefNameDecl(
                         hasName("HasUsingGarbageCollectedMixinMacro")))),
                     isSameOrDerivedFrom(has(
                         fieldDecl(hasName("mixin_constructor_marker_")))))),
        // ...and might end up actually being constructed
        unless(hasMethod(isPure())), unless(matchesName("::SameSizeAs")));
    match_finder.addDynamicMatcher(class_missing_mixin_marker, this);
  }

  void run(const MatchFinder::MatchResult& result) {
    auto* bad_class = result.Nodes.getNodeAs<clang::CXXRecordDecl>("bad_class");
    auto* gc_base_class =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("gc_base_class");
    auto* mixin_base_class =
        result.Nodes.getNodeAs<clang::CXXRecordDecl>("mixin_base_class");

    clang::CXXBasePaths paths;
    if (!bad_class->isDerivedFrom(mixin_base_class, paths))
      return;
    const auto& path = paths.front();

    // It's most useful to describe the most derived "mixin" class (i.e. which
    // does not derive the concrete GarbageCollected base).
    auto mixin_it = std::find_if(
        path.begin(), path.end(),
        [gc_base_class](const clang::CXXBasePathElement& path_element) {
          return !path_element.Class->isDerivedFrom(gc_base_class);
        });
    const clang::CXXRecordDecl* mixin_class = mixin_it->Class;
    diagnostics_.MissingMixinMarker(bad_class, mixin_class, path.begin()->Base);

    ++mixin_it;
    for (auto it = path.begin() + 1; it != mixin_it; ++it)
      diagnostics_.MissingMixinMarkerNote(it->Base);
  }

 private:
  DiagnosticsReporter& diagnostics_;
};

}  // namespace

void FindBadPatterns(clang::ASTContext& ast_context,
                     DiagnosticsReporter& diagnostics) {
  MatchFinder match_finder;

  UniquePtrGarbageCollectedMatcher unique_ptr_gc(diagnostics);
  unique_ptr_gc.Register(match_finder);

  OptionalGarbageCollectedMatcher optional_gc(diagnostics);
  optional_gc.Register(match_finder);

  MissingMixinMarker missing_mixin_marker(diagnostics);
  missing_mixin_marker.Register(match_finder);

  match_finder.matchAST(ast_context);
}
