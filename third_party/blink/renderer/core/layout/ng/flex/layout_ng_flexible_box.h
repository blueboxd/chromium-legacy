// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGFlexibleBox : public LayoutNGMixin<LayoutBlock> {
 public:
  explicit LayoutNGFlexibleBox(Element*);

  bool HasTopOverflow() const override;
  bool HasLeftOverflow() const override;

  void UpdateBlockLayout(bool relayout_children) override;

  bool IsFlexibleBoxIncludingDeprecatedAndNG() const final { return true; }
  bool IsFlexibleBoxIncludingNG() const final { return true; }
  const char* GetName() const override { return "LayoutNGFlexibleBox"; }

 protected:
  bool IsChildAllowed(LayoutObject* object,
                      const ComputedStyle& style) const override;
  void RemoveChild(LayoutObject*) override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectNGFlexibleBox ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_LAYOUT_NG_FLEXIBLE_BOX_H_
