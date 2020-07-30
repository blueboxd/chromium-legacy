// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_TOKEN_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_TOKEN_H_

#include "base/unguessable_token.h"
#include "base/util/type_safety/token_type.h"

namespace performance_manager {
namespace execution_context {

using ExecutionContextToken = util::TokenType<class ExecutionContextTokenTag>;

// These objects should have the identical size and layout; the StrongAlias
// wrapper is simply there to provide type safety. This allows us to
// safely reinterpret_cast behind the scenes, so we can continue to return
// references and pointers.
static_assert(sizeof(ExecutionContextToken) == sizeof(base::UnguessableToken),
              "TokenType should not change object layout");

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_TOKEN_H_
