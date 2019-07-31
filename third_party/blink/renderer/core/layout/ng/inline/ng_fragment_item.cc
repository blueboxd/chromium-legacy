// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGFragmentItem::~NGFragmentItem() {
  switch (Type()) {
    case kText:
      text_.~Text();
      break;
    case kGeneratedText:
      generated_text_.~GeneratedText();
      break;
    case kLine:
      line_.~Line();
      break;
    case kBox:
      box_.~Box();
      break;
  }
}

String NGFragmentItem::DebugName() const {
  return "NGFragmentItem";
}

IntRect NGFragmentItem::VisualRect() const {
  // TODO(kojii): Implement.
  return IntRect();
}

}  // namespace blink
