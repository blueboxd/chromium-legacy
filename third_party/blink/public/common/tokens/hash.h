// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_HASH_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_HASH_H_

#include "base/unguessable_token.h"

namespace mojo {
template <typename S>
class InlinedStructPtr;
}  // namespace mojo

namespace blink {

// Provides a hash function for the various Mojo wrappers of
// base::UnguessableToken. This is suitable for use with STL and Blink
// containers. This assumes that the MojoTokenStruct is a simple struct that
// contains a single "mojo_base.mojom.UnguessableToken value" member.
template <typename MojoTokenStruct>
struct MojomTokenHash {
  // This requires that the struct be inlined. If this stops working for some
  // reason then the MojoTokenStructPtr should be made a second template
  // parameter.
  using MojoTokenStructPtr = typename mojo::InlinedStructPtr<MojoTokenStruct>;

  size_t operator()(const MojoTokenStruct& token) const {
    return base::UnguessableTokenHash()(token.value);
  }

  size_t operator()(const MojoTokenStruct* token) const {
    return base::UnguessableTokenHash()(token->value);
  }

  size_t operator()(const MojoTokenStructPtr& token) const {
    return base::UnguessableTokenHash()(token->value);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_HASH_H_
