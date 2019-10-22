// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace http_structured_header {

// This file implements parsing of HTTP structured headers, as defined in
// https://httpwg.org/http-extensions/draft-ietf-httpbis-header-structure.html.
//
// Currently supported data types are:
//  Item:
//   integer: 123
//   string: "abc"
//   token: abc
//   byte sequence: *YWJj*
//  Parameterised list: abc_123;a=1;b=2; cdef_456, ghi;q="9";r="w"
//  List-of-lists: "foo";"bar", "baz", "bat"; "one"
//
// Functions are provided to parse each of these, which are intended to be
// called with the complete value of an HTTP header (that is, any
// sub-structure will be handled internally by the parser; the exported
// functions are not intended to be called on partial header strings.) Input
// values should be ASCII byte strings (non-ASCII characters should not be
// present in Structured Header values, and will cause the entire header to fail
// to parse.)
//
// Currently only limited types (non-negative integers, strings, tokens and
// byte sequences) are supported.
// TODO(1011101): Add support for other types.

class BLINK_COMMON_EXPORT Item {
 public:
  enum ItemType {
    kNullType,
    kIntegerType,
    kStringType,
    kTokenType,
    kByteSequenceType
  };
  Item();
  Item(int64_t value);

  // Constructors for string-like items: Strings, Tokens and Byte Sequences.
  Item(const std::string& value, Item::ItemType type = kStringType);
  Item(std::string&& value, Item::ItemType type = kStringType);

  BLINK_COMMON_EXPORT friend bool operator==(const Item& lhs, const Item& rhs);
  inline friend bool operator!=(const Item& lhs, const Item& rhs) {
    return !(lhs == rhs);
  }

  bool is_null() const { return type_ == kNullType; }
  bool is_integer() const { return type_ == kIntegerType; }
  bool is_string() const { return type_ == kStringType; }
  bool is_token() const { return type_ == kTokenType; }
  bool is_byte_sequence() const { return type_ == kByteSequenceType; }

  int64_t integer() const {
    DCHECK_EQ(type_, kIntegerType);
    return integer_value_;
  }
  // TODO(iclelland): Split up accessors for String, Token and Byte Sequence.
  const std::string& string() const {
    DCHECK(type_ == kStringType || type_ == kTokenType ||
           type_ == kByteSequenceType);
    return string_value_;
  }

 private:
  ItemType type_ = kNullType;
  // TODO(iclelland): Make this class more memory-efficient, replacing the
  // values here with a union or std::variant (when available).
  int64_t integer_value_ = 0;
  std::string string_value_;
};

struct BLINK_COMMON_EXPORT ParameterisedIdentifier {
  using Parameters = std::map<std::string, Item>;

  Item identifier;
  Parameters params;

  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ParameterisedIdentifier& operator=(const ParameterisedIdentifier&);
  ParameterisedIdentifier(Item, const Parameters&);
  ~ParameterisedIdentifier();
};

using ParameterisedList = std::vector<ParameterisedIdentifier>;
using ListOfLists = std::vector<std::vector<Item>>;

// Returns the result of parsing the header value as an Item, if it can be
// parsed as one, or nullopt if it cannot.
BLINK_COMMON_EXPORT base::Optional<Item> ParseItem(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a Parameterised List, if it
// can be parsed as one, or nullopt if it cannot. Note that parameter keys will
// be returned as strings, which are guaranteed to be ASCII-encoded. List items,
// as well as parameter values, will be returned as Items.
BLINK_COMMON_EXPORT base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a List of Lists, if it can
// be parsed as one, or nullopt if it cannot. Inner list items will be returned
// as Items.
BLINK_COMMON_EXPORT base::Optional<ListOfLists> ParseListOfLists(
    const base::StringPiece& str);

}  // namespace http_structured_header
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
