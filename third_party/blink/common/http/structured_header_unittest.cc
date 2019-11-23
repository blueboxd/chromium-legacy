// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/http/structured_header.h"

#include <limits>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace http_structured_header {
namespace {

// Helpers to make test cases clearer

Item Token(std::string value) {
  return Item(value, Item::kTokenType);
}

Item Integer(int64_t value) {
  return Item(value);
}

std::pair<std::string, Item> Param(std::string key) {
  return std::make_pair(key, Item());
}

std::pair<std::string, Item> Param(std::string key, int64_t value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> Param(std::string key, std::string value) {
  return std::make_pair(key, Item(value));
}

// Most test cases are taken from
// https://github.com/httpwg/structured-header-tests.
const struct ItemTestCase {
  const char* name;
  const char* raw;
  const base::Optional<Item> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} item_test_cases[] = {
    // Token
    {"basic token - item", "a_b-c.d3:f%00/*", Token("a_b-c.d3:f%00/*")},
    {"token with capitals - item", "fooBar", Token("fooBar")},
    {"token starting with capitals - item", "FooBar", Token("FooBar")},
    {"bad token - item", "abc$%!", base::nullopt},
    {"leading whitespace", " foo", Token("foo"), "foo"},
    {"trailing whitespace", "foo ", Token("foo"), "foo"},
    // Number
    {"basic integer", "42", Integer(42L)},
    {"zero integer", "0", Integer(0L)},
    {"leading 0 zero", "00", Integer(0L), "0"},
    {"negative zero", "-0", Integer(0L), "0"},
    {"double negative zero", "--0", base::nullopt},
    {"negative integer", "-42", Integer(-42L)},
    {"leading zero integer", "042", Integer(42L), "42"},
    {"leading zero negative integer", "-042", Integer(-42L), "-42"},
    {"comma", "2,3", base::nullopt},
    {"negative non-DIGIT first character", "-a23", base::nullopt},
    {"sign out of place", "4-2", base::nullopt},
    {"whitespace after sign", "- 42", base::nullopt},
    {"long integer", "999999999999999", Integer(999999999999999L)},
    {"long negative integer", "-999999999999999", Integer(-999999999999999L)},
    {"too long integer", "1000000000000000", base::nullopt},
    {"negative too long integer", "-1000000000000000", base::nullopt},
    {"simple float", "1.23", Item(1.23)},
    {"negative float", "-1.23", Item(-1.23)},
    {"integral float", "1.0", Item(1.0)},
    {"float, whitespace after decimal", "1. 23", base::nullopt},
    {"float, whitespace before decimal", "1 .23", base::nullopt},
    {"negative float, whitespace after sign", "- 1.23", base::nullopt},
    {"double decimal float", "1.5.4", base::nullopt},
    {"adjacent double decimal float", "1..4", base::nullopt},
    {"float with six fractional digits", "1.123456", Item(1.123456)},
    {"negative float with six fractional digits", "-1.123456", Item(-1.123456)},
    {"float with seven fractional digits", "1.1234567", base::nullopt},
    {"negative float with seven fractional digits", "-1.1234567",
     base::nullopt},
    {"long float", "123456789.123456", Item(123456789.123456)},
    {"long negative float", "-123456789.123456", Item(-123456789.123456)},
    {"too long float", "1234567890.123456", base::nullopt},
    {"negative too long float", "-1234567890.123456", base::nullopt},
    {"long float 2", "12345678901234.1", Item(12345678901234.1),
     "12345678901234.1"},
    {"negative long float 2", "-12345678901234.1", Item(-12345678901234.1),
     "-12345678901234.1"},
    {"too long float 2", "12345678901234.12", base::nullopt},
    {"negative too long float 2", "-12345678901234.12", base::nullopt},
    {"too long float 3", "123456789012345.1", base::nullopt},
    {"negative too long float 3", "-123456789012345.1", base::nullopt},
    // Generated number tests
    {"1 digits of zero", "0", Integer(0), "0"},
    {"1 digit small integer", "1", Integer(1)},
    {"1 digit large integer", "9", Integer(9)},
    {"2 digits of zero", "00", Integer(0), "0"},
    {"2 digit small integer", "11", Integer(11)},
    {"2 digit large integer", "99", Integer(99)},
    {"3 digits of zero", "000", Integer(0), "0"},
    {"3 digit small integer", "111", Integer(111)},
    {"3 digit large integer", "999", Integer(999)},
    {"4 digits of zero", "0000", Integer(0), "0"},
    {"4 digit small integer", "1111", Integer(1111)},
    {"4 digit large integer", "9999", Integer(9999)},
    {"5 digits of zero", "00000", Integer(0), "0"},
    {"5 digit small integer", "11111", Integer(11111)},
    {"5 digit large integer", "99999", Integer(99999)},
    {"6 digits of zero", "000000", Integer(0), "0"},
    {"6 digit small integer", "111111", Integer(111111)},
    {"6 digit large integer", "999999", Integer(999999)},
    {"7 digits of zero", "0000000", Integer(0), "0"},
    {"7 digit small integer", "1111111", Integer(1111111)},
    {"7 digit large integer", "9999999", Integer(9999999)},
    {"8 digits of zero", "00000000", Integer(0), "0"},
    {"8 digit small integer", "11111111", Integer(11111111)},
    {"8 digit large integer", "99999999", Integer(99999999)},
    {"9 digits of zero", "000000000", Integer(0), "0"},
    {"9 digit small integer", "111111111", Integer(111111111)},
    {"9 digit large integer", "999999999", Integer(999999999)},
    {"10 digits of zero", "0000000000", Integer(0), "0"},
    {"10 digit small integer", "1111111111", Integer(1111111111)},
    {"10 digit large integer", "9999999999", Integer(9999999999)},
    {"11 digits of zero", "00000000000", Integer(0), "0"},
    {"11 digit small integer", "11111111111", Integer(11111111111)},
    {"11 digit large integer", "99999999999", Integer(99999999999)},
    {"12 digits of zero", "000000000000", Integer(0), "0"},
    {"12 digit small integer", "111111111111", Integer(111111111111)},
    {"12 digit large integer", "999999999999", Integer(999999999999)},
    {"13 digits of zero", "0000000000000", Integer(0), "0"},
    {"13 digit small integer", "1111111111111", Integer(1111111111111)},
    {"13 digit large integer", "9999999999999", Integer(9999999999999)},
    {"14 digits of zero", "00000000000000", Integer(0), "0"},
    {"14 digit small integer", "11111111111111", Integer(11111111111111)},
    {"14 digit large integer", "99999999999999", Integer(99999999999999)},
    {"15 digits of zero", "000000000000000", Integer(0), "0"},
    {"15 digit small integer", "111111111111111", Integer(111111111111111)},
    {"15 digit large integer", "999999999999999", Integer(999999999999999)},
    {"2 digit 0, 1 fractional small float", "0.1", Item(0.100000), "0.1"},
    {"2 digit, 1 fractional 0 float", "1.0", Item(1.000000), "1.0"},
    {"2 digit, 1 fractional small float", "1.1", Item(1.100000)},
    {"2 digit, 1 fractional large float", "9.9", Item(9.900000)},
    {"3 digit 0, 1 fractional small float", "00.1", Item(0.100000), "0.1"},
    {"3 digit, 1 fractional 0 float", "11.0", Item(11.000000), "11.0"},
    {"3 digit, 1 fractional small float", "11.1", Item(11.100000)},
    {"3 digit, 1 fractional large float", "99.9", Item(99.900000)},
    {"3 digit 0, 2 fractional small float", "0.11", Item(0.110000), "0.11"},
    {"3 digit, 2 fractional 0 float", "1.00", Item(1.000000), "1.0"},
    {"3 digit, 2 fractional small float", "1.11", Item(1.110000)},
    {"3 digit, 2 fractional large float", "9.99", Item(9.990000)},
    {"4 digit 0, 1 fractional small float", "000.1", Item(0.100000), "0.1"},
    {"4 digit, 1 fractional 0 float", "111.0", Item(111.000000), "111.0"},
    {"4 digit, 1 fractional small float", "111.1", Item(111.100000)},
    {"4 digit, 1 fractional large float", "999.9", Item(999.900000)},
    {"4 digit 0, 2 fractional small float", "00.11", Item(0.110000), "0.11"},
    {"4 digit, 2 fractional 0 float", "11.00", Item(11.000000), "11.0"},
    {"4 digit, 2 fractional small float", "11.11", Item(11.110000)},
    {"4 digit, 2 fractional large float", "99.99", Item(99.990000)},
    {"4 digit 0, 3 fractional small float", "0.111", Item(0.111000), "0.111"},
    {"4 digit, 3 fractional 0 float", "1.000", Item(1.000000), "1.0"},
    {"4 digit, 3 fractional small float", "1.111", Item(1.111000)},
    {"4 digit, 3 fractional large float", "9.999", Item(9.999000)},
    {"5 digit 0, 1 fractional small float", "0000.1", Item(0.100000), "0.1"},
    {"5 digit, 1 fractional 0 float", "1111.0", Item(1111.000000), "1111.0"},
    {"5 digit, 1 fractional small float", "1111.1", Item(1111.100000)},
    {"5 digit, 1 fractional large float", "9999.9", Item(9999.900000)},
    {"5 digit 0, 2 fractional small float", "000.11", Item(0.110000), "0.11"},
    {"5 digit, 2 fractional 0 float", "111.00", Item(111.000000), "111.0"},
    {"5 digit, 2 fractional small float", "111.11", Item(111.110000)},
    {"5 digit, 2 fractional large float", "999.99", Item(999.990000)},
    {"5 digit 0, 3 fractional small float", "00.111", Item(0.111000), "0.111"},
    {"5 digit, 3 fractional 0 float", "11.000", Item(11.000000), "11.0"},
    {"5 digit, 3 fractional small float", "11.111", Item(11.111000)},
    {"5 digit, 3 fractional large float", "99.999", Item(99.999000)},
    {"5 digit 0, 4 fractional small float", "0.1111", Item(0.111100), "0.1111"},
    {"5 digit, 4 fractional 0 float", "1.0000", Item(1.000000), "1.0"},
    {"5 digit, 4 fractional small float", "1.1111", Item(1.111100)},
    {"5 digit, 4 fractional large float", "9.9999", Item(9.999900)},
    {"6 digit 0, 1 fractional small float", "00000.1", Item(0.100000), "0.1"},
    {"6 digit, 1 fractional 0 float", "11111.0", Item(11111.000000), "11111.0"},
    {"6 digit, 1 fractional small float", "11111.1", Item(11111.100000)},
    {"6 digit, 1 fractional large float", "99999.9", Item(99999.900000)},
    {"6 digit 0, 2 fractional small float", "0000.11", Item(0.110000), "0.11"},
    {"6 digit, 2 fractional 0 float", "1111.00", Item(1111.000000), "1111.0"},
    {"6 digit, 2 fractional small float", "1111.11", Item(1111.110000)},
    {"6 digit, 2 fractional large float", "9999.99", Item(9999.990000)},
    {"6 digit 0, 3 fractional small float", "000.111", Item(0.111000), "0.111"},
    {"6 digit, 3 fractional 0 float", "111.000", Item(111.000000), "111.0"},
    {"6 digit, 3 fractional small float", "111.111", Item(111.111000)},
    {"6 digit, 3 fractional large float", "999.999", Item(999.999000)},
    {"6 digit 0, 4 fractional small float", "00.1111", Item(0.111100),
     "0.1111"},
    {"6 digit, 4 fractional 0 float", "11.0000", Item(11.000000), "11.0"},
    {"6 digit, 4 fractional small float", "11.1111", Item(11.111100)},
    {"6 digit, 4 fractional large float", "99.9999", Item(99.999900)},
    {"6 digit 0, 5 fractional small float", "0.11111", Item(0.111110),
     "0.11111"},
    {"6 digit, 5 fractional 0 float", "1.00000", Item(1.000000), "1.0"},
    {"6 digit, 5 fractional small float", "1.11111", Item(1.111110)},
    {"6 digit, 5 fractional large float", "9.99999", Item(9.999990)},
    {"7 digit 0, 1 fractional small float", "000000.1", Item(0.100000), "0.1"},
    {"7 digit, 1 fractional 0 float", "111111.0", Item(111111.000000),
     "111111.0"},
    {"7 digit, 1 fractional small float", "111111.1", Item(111111.100000)},
    {"7 digit, 1 fractional large float", "999999.9", Item(999999.900000)},
    {"7 digit 0, 2 fractional small float", "00000.11", Item(0.110000), "0.11"},
    {"7 digit, 2 fractional 0 float", "11111.00", Item(11111.000000),
     "11111.0"},
    {"7 digit, 2 fractional small float", "11111.11", Item(11111.110000)},
    {"7 digit, 2 fractional large float", "99999.99", Item(99999.990000)},
    {"7 digit 0, 3 fractional small float", "0000.111", Item(0.111000),
     "0.111"},
    {"7 digit, 3 fractional 0 float", "1111.000", Item(1111.000000), "1111.0"},
    {"7 digit, 3 fractional small float", "1111.111", Item(1111.111000)},
    {"7 digit, 3 fractional large float", "9999.999", Item(9999.999000)},
    {"7 digit 0, 4 fractional small float", "000.1111", Item(0.111100),
     "0.1111"},
    {"7 digit, 4 fractional 0 float", "111.0000", Item(111.000000), "111.0"},
    {"7 digit, 4 fractional small float", "111.1111", Item(111.111100)},
    {"7 digit, 4 fractional large float", "999.9999", Item(999.999900)},
    {"7 digit 0, 5 fractional small float", "00.11111", Item(0.111110),
     "0.11111"},
    {"7 digit, 5 fractional 0 float", "11.00000", Item(11.000000), "11.0"},
    {"7 digit, 5 fractional small float", "11.11111", Item(11.111110)},
    {"7 digit, 5 fractional large float", "99.99999", Item(99.999990)},
    {"7 digit 0, 6 fractional small float", "0.111111", Item(0.111111),
     "0.111111"},
    {"7 digit, 6 fractional 0 float", "1.000000", Item(1.000000), "1.0"},
    {"7 digit, 6 fractional small float", "1.111111", Item(1.111111)},
    {"7 digit, 6 fractional large float", "9.999999", Item(9.999999)},
    {"8 digit 0, 1 fractional small float", "0000000.1", Item(0.100000), "0.1"},
    {"8 digit, 1 fractional 0 float", "1111111.0", Item(1111111.000000),
     "1111111.0"},
    {"8 digit, 1 fractional small float", "1111111.1", Item(1111111.100000)},
    {"8 digit, 1 fractional large float", "9999999.9", Item(9999999.900000)},
    {"8 digit 0, 2 fractional small float", "000000.11", Item(0.110000),
     "0.11"},
    {"8 digit, 2 fractional 0 float", "111111.00", Item(111111.000000),
     "111111.0"},
    {"8 digit, 2 fractional small float", "111111.11", Item(111111.110000)},
    {"8 digit, 2 fractional large float", "999999.99", Item(999999.990000)},
    {"8 digit 0, 3 fractional small float", "00000.111", Item(0.111000),
     "0.111"},
    {"8 digit, 3 fractional 0 float", "11111.000", Item(11111.000000),
     "11111.0"},
    {"8 digit, 3 fractional small float", "11111.111", Item(11111.111000)},
    {"8 digit, 3 fractional large float", "99999.999", Item(99999.999000)},
    {"8 digit 0, 4 fractional small float", "0000.1111", Item(0.111100),
     "0.1111"},
    {"8 digit, 4 fractional 0 float", "1111.0000", Item(1111.000000), "1111.0"},
    {"8 digit, 4 fractional small float", "1111.1111", Item(1111.111100)},
    {"8 digit, 4 fractional large float", "9999.9999", Item(9999.999900)},
    {"8 digit 0, 5 fractional small float", "000.11111", Item(0.111110),
     "0.11111"},
    {"8 digit, 5 fractional 0 float", "111.00000", Item(111.000000), "111.0"},
    {"8 digit, 5 fractional small float", "111.11111", Item(111.111110)},
    {"8 digit, 5 fractional large float", "999.99999", Item(999.999990)},
    {"8 digit 0, 6 fractional small float", "00.111111", Item(0.111111),
     "0.111111"},
    {"8 digit, 6 fractional 0 float", "11.000000", Item(11.000000), "11.0"},
    {"8 digit, 6 fractional small float", "11.111111", Item(11.111111)},
    {"8 digit, 6 fractional large float", "99.999999", Item(99.999999)},
    {"9 digit 0, 1 fractional small float", "00000000.1", Item(0.100000),
     "0.1"},
    {"9 digit, 1 fractional 0 float", "11111111.0", Item(11111111.000000),
     "11111111.0"},
    {"9 digit, 1 fractional small float", "11111111.1", Item(11111111.100000)},
    {"9 digit, 1 fractional large float", "99999999.9", Item(99999999.900000)},
    {"9 digit 0, 2 fractional small float", "0000000.11", Item(0.110000),
     "0.11"},
    {"9 digit, 2 fractional 0 float", "1111111.00", Item(1111111.000000),
     "1111111.0"},
    {"9 digit, 2 fractional small float", "1111111.11", Item(1111111.110000)},
    {"9 digit, 2 fractional large float", "9999999.99", Item(9999999.990000)},
    {"9 digit 0, 3 fractional small float", "000000.111", Item(0.111000),
     "0.111"},
    {"9 digit, 3 fractional 0 float", "111111.000", Item(111111.000000),
     "111111.0"},
    {"9 digit, 3 fractional small float", "111111.111", Item(111111.111000)},
    {"9 digit, 3 fractional large float", "999999.999", Item(999999.999000)},
    {"9 digit 0, 4 fractional small float", "00000.1111", Item(0.111100),
     "0.1111"},
    {"9 digit, 4 fractional 0 float", "11111.0000", Item(11111.000000),
     "11111.0"},
    {"9 digit, 4 fractional small float", "11111.1111", Item(11111.111100)},
    {"9 digit, 4 fractional large float", "99999.9999", Item(99999.999900)},
    {"9 digit 0, 5 fractional small float", "0000.11111", Item(0.111110),
     "0.11111"},
    {"9 digit, 5 fractional 0 float", "1111.00000", Item(1111.000000),
     "1111.0"},
    {"9 digit, 5 fractional small float", "1111.11111", Item(1111.111110)},
    {"9 digit, 5 fractional large float", "9999.99999", Item(9999.999990)},
    {"9 digit 0, 6 fractional small float", "000.111111", Item(0.111111),
     "0.111111"},
    {"9 digit, 6 fractional 0 float", "111.000000", Item(111.000000), "111.0"},
    {"9 digit, 6 fractional small float", "111.111111", Item(111.111111)},
    {"9 digit, 6 fractional large float", "999.999999", Item(999.999999)},
    {"10 digit 0, 1 fractional small float", "000000000.1", Item(0.100000),
     "0.1"},
    {"10 digit, 1 fractional 0 float", "111111111.0", Item(111111111.000000),
     "111111111.0"},
    {"10 digit, 1 fractional small float", "111111111.1",
     Item(111111111.100000)},
    {"10 digit, 1 fractional large float", "999999999.9",
     Item(999999999.900000)},
    {"10 digit 0, 2 fractional small float", "00000000.11", Item(0.110000),
     "0.11"},
    {"10 digit, 2 fractional 0 float", "11111111.00", Item(11111111.000000),
     "11111111.0"},
    {"10 digit, 2 fractional small float", "11111111.11",
     Item(11111111.110000)},
    {"10 digit, 2 fractional large float", "99999999.99",
     Item(99999999.990000)},
    {"10 digit 0, 3 fractional small float", "0000000.111", Item(0.111000),
     "0.111"},
    {"10 digit, 3 fractional 0 float", "1111111.000", Item(1111111.000000),
     "1111111.0"},
    {"10 digit, 3 fractional small float", "1111111.111", Item(1111111.111000)},
    {"10 digit, 3 fractional large float", "9999999.999", Item(9999999.999000)},
    {"10 digit 0, 4 fractional small float", "000000.1111", Item(0.111100),
     "0.1111"},
    {"10 digit, 4 fractional 0 float", "111111.0000", Item(111111.000000),
     "111111.0"},
    {"10 digit, 4 fractional small float", "111111.1111", Item(111111.111100)},
    {"10 digit, 4 fractional large float", "999999.9999", Item(999999.999900)},
    {"10 digit 0, 5 fractional small float", "00000.11111", Item(0.111110),
     "0.11111"},
    {"10 digit, 5 fractional 0 float", "11111.00000", Item(11111.000000),
     "11111.0"},
    {"10 digit, 5 fractional small float", "11111.11111", Item(11111.111110)},
    {"10 digit, 5 fractional large float", "99999.99999", Item(99999.999990)},
    {"10 digit 0, 6 fractional small float", "0000.111111", Item(0.111111),
     "0.111111"},
    {"10 digit, 6 fractional 0 float", "1111.000000", Item(1111.000000),
     "1111.0"},
    {"10 digit, 6 fractional small float", "1111.111111", Item(1111.111111)},
    {"10 digit, 6 fractional large float", "9999.999999", Item(9999.999999)},
    {"11 digit 0, 1 fractional small float", "0000000000.1", Item(0.100000),
     "0.1"},
    {"11 digit, 1 fractional 0 float", "1111111111.0", Item(1111111111.000000),
     "1111111111.0"},
    {"11 digit, 1 fractional small float", "1111111111.1",
     Item(1111111111.100000)},
    {"11 digit, 1 fractional large float", "9999999999.9",
     Item(9999999999.900000)},
    {"11 digit 0, 2 fractional small float", "000000000.11", Item(0.110000),
     "0.11"},
    {"11 digit, 2 fractional 0 float", "111111111.00", Item(111111111.000000),
     "111111111.0"},
    {"11 digit, 2 fractional small float", "111111111.11",
     Item(111111111.110000)},
    {"11 digit, 2 fractional large float", "999999999.99",
     Item(999999999.990000)},
    {"11 digit 0, 3 fractional small float", "00000000.111", Item(0.111000),
     "0.111"},
    {"11 digit, 3 fractional 0 float", "11111111.000", Item(11111111.000000),
     "11111111.0"},
    {"11 digit, 3 fractional small float", "11111111.111",
     Item(11111111.111000)},
    {"11 digit, 3 fractional large float", "99999999.999",
     Item(99999999.999000)},
    {"11 digit 0, 4 fractional small float", "0000000.1111", Item(0.111100),
     "0.1111"},
    {"11 digit, 4 fractional 0 float", "1111111.0000", Item(1111111.000000),
     "1111111.0"},
    {"11 digit, 4 fractional small float", "1111111.1111",
     Item(1111111.111100)},
    {"11 digit, 4 fractional large float", "9999999.9999",
     Item(9999999.999900)},
    {"11 digit 0, 5 fractional small float", "000000.11111", Item(0.111110),
     "0.11111"},
    {"11 digit, 5 fractional 0 float", "111111.00000", Item(111111.000000),
     "111111.0"},
    {"11 digit, 5 fractional small float", "111111.11111", Item(111111.111110)},
    {"11 digit, 5 fractional large float", "999999.99999", Item(999999.999990)},
    {"11 digit 0, 6 fractional small float", "00000.111111", Item(0.111111),
     "0.111111"},
    {"11 digit, 6 fractional 0 float", "11111.000000", Item(11111.000000),
     "11111.0"},
    {"11 digit, 6 fractional small float", "11111.111111", Item(11111.111111)},
    {"11 digit, 6 fractional large float", "99999.999999", Item(99999.999999)},
    {"12 digit 0, 1 fractional small float", "00000000000.1", Item(0.100000),
     "0.1"},
    {"12 digit, 1 fractional 0 float", "11111111111.0",
     Item(11111111111.000000), "11111111111.0"},
    {"12 digit, 1 fractional small float", "11111111111.1",
     Item(11111111111.100000)},
    {"12 digit, 1 fractional large float", "99999999999.9",
     Item(99999999999.899994)},
    {"12 digit 0, 2 fractional small float", "0000000000.11", Item(0.110000),
     "0.11"},
    {"12 digit, 2 fractional 0 float", "1111111111.00", Item(1111111111.000000),
     "1111111111.0"},
    {"12 digit, 2 fractional small float", "1111111111.11",
     Item(1111111111.110000)},
    {"12 digit, 2 fractional large float", "9999999999.99",
     Item(9999999999.990000)},
    {"12 digit 0, 3 fractional small float", "000000000.111", Item(0.111000),
     "0.111"},
    {"12 digit, 3 fractional 0 float", "111111111.000", Item(111111111.000000),
     "111111111.0"},
    {"12 digit, 3 fractional small float", "111111111.111",
     Item(111111111.111000)},
    {"12 digit, 3 fractional large float", "999999999.999",
     Item(999999999.999000)},
    {"12 digit 0, 4 fractional small float", "00000000.1111", Item(0.111100),
     "0.1111"},
    {"12 digit, 4 fractional 0 float", "11111111.0000", Item(11111111.000000),
     "11111111.0"},
    {"12 digit, 4 fractional small float", "11111111.1111",
     Item(11111111.111100)},
    {"12 digit, 4 fractional large float", "99999999.9999",
     Item(99999999.999900)},
    {"12 digit 0, 5 fractional small float", "0000000.11111", Item(0.111110),
     "0.11111"},
    {"12 digit, 5 fractional 0 float", "1111111.00000", Item(1111111.000000),
     "1111111.0"},
    {"12 digit, 5 fractional small float", "1111111.11111",
     Item(1111111.111110)},
    {"12 digit, 5 fractional large float", "9999999.99999",
     Item(9999999.999990)},
    {"12 digit 0, 6 fractional small float", "000000.111111", Item(0.111111),
     "0.111111"},
    {"12 digit, 6 fractional 0 float", "111111.000000", Item(111111.000000),
     "111111.0"},
    {"12 digit, 6 fractional small float", "111111.111111",
     Item(111111.111111)},
    {"12 digit, 6 fractional large float", "999999.999999",
     Item(999999.999999)},
    {"13 digit 0, 1 fractional small float", "000000000000.1", Item(0.100000),
     "0.1"},
    {"13 digit, 1 fractional 0 float", "111111111111.0",
     Item(111111111111.000000), "111111111111.0"},
    {"13 digit, 1 fractional small float", "111111111111.1",
     Item(111111111111.100006)},
    {"13 digit, 1 fractional large float", "999999999999.9",
     Item(999999999999.900024)},
    {"13 digit 0, 2 fractional small float", "00000000000.11", Item(0.110000),
     "0.11"},
    {"13 digit, 2 fractional 0 float", "11111111111.00",
     Item(11111111111.000000), "11111111111.0"},
    {"13 digit, 2 fractional small float", "11111111111.11",
     Item(11111111111.110001)},
    {"13 digit, 2 fractional large float", "99999999999.99",
     Item(99999999999.990005)},
    {"13 digit 0, 3 fractional small float", "0000000000.111", Item(0.111000),
     "0.111"},
    {"13 digit, 3 fractional 0 float", "1111111111.000",
     Item(1111111111.000000), "1111111111.0"},
    {"13 digit, 3 fractional small float", "1111111111.111",
     Item(1111111111.111000)},
    {"13 digit, 3 fractional large float", "9999999999.999",
     Item(9999999999.999001)},
    {"13 digit 0, 4 fractional small float", "000000000.1111", Item(0.111100),
     "0.1111"},
    {"13 digit, 4 fractional 0 float", "111111111.0000", Item(111111111.000000),
     "111111111.0"},
    {"13 digit, 4 fractional small float", "111111111.1111",
     Item(111111111.111100)},
    {"13 digit, 4 fractional large float", "999999999.9999",
     Item(999999999.999900)},
    {"13 digit 0, 5 fractional small float", "00000000.11111", Item(0.111110),
     "0.11111"},
    {"13 digit, 5 fractional 0 float", "11111111.00000", Item(11111111.000000),
     "11111111.0"},
    {"13 digit, 5 fractional small float", "11111111.11111",
     Item(11111111.111110)},
    {"13 digit, 5 fractional large float", "99999999.99999",
     Item(99999999.999990)},
    {"13 digit 0, 6 fractional small float", "0000000.111111", Item(0.111111),
     "0.111111"},
    {"13 digit, 6 fractional 0 float", "1111111.000000", Item(1111111.000000),
     "1111111.0"},
    {"13 digit, 6 fractional small float", "1111111.111111",
     Item(1111111.111111)},
    {"13 digit, 6 fractional large float", "9999999.999999",
     Item(9999999.999999)},
    {"14 digit 0, 1 fractional small float", "0000000000000.1", Item(0.100000),
     "0.1"},
    {"14 digit, 1 fractional 0 float", "1111111111111.0",
     Item(1111111111111.000000), "1111111111111.0"},
    {"14 digit, 1 fractional small float", "1111111111111.1",
     Item(1111111111111.100098)},
    {"14 digit, 1 fractional large float", "9999999999999.9",
     Item(9999999999999.900391)},
    {"14 digit 0, 2 fractional small float", "000000000000.11", Item(0.110000),
     "0.11"},
    {"14 digit, 2 fractional 0 float", "111111111111.00",
     Item(111111111111.000000), "111111111111.0"},
    {"14 digit, 2 fractional small float", "111111111111.11",
     Item(111111111111.110001)},
    {"14 digit, 2 fractional large float", "999999999999.99",
     Item(999999999999.989990)},
    {"14 digit 0, 3 fractional small float", "00000000000.111", Item(0.111000),
     "0.111"},
    {"14 digit, 3 fractional 0 float", "11111111111.000",
     Item(11111111111.000000), "11111111111.0"},
    {"14 digit, 3 fractional small float", "11111111111.111",
     Item(11111111111.111000)},
    {"14 digit, 3 fractional large float", "99999999999.999",
     Item(99999999999.998993)},
    {"14 digit 0, 4 fractional small float", "0000000000.1111", Item(0.111100),
     "0.1111"},
    {"14 digit, 4 fractional 0 float", "1111111111.0000",
     Item(1111111111.000000), "1111111111.0"},
    {"14 digit, 4 fractional small float", "1111111111.1111",
     Item(1111111111.111100)},
    {"14 digit, 4 fractional large float", "9999999999.9999",
     Item(9999999999.999901)},
    {"14 digit 0, 5 fractional small float", "000000000.11111", Item(0.111110),
     "0.11111"},
    {"14 digit, 5 fractional 0 float", "111111111.00000",
     Item(111111111.000000), "111111111.0"},
    {"14 digit, 5 fractional small float", "111111111.11111",
     Item(111111111.111110)},
    {"14 digit, 5 fractional large float", "999999999.99999",
     Item(999999999.999990)},
    {"14 digit 0, 6 fractional small float", "00000000.111111", Item(0.111111),
     "0.111111"},
    {"14 digit, 6 fractional 0 float", "11111111.000000", Item(11111111.000000),
     "11111111.0"},
    {"14 digit, 6 fractional small float", "11111111.111111",
     Item(11111111.111111)},
    {"14 digit, 6 fractional large float", "99999999.999999",
     Item(99999999.999999)},
    {"15 digit 0, 1 fractional small float", "00000000000000.1", Item(0.100000),
     "0.1"},
    {"15 digit, 1 fractional 0 float", "11111111111111.0",
     Item(11111111111111.000000), "11111111111111.0"},
    {"15 digit, 1 fractional small float", "11111111111111.1",
     Item(11111111111111.099609)},
    {"15 digit, 1 fractional large float", "99999999999999.9",
     Item(99999999999999.906250)},
    {"15 digit 0, 2 fractional small float", "0000000000000.11", Item(0.110000),
     "0.11"},
    {"15 digit, 2 fractional 0 float", "1111111111111.00",
     Item(1111111111111.000000), "1111111111111.0"},
    {"15 digit, 2 fractional small float", "1111111111111.11",
     Item(1111111111111.110107)},
    {"15 digit, 2 fractional large float", "9999999999999.99",
     Item(9999999999999.990234)},
    {"15 digit 0, 3 fractional small float", "000000000000.111", Item(0.111000),
     "0.111"},
    {"15 digit, 3 fractional 0 float", "111111111111.000",
     Item(111111111111.000000), "111111111111.0"},
    {"15 digit, 3 fractional small float", "111111111111.111",
     Item(111111111111.110992)},
    {"15 digit, 3 fractional large float", "999999999999.999",
     Item(999999999999.999023)},
    {"15 digit 0, 4 fractional small float", "00000000000.1111", Item(0.111100),
     "0.1111"},
    {"15 digit, 4 fractional 0 float", "11111111111.0000",
     Item(11111111111.000000), "11111111111.0"},
    {"15 digit, 4 fractional small float", "11111111111.1111",
     Item(11111111111.111099)},
    {"15 digit, 4 fractional large float", "99999999999.9999",
     Item(99999999999.999893)},
    {"15 digit 0, 5 fractional small float", "0000000000.11111", Item(0.111110),
     "0.11111"},
    {"15 digit, 5 fractional 0 float", "1111111111.00000",
     Item(1111111111.000000), "1111111111.0"},
    {"15 digit, 5 fractional small float", "1111111111.11111",
     Item(1111111111.111110)},
    {"15 digit, 5 fractional large float", "9999999999.99999",
     Item(9999999999.999990)},
    {"15 digit 0, 6 fractional small float", "000000000.111111", Item(0.111111),
     "0.111111"},
    {"15 digit, 6 fractional 0 float", "111111111.000000",
     Item(111111111.000000), "111111111.0"},
    {"15 digit, 6 fractional small float", "111111111.111111",
     Item(111111111.111111)},
    {"15 digit, 6 fractional large float", "999999999.999999",
     Item(999999999.999999)},
    {"too many digit 0 float", "000000000000000.0", base::nullopt},
    {"too many fractional digits 0 float", "000000000.0000000", base::nullopt},
    {"too many digit 9 float", "999999999999999.9", base::nullopt},
    {"too many fractional digits 9 float", "999999999.9999999", base::nullopt},
    // Boolean
    {"basic true boolean", "?1", Item(true)},
    {"basic false boolean", "?0", Item(false)},
    {"unknown boolean", "?Q", base::nullopt},
    {"whitespace boolean", "? 1", base::nullopt},
    {"negative zero boolean", "?-0", base::nullopt},
    {"T boolean", "?T", base::nullopt},
    {"F boolean", "?F", base::nullopt},
    {"t boolean", "?t", base::nullopt},
    {"f boolean", "?f", base::nullopt},
    {"spelled-out True boolean", "?True", base::nullopt},
    {"spelled-out False boolean", "?False", base::nullopt},
    // Byte Sequence
    {"basic binary", "*aGVsbG8=*", Item("hello", Item::kByteSequenceType)},
    {"empty binary", "**", Item("", Item::kByteSequenceType)},
    {"bad paddding", "*aGVsbG8*", Item("hello", Item::kByteSequenceType),
     "*aGVsbG8=*"},
    {"bad end delimiter", "*aGVsbG8=", base::nullopt},
    {"extra whitespace", "*aGVsb G8=*", base::nullopt},
    {"extra chars", "*aGVsbG!8=*", base::nullopt},
    {"suffix chars", "*aGVsbG8=!*", base::nullopt},
    {"non-zero pad bits", "*iZ==*", Item("\x89", Item::kByteSequenceType),
     "*iQ==*"},
    {"non-ASCII binary", "*/+Ah*", Item("\xFF\xE0!", Item::kByteSequenceType)},
    {"base64url binary", "*_-Ah*", base::nullopt},
    // String
    {"basic string", "\"foo\"", Item("foo")},
    {"empty string", "\"\"", Item("")},
    {"long string",
     "\"foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo \"",
     Item("foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo ")},
    {"whitespace string", "\"   \"", Item("   ")},
    {"non-ascii string", "\"f\xC3\xBC\xC3\xBC\"", base::nullopt},
    {"tab in string", "\"\t\"", base::nullopt},
    {"newline in string", "\" \n \"", base::nullopt},
    {"single quoted string", "'foo'", base::nullopt},
    {"unbalanced string", "\"foo", base::nullopt},
    {"string quoting", "\"foo \\\"bar\\\" \\\\ baz\"",
     Item("foo \"bar\" \\ baz")},
    {"bad string quoting", "\"foo \\,\"", base::nullopt},
    {"ending string quote", "\"foo \\\"", base::nullopt},
    {"abruptly ending string quote", "\"foo \\", base::nullopt},
    // Additional tests
    {"valid quoting containing \\n", "\"\\\\n\"", Item("\\n")},
    {"valid quoting containing \\t", "\"\\\\t\"", Item("\\t")},
    {"valid quoting containing \\x", "\"\\\\x61\"", Item("\\x61")},
    {"c-style hex escape in string", "\"\\x61\"", base::nullopt},
    {"valid quoting containing \\u", "\"\\\\u0061\"", Item("\\u0061")},
    {"c-style unicode escape in string", "\"\\u0061\"", base::nullopt},
};

// For Structured Headers Draft 13
const struct ListTestCase {
  const char* name;
  const char* raw;
  const base::Optional<List> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} list_test_cases[] = {
    // Basic lists
    {"basic list", "1, 42", {{{Integer(1L), {}}, {Integer(42L), {}}}}},
    {"empty list", "", List()},
    {"single item list", "42", {{{Integer(42L), {}}}}},
    {"no whitespace list",
     "1,42",
     {{{Integer(1L), {}}, {Integer(42L), {}}}},
     "1, 42"},
    {"trailing comma list", "1, 42,", base::nullopt},
    {"empty item list", "1,,42", base::nullopt},
    // Lists of lists
    {"basic list of lists",
     "(1 2), (42 43)",
     {{{{Integer(1L), Integer(2L)}, {}}, {{Integer(42L), Integer(43L)}, {}}}}},
    {"single item list of lists",
     "(42)",
     {{{std::vector<Item>{Integer(42L)}, {}}}}},
    {"empty item list of lists", "()", {{{std::vector<Item>(), {}}}}},
    {"empty middle item list of lists",
     "(1),(),(42)",
     {{{std::vector<Item>{Integer(1L)}, {}},
       {std::vector<Item>(), {}},
       {std::vector<Item>{Integer(42L)}, {}}}},
     "(1), (), (42)"},
    {"extra whitespace list of lists",
     "(1  42)",
     {{{{Integer(1L), Integer(42L)}, {}}}},
     "(1 42)"},
    {"no trailing parenthesis list of lists", "(1 42", base::nullopt},
    {"no trailing parenthesis middle list of lists", "(1 2, (42 43)",
     base::nullopt},
    // Parameterized Lists
    {"basic parameterised list",
     "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"w\"",
     {{{Token("abc_123"), {Param("a", 1), Param("b", 2), Param("cdef_456")}},
       {Token("ghi"), {Param("q", "9"), Param("r", "w")}}}},
     "abc_123;a=1;b=2;cdef_456, ghi;q=\"9\";r=\"w\""},
    {"single item parameterised list",
     "text/html;q=1",
     {{{Token("text/html"), {Param("q", 1)}}}}},
    {"no whitespace parameterised list",
     "text/html,text/plain;q=1",
     {{{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
     "text/html, text/plain;q=1"},
    {"whitespace before = parameterised list", "text/html, text/plain;q =1",
     base::nullopt},
    {"whitespace after = parameterised list", "text/html, text/plain;q= 1",
     base::nullopt},
    {"extra whitespace param-list",
     "text/html  ,  text/plain ;  q=1",
     {{{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
     "text/html, text/plain;q=1"},
    {"empty item parameterised list", "text/html,,text/plain;q=1",
     base::nullopt},
};

}  // namespace

TEST(StructuredHeaderTest, ParseItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    base::Optional<Item> result = ParseItem(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// In Structured Headers Draft 9, integers can be larger than 1e15. This
// behaviour is exposed in the parser for SH09-specific lists, so test it
// through that interface.
TEST(StructuredHeaderTest, SH09LargeInteger) {
  base::Optional<ListOfLists> result =
      ParseListOfLists("9223372036854775807;-9223372036854775807");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, (ListOfLists{{Integer(9223372036854775807L),
                                   Integer(-9223372036854775807L)}}));

  result = ParseListOfLists("9223372036854775808");
  EXPECT_FALSE(result.has_value());

  result = ParseListOfLists("-9223372036854775808");
  EXPECT_FALSE(result.has_value());
}

// For Structured Headers Draft 9
TEST(StructuredHeaderTest, ParseListOfLists) {
  static const struct TestCase {
    const char* name;
    const char* raw;
    ListOfLists expected;  // empty if parse error is expected
  } cases[] = {
      {"basic list of lists",
       "1;2, 42;43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"empty list of lists", "", {}},
      {"single item list of lists", "42", {{Integer(42L)}}},
      {"no whitespace list of lists", "1,42", {{Integer(1L)}, {Integer(42L)}}},
      {"no inner whitespace list of lists",
       "1;2, 42;43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"extra whitespace list of lists",
       "1 , 42",
       {{Integer(1L)}, {Integer(42L)}}},
      {"extra inner whitespace list of lists",
       "1 ; 2,42 ; 43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"trailing comma list of lists", "1;2, 42,", {}},
      {"trailing semicolon list of lists", "1;2, 42;43;", {}},
      {"leading comma list of lists", ",1;2, 42", {}},
      {"leading semicolon list of lists", ";1;2, 42;43", {}},
      {"empty item list of lists", "1,,42", {}},
      {"empty inner item list of lists", "1;;2,42", {}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    base::Optional<ListOfLists> result = ParseListOfLists(c.raw);
    if (!c.expected.empty()) {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(*result, c.expected);
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }
}

// For Structured Headers Draft 9
TEST(StructuredHeaderTest, ParseParameterisedList) {
  static const struct TestCase {
    const char* name;
    const char* raw;
    ParameterisedList expected;  // empty if parse error is expected
  } cases[] = {
      {"basic param-list",
       "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"w\"",
       {
           {Token("abc_123"),
            {Param("a", 1), Param("b", 2), Param("cdef_456")}},
           {Token("ghi"), {Param("q", "9"), Param("r", "w")}},
       }},
      {"empty param-list", "", {}},
      {"single item param-list",
       "text/html;q=1",
       {{Token("text/html"), {Param("q", 1)}}}},
      {"empty param-list", "", {}},
      {"no whitespace param-list",
       "text/html,text/plain;q=1",
       {{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
      {"whitespace before = param-list", "text/html, text/plain;q =1", {}},
      {"whitespace after = param-list", "text/html, text/plain;q= 1", {}},
      {"extra whitespace param-list",
       "text/html  ,  text/plain ;  q=1",
       {{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
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
    SCOPED_TRACE(c.name);
    base::Optional<ParameterisedList> result = ParseParameterisedList(c.raw);
    if (c.expected.empty()) {
      EXPECT_FALSE(result.has_value());
      continue;
    }
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), c.expected.size());
    if (result->size() == c.expected.size()) {
      for (size_t i = 0; i < c.expected.size(); ++i)
        EXPECT_EQ((*result)[i], c.expected[i]);
    }
  }
}

// For Structured Headers Draft 13
TEST(StructuredHeaderTest, ParseList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    base::Optional<List> result = ParseList(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// Serializer tests are all exclusively for Structured Headers Draft 13

TEST(StructuredHeaderTest, SerializeItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      base::Optional<std::string> result = SerializeItem(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, UnserializableItems) {
  // Test that items with unknown type are not serialized.
  EXPECT_FALSE(SerializeItem(Item()).has_value());
}

TEST(StructuredHeaderTest, UnserializableTokens) {
  static const struct UnserializableString {
    const char* name;
    const char* value;
  } bad_tokens[] = {
      {"empty token", ""},
      {"contains high ascii", "a\xff"},
      {"contains nonprintable character", "a\x7f"},
      {"contains C0", "a\x01"},
      {"UTF-8 encoded", "a\xc3\xa9"},
      {"contains TAB", "a\t"},
      {"contains LF", "a\n"},
      {"contains CR", "a\r"},
      {"contains SP", "a "},
      {"begins with digit", "9token"},
      {"begins with hyphen", "-token"},
      {"begins with LF", "\ntoken"},
      {"begins with SP", " token"},
      {"begins with colon", ":token"},
      {"begins with percent", "%token"},
      {"begins with period", ".token"},
      {"begins with asterisk", "*token"},
      {"begins with slash", "/token"},
  };
  for (const auto& bad_token : bad_tokens) {
    SCOPED_TRACE(bad_token.name);
    base::Optional<std::string> serialization =
        SerializeItem(Token(bad_token.value));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableStrings) {
  static const struct UnserializableString {
    const char* name;
    const char* value;
  } bad_strings[] = {
      {"contains high ascii", "a\xff"},
      {"contains nonprintable character", "a\x7f"},
      {"UTF-8 encoded", "a\xc3\xa9"},
      {"contains TAB", "a\t"},
      {"contains LF", "a\n"},
      {"contains CR", "a\r"},
      {"contains C0", "a\x01"},
  };
  for (const auto& bad_string : bad_strings) {
    SCOPED_TRACE(bad_string.name);
    base::Optional<std::string> serialization =
        SerializeItem(Item(bad_string.value));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableIntegers) {
  EXPECT_FALSE(SerializeItem(Integer(1e15L)).has_value());
}

TEST(StructuredHeaderTest, UnserializableFloats) {
  for (double value :
       {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), 1e14, 1e14 - 0.0000001,
        1e14 - 0.05, -1e14, -1e14 + 0.0000001, -1e14 + 0.05}) {
    EXPECT_FALSE(SerializeItem(Item(value)).has_value());
  }
}

// These values cannot be directly parsed from headers, but are valid doubles
// which can be serialized as sh-floats (though rounding is expected.)
TEST(StructuredHeaderTest, SerializeUnparseableFloats) {
  struct UnparseableFloat {
    const char* name;
    double value;
    const char* canonical;
  } float_test_cases[] = {
      {"negative 0", -0.0, "0.0"},
      {"0.0000001", 0.0000001, "0.0"},
      {"0.0000000001", 0.0000000001, "0.0"},
      {"1.0000001", 1.0000001, "1.0"},
      {"1.0000009", 1.0000009, "1.000001"},
      {"subnormal numbers", std::numeric_limits<double>::denorm_min(), "0.0"},
      {"round up to 10 digits", 1e9 - 0.0000001, "1000000000.0"},
      {"round up to 11 digits", 1e10 - 0.000001, "10000000000.0"},
      {"round up to 12 digits", 1e11 - 0.00001, "100000000000.0"},
      {"round up to 13 digits", 1e12 - 0.0001, "1000000000000.0"},
      {"round up to 14 digits", 1e13 - 0.001, "10000000000000.0"},
      {"round down to 14 digits", -1e13 + 0.001, "-10000000000000.0"},
      {"largest serializable float", nextafter(1e14 - 0.05, 0),
       "99999999999999.9"},
      {"largest serializable negative float", -nextafter(1e14 - 0.05, 0),
       "-99999999999999.9"},
      // This will fail if we simply truncate the fractional portion.
      {"float rounds up to next int", 3.9999999, "4.0"},
      // This will fail if we first round to >6 digits, and then round again to
      // 6 digits.
      {"don't double round", 3.99999949, "3.999999"},
      // This will fail if we first round to 6 digits, and then round again to
      // max_avail_digits.
      {"don't double round", 123456789.99999949, "123456789.999999"},
  };
  for (const auto& test_case : float_test_cases) {
    SCOPED_TRACE(test_case.name);
    base::Optional<std::string> serialization =
        SerializeItem(Item(test_case.value));
    EXPECT_TRUE(serialization.has_value());
    EXPECT_EQ(*serialization, test_case.canonical);
  }
}

TEST(StructuredHeaderTest, SerializeList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      base::Optional<std::string> result = SerializeList(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, UnserializableLists) {
  static const struct UnserializableList {
    const char* name;
    const List value;
  } bad_lists[] = {
      {"Null item as member", {{Item(), {}}}},
      {"Unserializable item as member", {{Token("\n"), {}}}},
      {"Key is empty", {{Token("abc"), {Param("", 1)}}}},
      {"Key contains whitespace", {{Token("abc"), {Param("a\n", 1)}}}},
      {"Key contains UTF8", {{Token("abc"), {Param("a\xc3\xa9", 1)}}}},
      {"Key contains unprintable characters",
       {{Token("abc"), {Param("a\x7f", 1)}}}},
      {"Key contains disallowed characters",
       {{Token("abc"), {Param("a:", 1)}}}},
      {"Param value is unserializable", {{Token("abc"), {{"a", Token("\n")}}}}},
      {"Inner list contains unserializable item",
       {{std::vector<Item>{Token("\n")}, {}}}},
  };
  for (const auto& bad_list : bad_lists) {
    SCOPED_TRACE(bad_list.name);
    base::Optional<std::string> serialization = SerializeList(bad_list.value);
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

}  // namespace http_structured_header
}  // namespace blink
