// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_TYPE_SAFETY_TOKEN_TYPE_H_
#define BASE_UTIL_TYPE_SAFETY_TOKEN_TYPE_H_

#include "base/unguessable_token.h"
#include "base/util/type_safety/strong_alias.h"

namespace util {

// A specialization of StrongAlias for base::UnguessableToken.
template <typename TypeMarker>
class TokenType : public StrongAlias<TypeMarker, base::UnguessableToken> {
 public:
  // Inherit constructors.
  using StrongAlias<TypeMarker, base::UnguessableToken>::StrongAlias;

  // This object allows default assignment operators for compatibility with
  // STL containers.

  // Hash functor for use in unordered containers.
  struct Hasher {
    using argument_type = TokenType;
    using result_type = size_t;
    result_type operator()(const argument_type& token) const {
      return base::UnguessableTokenHash()(token.value());
    }
  };

  // Mimic the base::UnguessableToken API for ease and familiarity of use.
  static TokenType Create() {
    return TokenType(base::UnguessableToken::Create());
  }
  static const TokenType& Null() {
    static const TokenType kNull;
    return kNull;
  }
  bool is_empty() const { return this->value().is_empty(); }
  std::string ToString() const { return this->value().ToString(); }
  explicit constexpr operator bool() const { return !is_empty(); }
};

}  // namespace util

#endif  // BASE_UTIL_TYPE_SAFETY_TOKEN_TYPE_H_
