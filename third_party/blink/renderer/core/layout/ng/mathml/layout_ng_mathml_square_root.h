// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_SQUARE_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_SQUARE_ROOT_H_

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"

namespace blink {

// FIXME: should probably be called LayoutNGMathMLBlockWithAnonymousMrow
// when there are multiple users of this class.
class LayoutNGMathMLSquareRoot : public LayoutNGMathMLBlock {
 public:
  explicit LayoutNGMathMLSquareRoot(Element*);

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_LAYOUT_NG_MATHML_SQUARE_ROOT_H_
