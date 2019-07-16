// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/message_loop_type_mojom_traits.h"
#include "build/build_config.h"

namespace mojo {

// static
mojo_base::mojom::MessageLoopType
EnumTraits<mojo_base::mojom::MessageLoopType, base::MessageLoop::Type>::ToMojom(
    base::MessageLoop::Type input) {
  switch (input) {
    case base::MessageLoop::TYPE_DEFAULT:
      return mojo_base::mojom::MessageLoopType::kDefault;
    case base::MessageLoop::TYPE_UI:
      return mojo_base::mojom::MessageLoopType::kUi;
    case base::MessageLoop::TYPE_CUSTOM:
      return mojo_base::mojom::MessageLoopType::kCustom;
    case base::MessageLoop::TYPE_IO:
      return mojo_base::mojom::MessageLoopType::kIo;
#if defined(OS_ANDROID)
    case base::MessageLoop::TYPE_JAVA:
      return mojo_base::mojom::MessageLoopType::kJava;
#endif
#if defined(OS_MACOSX)
    case base::MessageLoop::Type::NS_RUNLOOP:
      return mojo_base::mojom::MessageLoopType::kNsRunloop;
#endif
#if defined(OS_WIN)
    case base::MessageLoop::Type::UI_WITH_WM_QUIT_SUPPORT:
      return mojo_base::mojom::MessageLoopType::kUiWithWmQuitSupport;
#endif
  }
  NOTREACHED();
  return mojo_base::mojom::MessageLoopType::kDefault;
}

// static
bool EnumTraits<mojo_base::mojom::MessageLoopType, base::MessageLoop::Type>::
    FromMojom(mojo_base::mojom::MessageLoopType input,
              base::MessageLoop::Type* output) {
  switch (input) {
    case mojo_base::mojom::MessageLoopType::kDefault:
      *output = base::MessageLoop::TYPE_DEFAULT;
      return true;
    case mojo_base::mojom::MessageLoopType::kUi:
      *output = base::MessageLoop::TYPE_UI;
      return true;
    case mojo_base::mojom::MessageLoopType::kCustom:
      *output = base::MessageLoop::TYPE_CUSTOM;
      return true;
    case mojo_base::mojom::MessageLoopType::kIo:
      *output = base::MessageLoop::TYPE_IO;
      return true;
#if defined(OS_ANDROID)
    case mojo_base::mojom::MessageLoopType::kJava:
      *output = base::MessageLoop::TYPE_JAVA;
      return true;
#endif
#if defined(OS_MACOSX)
    case mojo_base::mojom::MessageLoopType::kNsRunloop:
      *output = base::MessageLoop::Type::NS_RUNLOOP;
      return true;
#endif
#if defined(OS_WIN)
    case mojo_base::mojom::MessageLoopType::kUiWithWmQuitSupport:
      *output = base::MessageLoop::Type::UI_WITH_WM_QUIT_SUPPORT;
      return true;
#endif
  }
  return false;
}

}  // namespace mojo
