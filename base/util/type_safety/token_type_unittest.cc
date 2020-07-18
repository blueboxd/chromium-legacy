// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/type_safety/token_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace util {

using FooToken = TokenType<class Foo>;

TEST(TokenType, TokenApi) {
  // Test static builders.
  EXPECT_TRUE(FooToken::Null().is_empty());
  EXPECT_FALSE(FooToken::Create().is_empty());

  // Test default initialization.
  FooToken token1;
  EXPECT_TRUE(token1.is_empty());

  // Test copy construction.
  FooToken token2(FooToken::Create());
  EXPECT_FALSE(token2.is_empty());

  // Test assignment.
  FooToken token3;
  EXPECT_TRUE(token3.is_empty());
  token3 = token2;
  EXPECT_FALSE(token3.is_empty());

  // Test bool conversion.
  EXPECT_FALSE(token1);
  EXPECT_TRUE(token2);
  EXPECT_TRUE(token3);

  // Test comparison operators.
  EXPECT_TRUE(token1 == FooToken::Null());
  EXPECT_FALSE(token1 == token2);
  EXPECT_TRUE(token2 == token3);
  EXPECT_FALSE(token1 != FooToken::Null());
  EXPECT_TRUE(token1 != token2);
  EXPECT_FALSE(token2 != token3);
  EXPECT_TRUE(token1 < token2);
  EXPECT_FALSE(token2 < token3);

  // Test hasher.
  EXPECT_EQ(FooToken::Hasher()(token2),
            base::UnguessableTokenHash()(token2.value()));

  // Test string representation.
  EXPECT_EQ(token2.ToString(), token2.value().ToString());
}

}  // namespace util
