// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_CLIPBOARD_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_CLIPBOARD_MOJOM_TRAITS_H_

#include "ui/base/clipboard/clipboard_types.h"
#include "ui/base/mojom/clipboard.mojom.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::ClipboardType, ui::ClipboardType> {
  static ui::mojom::ClipboardType ToMojom(ui::ClipboardType type) {
    switch (type) {
      case ui::ClipboardType::kCopyPaste:
        return ui::mojom::ClipboardType::COPY_PASTE;
      case ui::ClipboardType::kSelection:
        return ui::mojom::ClipboardType::SELECTION;
      case ui::ClipboardType::kDrag:
        return ui::mojom::ClipboardType::DRAG;
    }
  }

  static bool FromMojom(ui::mojom::ClipboardType type, ui::ClipboardType* out) {
    switch (type) {
      case ui::mojom::ClipboardType::COPY_PASTE:
        *out = ui::ClipboardType::kCopyPaste;
        return true;
      case ui::mojom::ClipboardType::SELECTION:
        *out = ui::ClipboardType::kSelection;
        return true;
      case ui::mojom::ClipboardType::DRAG:
        *out = ui::ClipboardType::kDrag;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_CLIPBOARD_MOJOM_TRAITS_H_
