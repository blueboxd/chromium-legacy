// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"

namespace blink {

using Result = CSSSupportsParser::Result;

class CSSSupportsParserTest : public testing::Test {
 public:
  CSSParserContext* MakeContext() {
    return MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
  }

  Vector<CSSParserToken, 32> Tokenize(const String& string) {
    return CSSTokenizer(string).TokenizeToEOF();
  }

  Result SupportsCondition(String string, CSSSupportsParser::Mode mode) {
    CSSParserImpl impl(MakeContext());
    auto tokens = Tokenize(string);
    return CSSSupportsParser::SupportsCondition(tokens, impl, mode);
  }

  Result AtSupports(String string) {
    return SupportsCondition(string, CSSSupportsParser::Mode::kForAtRule);
  }

  Result WindowCSSSupports(String string) {
    return SupportsCondition(string, CSSSupportsParser::Mode::kForWindowCSS);
  }

  Result ConsumeSupportsCondition(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    auto tokens = Tokenize(string);
    CSSParserTokenRange range = tokens;
    return parser.ConsumeSupportsCondition(range);
  }

  Result ConsumeSupportsInParens(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    auto tokens = Tokenize(string);
    CSSParserTokenRange range = tokens;
    return parser.ConsumeSupportsInParens(range);
  }

  Result ConsumeSupportsFeature(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    auto tokens = Tokenize(string);
    CSSParserTokenRange range = tokens;
    return parser.ConsumeSupportsFeature(range);
  }

  Result ConsumeSupportsDecl(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    auto tokens = Tokenize(string);
    CSSParserTokenRange range = tokens;
    return parser.ConsumeSupportsDecl(range);
  }

