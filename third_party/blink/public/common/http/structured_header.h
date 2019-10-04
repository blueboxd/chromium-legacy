// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_

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
// Note that all values of Item type are currently returned as strings. The
// string, token and byte sequence examples above would all be returned by the
// parser as the string "abc". It is not currently possible to determine which
// type of Item was parsed from a given header.
// TODO(1011101): Return values with type information attached.
//
// TODO(1011101): The current parser implementation is of draft #9
// (https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09) from
// December 2018; this should be brought up to date with the latest changes.

struct BLINK_COMMON_EXPORT ParameterisedIdentifier {
  using Parameters = std::map<std::string, std::string>;

  std::string identifier;
  Parameters params;

  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ParameterisedIdentifier& operator=(const ParameterisedIdentifier&);
  ParameterisedIdentifier(const std::string&, const Parameters&);
  ~ParameterisedIdentifier();
};

using ParameterisedList = std::vector<ParameterisedIdentifier>;
using ListOfLists = std::vector<std::vector<std::string>>;

// Returns the string representation of the header value, if it can be parsed as
// an Item, or nullopt if it cannot. Note that the returned string is not
// guaranteed to have any encoding, as it may have been a Byte Sequence.
BLINK_COMMON_EXPORT base::Optional<std::string> ParseItem(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a Parameterised List, if it
// can be parsed as one, or nullopt if it cannot. Note that list items, as well
// as parameter values, will be returned as strings, and that those strings are
// not guaranteed to have any encoding, as they may have been Byte Sequences.
BLINK_COMMON_EXPORT base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str);

// Returns the result of parsing the header value as a List of Lists, if it can
// be parsed as one, or nullopt if it cannot. Note that inner list items will be
// be returned as strings, and that those strings are not guaranteed to have any
// encoding, as they may have been Byte Sequences.
BLINK_COMMON_EXPORT base::Optional<ListOfLists> ParseListOfLists(
    const base::StringPiece& str);

}  // namespace http_structured_header
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_HTTP_STRUCTURED_HEADER_H_
