// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

namespace blink {

class CORE_EXPORT LayoutNGGrid : public LayoutNGMixin<LayoutBlock> {
 public:
  explicit LayoutNGGrid(Element*);

  void UpdateBlockLayout(bool relayout_children) override;

  const char* GetName() const override { return "LayoutNGGrid"; }

 protected:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectNGGrid ||
           LayoutNGMixin<LayoutBlock>::IsOfType(type);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_LAYOUT_NG_GRID_H_