  Result ConsumeGeneralEnclosed(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    auto tokens = Tokenize(string);
    CSSParserTokenRange range = tokens;
    return parser.ConsumeGeneralEnclosed(range);
  }
};

TEST_F(CSSSupportsParserTest, ResultNot) {
  EXPECT_EQ(Result::kSupported, !Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, !Result::kSupported);
  EXPECT_EQ(Result::kParseFailure, !Result::kParseFailure);
  EXPECT_EQ(Result::kUnknown, !Result::kUnknown);
}

TEST_F(CSSSupportsParserTest, ResultAnd) {
  EXPECT_EQ(Result::kSupported, Result::kSupported & Result::kSupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported & Result::kSupported);
  EXPECT_EQ(Result::kUnsupported, Result::kSupported & Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported & Result::kUnsupported);

  EXPECT_EQ(Result::kParseFailure, Result::kSupported & Result::kParseFailure);
  EXPECT_EQ(Result::kParseFailure, Result::kParseFailure & Result::kSupported);

  EXPECT_EQ(Result::kUnknown, Result::kUnknown & Result::kUnknown);
  EXPECT_EQ(Result::kUnsupported, Result::kSupported & Result::kUnknown);
  EXPECT_EQ(Result::kUnsupported, Result::kUnknown & Result::kSupported);
}

TEST_F(CSSSupportsParserTest, ResultOr) {
  EXPECT_EQ(Result::kSupported, Result::kSupported | Result::kSupported);
  EXPECT_EQ(Result::kSupported, Result::kUnsupported | Result::kSupported);
  EXPECT_EQ(Result::kSupported, Result::kSupported | Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported | Result::kUnsupported);

  EXPECT_EQ(Result::kParseFailure, Result::kSupported | Result::kParseFailure);
  EXPECT_EQ(Result::kParseFailure, Result::kParseFailure | Result::kSupported);

  EXPECT_EQ(Result::kUnknown, Result::kUnknown | Result::kUnknown);
  EXPECT_EQ(Result::kSupported, Result::kSupported | Result::kUnknown);
  EXPECT_EQ(Result::kSupported, Result::kUnknown | Result::kSupported);
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsCondition) {
  // not <supports-in-parens>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsCondition("not (asdf:red)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(not (color:red))"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsCondition("nay (color:red)"));

  // <supports-in-parens> [ and <supports-in-parens> ]*
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:red) and (color:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(color:red) and (asdf:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(asdf:red) and (asdf:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition(
                "(color:red) and (color:green) and (asdf:color)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition(
                "(color:red) and (color:green) and (not (asdf:color))"));

  // <supports-in-parens> [ or <supports-in-parens> ]*
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:asdf) or (color:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(asdf:red) or (asdf:green)"));
  EXPECT_EQ(
      Result::kSupported,
      ConsumeSupportsCondition("(color:red) or (color:green) or (asdf:color)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition(
                "(color:asdf1) or (color:asdf2) or (asdf:asdf2)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition(
                "(color:asdf) or (color:ghjk) or (not (asdf:color))"));

  // <supports-feature>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsCondition("(color:red)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsCondition("(color:asdf)"));

  // <general-enclosed>
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsCondition("asdf(1)"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsInParens) {
  // ( <supports-condition> )
  EXPECT_EQ(Result::kSupported, ConsumeSupportsInParens("(not (asdf:red))"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("(not (color:red))"));

  // <supports-feature>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsInParens("(color:red)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("(color:asdf)"));

  // <general-enclosed>
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("asdf(1)"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsDecl) {
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(color:red)"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(color:    red)"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(color   : red)"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(color   :red)"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("( color:red )"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(--x:red)"));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(--x:\tred) "));
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(--x:\tred) \t "));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsDecl("(color:green !important)"));
  // For some reason EOF is allowed in place of ')' (everywhere in Blink).
  // Seems to be the case in Firefox too.
  EXPECT_EQ(Result::kSupported, ConsumeSupportsDecl("(color:red"));

  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsDecl("(color:asdf)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsDecl("(asdf)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsDecl("(color)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsDecl("(color:)"));

  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl(""));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl("("));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl(")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl("()"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl("color:red)"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsDecl("color:red"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsFeature) {
  EXPECT_EQ(Result::kSupported, ConsumeSupportsFeature("(color:red)"));
}

TEST_F(CSSSupportsParserTest, ConsumeGeneralEnclosed) {
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(asdf)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("( asdf )"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(3)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("max(1, 2)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("asdf(1, 2)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("asdf(1, 2)\t"));

  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed(""));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("("));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed(")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("()"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("color:red"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("asdf"));

  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(asdf)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("( asdf )"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(3)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("max(1, 2)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("asdf(1, 2)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("asdf(1, 2)\t"));

  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed(""));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("("));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed(")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("()"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("color:red"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("asdf"));

  // Invalid <any-value>:
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("(asdf})"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("(asd]f)"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("(\"as\ndf\")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeGeneralEnclosed("(url(as'df))"));

  // Valid <any-value>
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(as;df)"));
  EXPECT_EQ(Result::kUnknown, ConsumeGeneralEnclosed("(as ! df)"));
}

TEST_F(CSSSupportsParserTest, AtSupportsCondition) {
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red)"));
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red) and (color:green)"));
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            AtSupports("not ((color:gjhk) or (color:asdf))"));
  EXPECT_EQ(Result::kSupported,
            AtSupports("(display: none) and ( (display: none) )"));

  EXPECT_EQ(Result::kUnsupported, AtSupports("(color:ghjk) or (color:asdf)"));
  EXPECT_EQ(Result::kUnsupported, AtSupports("(color:ghjk) or asdf(1)"));
  EXPECT_EQ(Result::kParseFailure, AtSupports("color:red"));
  EXPECT_EQ(
      Result::kParseFailure,
      AtSupports("(display: none) and (display: block) or (display: inline)"));
  EXPECT_EQ(Result::kParseFailure,
            AtSupports("not (display: deadbeef) and (display: block)"));
  EXPECT_EQ(Result::kParseFailure,
            AtSupports("(margin: 0) and (display: inline) or (width:1em)"));

  // "and("/"or(" are function tokens, hence not allowed here.
  EXPECT_EQ(Result::kParseFailure, AtSupports("(left:0) and(top:0)"));
  EXPECT_EQ(Result::kParseFailure, AtSupports("(left:0) or(top:0)"));
}

TEST_F(CSSSupportsParserTest, WindowCSSSupportsCondition) {
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("(--x:red)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("( --x:red )"));
  EXPECT_EQ(Result::kSupported,
            WindowCSSSupports("(--x:red) and (color:green)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("(--x:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            WindowCSSSupports("not ((color:gjhk) or (color:asdf))"));

  EXPECT_EQ(Result::kUnsupported,
            WindowCSSSupports("(color:ghjk) or (color:asdf)"));
  EXPECT_EQ(Result::kUnsupported, WindowCSSSupports("(color:ghjk) or asdf(1)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("color:red"));
}

}  // namespace blink
