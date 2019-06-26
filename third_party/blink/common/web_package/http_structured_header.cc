// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_package/http_structured_header.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace blink {
namespace http_structured_header {

namespace {

#define DIGIT "0123456789"
#define LCALPHA "abcdefghijklmnopqrstuvwxyz"
#define UCALPHA "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.9
constexpr char kTokenChars[] = DIGIT UCALPHA LCALPHA "_-.:%*/";
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.1
constexpr char kKeyChars[] = DIGIT LCALPHA "_-";
#undef DIGIT
#undef LCALPHA
#undef UCALPHA

// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09#section-3.8
bool IsPrintableASCII(char c) {
  return ' ' <= c && c <= '~';  // 0x20 (' ') to 0x7E ('~')
}

// Parser for (a subset of) Structured Headers for HTTP defined in [SH].
// [SH] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-09
class StructuredHeaderParser {
 public:
  explicit StructuredHeaderParser(const base::StringPiece& str) : input_(str) {
    // [SH] 4.2. Step 1. Discard any leading OWS from input_string.
    SkipWhitespaces();
  }

  // Callers should call this after ReadSomething(), to check if parser has
  // consumed all the input successfully.
  bool FinishParsing() {
    // [SH] 4.2 Step 7. Discard any leading OWS from input_string.
    SkipWhitespaces();
    // [SH] 4.2 Step 8. If input_string is not empty, fail parsing.
    return input_.empty();
  }

  // Parses a List of Lists ([SH] 4.2.4).
  base::Optional<ListOfLists> ReadListOfLists() {
    ListOfLists result;
    while (true) {
      std::vector<std::string> inner_list;
      while (true) {
        base::Optional<std::string> item = ReadItem();
        if (!item)
          return base::nullopt;
        inner_list.push_back(*item);
        SkipWhitespaces();
        if (!ConsumeChar(';'))
          break;
        SkipWhitespaces();
      }
      result.push_back(std::move(inner_list));
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
    }
    return result;
  }

  // Parses an Item ([SH] 4.2.7).
  // Currently only limited types (non-negative integers, strings, tokens and
  // byte sequences) are supported, and all types are returned as a string
  // regardless of the item type. E.g. both 123 (number) and "123" (string) are
  // returned as "123".
  // TODO(ksakamoto): Add support for other types, and return a value with type
  // info.
  base::Optional<std::string> ReadItem() {
    if (input_.empty()) {
      DVLOG(1) << "ReadItem: unexpected EOF";
      return base::nullopt;
    }
    switch (input_.front()) {
      case '"':
        return ReadString();
      case '*':
        return ReadByteSequence();
      default:
        if (base::IsAsciiDigit(input_.front()))
          return ReadNumber();
        else
          return ReadToken();
    }
  }

  // Parses a Parameterised List ([SH] 4.2.5).
  base::Optional<ParameterisedList> ReadParameterisedList() {
    ParameterisedList items;
    while (true) {
      base::Optional<ParameterisedIdentifier> item =
          ReadParameterisedIdentifier();
      if (!item)
        return base::nullopt;
      items.push_back(std::move(*item));
      SkipWhitespaces();
      if (!ConsumeChar(','))
        return items;
      SkipWhitespaces();
    }
  }

 private:
  // Parses a Parameterised Identifier ([SH] 4.2.6).
  base::Optional<ParameterisedIdentifier> ReadParameterisedIdentifier() {
    base::Optional<std::string> primary_identifier = ReadToken();
    if (!primary_identifier)
      return base::nullopt;

    ParameterisedIdentifier::Parameters parameters;

    SkipWhitespaces();
    while (ConsumeChar(';')) {
      SkipWhitespaces();

      base::Optional<std::string> name = ReadKey();
      if (!name)
        return base::nullopt;

      std::string value;
      if (ConsumeChar('=')) {
        auto item = ReadItem();
        if (!item)
          return base::nullopt;
        value = *item;
      }
      if (!parameters.insert(std::make_pair(*name, value)).second) {
        DVLOG(1) << "ReadParameterisedIdentifier: duplicated parameter: "
                 << *name;
        return base::nullopt;
      }
      SkipWhitespaces();
    }
    return ParameterisedIdentifier(*primary_identifier, std::move(parameters));
  }

  // Parses a Key ([SH] 4.2.2).
  base::Optional<std::string> ReadKey() {
    if (input_.empty() || !base::IsAsciiLower(input_.front())) {
      LogParseError("ReadKey", "lcalpha");
      return base::nullopt;
    }
    size_t len = input_.find_first_not_of(kKeyChars);
    if (len == base::StringPiece::npos)
      len = input_.size();
    std::string key(input_.substr(0, len));
    input_.remove_prefix(len);
    return key;
  }

