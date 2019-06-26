// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_package/http_structured_header.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace http_structured_header {

// Test cases are taken from https://github.com/httpwg/structured-header-tests.

TEST(StructuredHeaderTest, ParseItem) {
  struct TestCase {
    const char* name;
    const char* raw;
    const char* expected;  // nullptr if parse error is expected
  } cases[] = {
      // Item
      {"basic token - item", "a_b-c.d3:f%00/*", "a_b-c.d3:f%00/*"},
      {"token with capitals - item", "fooBar", "fooBar"},
      {"token starting with capitals - item", "FooBar", "FooBar"},
      {"bad token - item", "abc$%!", nullptr},
      {"leading whitespace", " foo", "foo"},
      {"trailing whitespace", "foo ", "foo"},
      // Number
      {"basic integer", "42", "42"},
      {"zero integer", "0", "0"},
      {"comma", "2,3", nullptr},
      {"long integer", "9223372036854775807", "9223372036854775807"},
      {"too long integer", "9223372036854775808", nullptr},
      // Byte Sequence
      {"basic binary", "*aGVsbG8=*", "hello"},
      {"empty binary", "**", ""},
      {"bad paddding", "*aGVsbG8*", "hello"},
      {"bad end delimiter", "*aGVsbG8=", nullptr},
      {"extra whitespace", "*aGVsb G8=*", nullptr},
      {"extra chars", "*aGVsbG!8=*", nullptr},
      {"suffix chars", "*aGVsbG8=!*", nullptr},
      {"non-zero pad bits", "*iZ==*", "\x89"},
      {"non-ASCII binary", "*/+Ah*", "\xFF\xE0!"},
      {"base64url binary", "*_-Ah*", nullptr},
      // String
      {"basic string", "\"foo\"", "foo"},
      {"empty string", "\"\"", ""},
      {"long string",
       "\"foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo \"",
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
       "foo foo foo foo foo foo foo foo foo foo foo foo foo foo "},
      {"whitespace string", "\"   \"", "   "},
      {"non-ascii string", "\"f\xC3\xBC\xC3\xBC\"", nullptr},
      {"tab in string", "\"\t\"", nullptr},
      {"newline in string", "\" \n \"", nullptr},
      {"single quoted string", "'foo'", nullptr},
      {"unbalanced string", "\"foo", nullptr},
      {"string quoting", "\"foo \\\"bar\\\" \\\\ baz\"", "foo \"bar\" \\ baz"},
      {"bad string quoting", "\"foo \\,\"", nullptr},
      {"ending string quote", "\"foo \\\"", nullptr},
      {"abruptly ending string quote", "\"foo \\", nullptr},
  };
  for (const auto& c : cases) {
    base::Optional<std::string> result = ParseItem(c.raw);
    if (c.expected) {
      EXPECT_TRUE(result.has_value()) << c.name;
      EXPECT_EQ(*result, c.expected) << c.name;
    } else {
      EXPECT_FALSE(result.has_value()) << c.name;
    }
  }
}

TEST(StructuredHeaderTest, ParseListOfLists) {
  struct TestCase {
    const char* name;
    const char* raw;
    ListOfLists expected;  // empty if parse error is expected
  } cases[] = {
      {"basic list of lists", "1;2, 42;43", {{"1", "2"}, {"42", "43"}}},
      {"empty list of lists", "", {}},
      {"single item list of lists", "42", {{"42"}}},
      {"no whitespace list of lists", "1,42", {{"1"}, {"42"}}},
      {"no inner whitespace list of lists",
       "1;2, 42;43",
       {{"1", "2"}, {"42", "43"}}},
      {"extra whitespace list of lists", "1 , 42", {{"1"}, {"42"}}},
      {"extra inner whitespace list of lists",
       "1 ; 2,42 ; 43",
       {{"1", "2"}, {"42", "43"}}},
      {"trailing comma list of lists", "1;2, 42,", {}},
      {"trailing semicolon list of lists", "1;2, 42;43;", {}},
      {"leading comma list of lists", ",1;2, 42", {}},
      {"leading semicolon list of lists", ";1;2, 42;43", {}},
      {"empty item list of lists", "1,,42", {}},
      {"empty inner item list of lists", "1;;2,42", {}},
  };
  for (const auto& c : cases) {
    base::Optional<ListOfLists> result = ParseListOfLists(c.raw);
    if (!c.expected.empty()) {
      EXPECT_TRUE(result.has_value()) << c.name;
      EXPECT_EQ(*result, c.expected) << c.name;
    } else {
      EXPECT_FALSE(result.has_value()) << c.name;
    }
  }
}

inline bool operator==(const ParameterisedIdentifier& lhs,
                       const ParameterisedIdentifier& rhs) {
  return lhs.identifier == rhs.identifier && lhs.params == rhs.params;
}

TEST(StructuredHeaderTest, ParseParameterisedList) {
  struct TestCase {
    const char* name;
    const char* raw;
    ParameterisedList expected;  // empty if parse error is expected
  } cases[] = {
      {"basic param-list",
       "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"w\"",
       {
           {"abc_123", {{"a", "1"}, {"b", "2"}, {"cdef_456", ""}}},
           {"ghi", {{"q", "9"}, {"r", "w"}}},
       }},
      {"empty param-list", "", {}},
      {"single item param-list",
       "text/html;q=1",
       {{"text/html", {{"q", "1"}}}}},
      {"no whitespace param-list",
       "text/html,text/plain;q=1",
       {{"text/html", {}}, {"text/plain", {{"q", "1"}}}}},
      {"whitespace before = param-list", "text/html, text/plain;q =1", {}},
      {"whitespace after = param-list", "text/html, text/plain;q= 1", {}},
      {"extra whitespace param-list",
       "text/html  ,  text/plain ;  q=1",
       {{"text/html", {}}, {"text/plain", {{"q", "1"}}}}},
      {"duplicate key", "abc;a=1;b=2;a=1", {}},
      {"numeric key", "abc;a=1;1b=2;c=1", {}},
      {"uppercase key", "abc;a=1;B=2;c=1", {}},
      {"bad key", "abc;a=1;b!=2;c=1", {}},
      {"another bad key", "abc;a=1;b==2;c=1", {}},
      {"empty key name", "abc;a=1;=2;c=1", {}},
      {"empty parameter", "abc;a=1;;c=1", {}},
      {"empty list item", "abc;a=1,,def;b=1", {}},
      {"extra semicolon", "abc;a=1;b=1;", {}},
      {"extra comma", "abc;a=1,def;b=1,", {}},
      {"leading semicolon", ";abc;a=1", {}},
      {"leading comma", ",abc;a=1", {}},
  };
  for (const auto& c : cases) {
    base::Optional<ParameterisedList> result = ParseParameterisedList(c.raw);
    if (c.expected.empty()) {
      EXPECT_FALSE(result.has_value()) << c.name;
      continue;
    }
    EXPECT_TRUE(result.has_value()) << c.name;
    EXPECT_EQ(result->size(), c.expected.size()) << c.name;
    if (result->size() == c.expected.size()) {
      for (size_t i = 0; i < c.expected.size(); ++i)
        EXPECT_EQ((*result)[i], c.expected[i]) << c.name;
    }
  }
}

}  // namespace http_structured_header
}  // namespace blink
