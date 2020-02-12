// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
#include "mojo/public/cpp/bindings/lib/serialization_forward.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {
namespace internal {

template <typename T>
struct Serializer<ScopedHandleBase<T>, ScopedHandleBase<T>> {
  static void Serialize(ScopedHandleBase<T>& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    context->AddHandle(ScopedHandle::From(std::move(input)), output);
  }

  static bool Deserialize(Handle_Data* input,
                          ScopedHandleBase<T>* output,
                          SerializationContext* context) {
    *output = context->TakeHandleAs<T>(*input);
    return true;
  }
};

template <>
struct Serializer<PlatformHandle, PlatformHandle> {
  static void Serialize(PlatformHandle& input,
                        Handle_Data* output,
                        SerializationContext* context) {
    ScopedHandle handle = WrapPlatformHandle(std::move(input));
    DCHECK(handle.is_valid());
    context->AddHandle(std::move(handle), output);
  }

  static bool Deserialize(Handle_Data* input,
                          PlatformHandle* output,
                          SerializationContext* context) {
    *output = UnwrapPlatformHandle(context->TakeHandleAs<Handle>(*input));
    return true;
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_HANDLE_SERIALIZATION_H_
