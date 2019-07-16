// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_MESSAGE_LOOP_TYPE_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_MESSAGE_LOOP_TYPE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/message_loop_type.mojom.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    EnumTraits<mojo_base::mojom::MessageLoopType, base::MessageLoop::Type> {
  static mojo_base::mojom::MessageLoopType ToMojom(
      base::MessageLoop::Type input);
  static bool FromMojom(mojo_base::mojom::MessageLoopType input,
                        base::MessageLoop::Type* output);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_MESSAGE_LOOP_TYPE_MOJOM_TRAITS_H_