  // Parses a Token ([SH] 4.2.10).
  base::Optional<std::string> ReadToken() {
    if (input_.empty() || !base::IsAsciiAlpha(input_.front())) {
      LogParseError("ReadToken", "ALPHA");
      return base::nullopt;
    }
    size_t len = input_.find_first_not_of(kTokenChars);
    if (len == base::StringPiece::npos)
      len = input_.size();
    std::string token(input_.substr(0, len));
    input_.remove_prefix(len);
    return token;
  }

  // Parses a Number ([SH] 4.2.8).
  // Currently only supports non-negative integers.
  base::Optional<std::string> ReadNumber() {
    size_t i = 0;
    for (; i < input_.size(); ++i) {
      if (!base::IsAsciiDigit(input_[i]))
        break;
    }
    if (i == 0) {
      LogParseError("ReadNumber", "DIGIT");
      return base::nullopt;
    }
    std::string output_number_string(input_.substr(0, i));
    input_.remove_prefix(i);

    // Check if it fits in a 64-bit signed integer.
    int64_t n;
    if (!base::StringToInt64(output_number_string, &n))
      return base::nullopt;
    return output_number_string;
  }

  // Parses a String ([SH] 4.2.9).
  base::Optional<std::string> ReadString() {
    std::string s;
    if (!ConsumeChar('"')) {
      LogParseError("ReadString", "'\"'");
      return base::nullopt;
    }
    while (!ConsumeChar('"')) {
      size_t i = 0;
      for (; i < input_.size(); ++i) {
        if (!IsPrintableASCII(input_[i])) {
          DVLOG(1) << "ReadString: non printable-ASCII character";
          return base::nullopt;
        }
        if (input_[i] == '"' || input_[i] == '\\')
          break;
      }
      if (i == input_.size()) {
        DVLOG(1) << "ReadString: missing closing '\"'";
        return base::nullopt;
      }
      s.append(std::string(input_.substr(0, i)));
      input_.remove_prefix(i);
      if (ConsumeChar('\\')) {
        if (input_.empty()) {
          DVLOG(1) << "ReadString: backslash at string end";
          return base::nullopt;
        }
        if (input_[0] != '"' && input_[0] != '\\') {
          DVLOG(1) << "ReadString: invalid escape";
          return base::nullopt;
        }
        s.push_back(input_.front());
        input_.remove_prefix(1);
      }
    }
    return s;
  }

  // Parses a Byte Sequence ([SH] 4.2.11).
  base::Optional<std::string> ReadByteSequence() {
    if (!ConsumeChar('*')) {
      LogParseError("ReadByteSequence", "'*'");
      return base::nullopt;
    }
    size_t len = input_.find('*');
    if (len == base::StringPiece::npos) {
      DVLOG(1) << "ReadByteSequence: missing closing '*'";
      return base::nullopt;
    }
    std::string base64(input_.substr(0, len));
    // Append the necessary padding characters.
    base64.resize((base64.size() + 3) / 4 * 4, '=');

    std::string binary;
    if (!base::Base64Decode(base64, &binary)) {
      DVLOG(1) << "ReadByteSequence: failed to decode base64: " << base64;
      return base::nullopt;
    }
    input_.remove_prefix(len);
    ConsumeChar('*');
    return binary;
  }

  void SkipWhitespaces() {
    input_ = base::TrimWhitespaceASCII(input_, base::TRIM_LEADING);
  }

  bool ConsumeChar(char expected) {
    if (!input_.empty() && input_.front() == expected) {
      input_.remove_prefix(1);
      return true;
    }
    return false;
  }

  void LogParseError(const char* func, const char* expected) {
    DVLOG(1) << func << ": " << expected << " expected, got "
             << (input_.empty() ? "'" + input_.substr(0, 1).as_string() + "'"
                                : "EOS");
  }

  base::StringPiece input_;
  DISALLOW_COPY_AND_ASSIGN(StructuredHeaderParser);
};

}  // namespace

ParameterisedIdentifier::ParameterisedIdentifier(
    const ParameterisedIdentifier&) = default;
ParameterisedIdentifier::ParameterisedIdentifier(const std::string& id,
                                                 const Parameters& ps)
    : identifier(id), params(ps) {}
ParameterisedIdentifier::~ParameterisedIdentifier() = default;

base::Optional<std::string> ParseItem(const base::StringPiece& str) {
  StructuredHeaderParser parser(str);
  base::Optional<std::string> item = parser.ReadItem();
  if (item && parser.FinishParsing())
    return item;
  return base::nullopt;
}

base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str) {
  StructuredHeaderParser parser(str);
  base::Optional<ParameterisedList> param_list = parser.ReadParameterisedList();
  if (param_list && parser.FinishParsing())
    return param_list;
  return base::nullopt;
}

base::Optional<ListOfLists> ParseListOfLists(const base::StringPiece& str) {
  StructuredHeaderParser parser(str);
  base::Optional<ListOfLists> list_of_lists = parser.ReadListOfLists();
  if (list_of_lists && parser.FinishParsing())
    return list_of_lists;
  return base::nullopt;
}

}  // namespace http_structured_header
}  // namespace blink
