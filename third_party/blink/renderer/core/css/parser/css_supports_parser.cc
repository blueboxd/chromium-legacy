// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// The result kUnknown must be converted to 'false' if passed to a context
// which requires a boolean value.
// TODO(crbug.com/1052274): This is supposed to happen at the top-level,
// but currently happens on ConsumeGeneralEnclosed's result.
CSSSupportsParser::Result EvalUnknown(CSSSupportsParser::Result result) {
  return result == CSSSupportsParser::Result::kUnknown
             ? CSSSupportsParser::Result::kUnsupported
             : result;
}

// https://drafts.csswg.org/css-syntax/#typedef-any-value
bool IsNextTokenAllowedForAnyValue(CSSParserTokenRange& range) {
  switch (range.Peek().GetType()) {
    case kBadStringToken:
    case kEOFToken:
    case kBadUrlToken:
      return false;
    case kRightParenthesisToken:
    case kRightBracketToken:
    case kRightBraceToken:
      return range.Peek().GetBlockType() == CSSParserToken::kBlockEnd;
    default:
      return true;
  }
}

// https://drafts.csswg.org/css-syntax/#typedef-any-value
bool ConsumeAnyValue(CSSParserTokenRange& range) {
  DCHECK(!range.AtEnd());
  while (IsNextTokenAllowedForAnyValue(range))
    range.Consume();
  return range.AtEnd();
}

}  // namespace

CSSSupportsParser::Result CSSSupportsParser::SupportsCondition(
    CSSParserTokenRange range,
    CSSParserImpl& parser,
    Mode mode) {
  range.ConsumeWhitespace();
  CSSParserTokenRange stored_range = range;
  CSSSupportsParser supports_parser(parser);
  Result result = supports_parser.ConsumeSupportsCondition(range);
  if (mode != Mode::kForWindowCSS || result != Result::kParseFailure)
    return result;

  // window.CSS.supports requires to parse as-if it was wrapped in parenthesis.
  // The only wrapped production that wouldn't have parsed above is
  // <declaration>.
  if (stored_range.Peek().GetType() != kIdentToken)
    return Result::kParseFailure;
  if (parser.SupportsDeclaration(stored_range))
    return Result::kSupported;
  return Result::kUnsupported;
}

bool CSSSupportsParser::AtIdent(CSSParserTokenRange& range, const char* ident) {
  return range.Peek().GetType() == kIdentToken &&
         EqualIgnoringASCIICase(range.Peek().Value(), ident);
}

bool CSSSupportsParser::ConsumeIfIdent(CSSParserTokenRange& range,
                                       const char* ident) {
  if (!AtIdent(range, ident))
    return false;
  range.ConsumeIncludingWhitespace();
  return true;
}

// <supports-condition> = not <supports-in-parens>
//                   | <supports-in-parens> [ and <supports-in-parens> ]*
//                   | <supports-in-parens> [ or <supports-in-parens> ]*
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsCondition(
    CSSParserTokenRange& range) {
  // not <supports-in-parens>
  if (ConsumeIfIdent(range, "not")) {
    Result result = !ConsumeSupportsInParens(range);
    return range.AtEnd() ? result : Result::kParseFailure;
  }

  // <supports-in-parens> [ and <supports-in-parens> ]*
  // | <supports-in-parens> [ or <supports-in-parens> ]*
  Result result = ConsumeSupportsInParens(range);

  if (AtIdent(range, "and")) {
    while (ConsumeIfIdent(range, "and"))
      result = result & ConsumeSupportsInParens(range);
  } else if (AtIdent(range, "or")) {
    while (ConsumeIfIdent(range, "or"))
      result = result | ConsumeSupportsInParens(range);
  }

  return range.AtEnd() ? result : Result::kParseFailure;
}

// <supports-in-parens> = ( <supports-condition> )
//                    | <supports-feature>
//                    | <general-enclosed>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsInParens(
    CSSParserTokenRange& range) {
  const CSSParserTokenRange stored_range = range;

  // ( <supports-condition> )
  if (range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    block.ConsumeWhitespace();
    range.ConsumeWhitespace();
    Result result = ConsumeSupportsCondition(block);
    if (result != Result::kParseFailure)
      return result;
    // Parsing failed, so try parsing again as <supports-feature>.
    range = stored_range;
  }

  // <supports-feature>
  Result result = ConsumeSupportsFeature(range);
  if (result != Result::kParseFailure)
    return result;
  // Parsing failed, try again as <general-enclosed>
  range = stored_range;

  // <general-enclosed>
  //
  // TODO(crbug.com/1052274): Support kUnknown beyond this point.
  //
  // The result kUnknown is supposed to be evaluated at the top level, but
  // we have already shipped the behavior of evaluating it here, and Firefox
  // does the same thing.
  return EvalUnknown(ConsumeGeneralEnclosed(range));
}

// <supports-feature> = <supports-selector-fn> | <supports-decl>
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsFeature(
    CSSParserTokenRange& range) {
  const CSSParserTokenRange stored_range = range;

  // <supports-selector-fn>
  Result result = ConsumeSupportsSelectorFn(range);
  if (result != Result::kParseFailure)
    return result;
  range = stored_range;

  // <supports-decl>
  return ConsumeSupportsDecl(range);
}

// <supports-selector-fn> = selector( <complex-selector> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsSelectorFn(
    CSSParserTokenRange& range) {
  if (!RuntimeEnabledFeatures::CSSSupportsSelectorEnabled())
    return Result::kParseFailure;
  if (range.Peek().GetType() != kFunctionToken)
    return Result::kParseFailure;
  if (range.Peek().FunctionId() != CSSValueID::kSelector)
    return Result::kParseFailure;
  auto block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  range.ConsumeWhitespace();
  if (CSSSelectorParser::SupportsComplexSelector(block, parser_.GetContext()))
    return Result::kSupported;
  return Result::kUnsupported;
}

// <supports-decl> = ( <declaration> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeSupportsDecl(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kLeftParenthesisToken)
    return Result::kParseFailure;
  auto block = range.ConsumeBlock();
  block.ConsumeWhitespace();
  range.ConsumeWhitespace();
  if (block.Peek().GetType() != kIdentToken)
    return Result::kParseFailure;
  if (parser_.SupportsDeclaration(block))
    return Result::kSupported;
  return Result::kUnsupported;
}

// <general-enclosed> = [ <function-token> <any-value> ) ]
//                  | ( <ident> <any-value> )
CSSSupportsParser::Result CSSSupportsParser::ConsumeGeneralEnclosed(
    CSSParserTokenRange& range) {
  if (range.Peek().GetType() == kFunctionToken ||
      range.Peek().GetType() == kLeftParenthesisToken) {
    auto block = range.ConsumeBlock();
    // Note that <any-value> matches a sequence of one or more tokens, hence the
    // block-range can't be empty.
    // https://drafts.csswg.org/css-syntax-3/#typedef-any-value
    if (block.AtEnd() || !ConsumeAnyValue(block))
      return Result::kParseFailure;
    range.ConsumeWhitespace();
    return Result::kUnknown;
  }
  return Result::kParseFailure;
}

}  // namespace blink
