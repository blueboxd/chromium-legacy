// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_

namespace blink {

struct LogicalSize;
class NGBlockNode;
class NGConstraintSpace;
class NGLayoutInputNode;

// Creates a new constraint space for the current child.
NGConstraintSpace CreateConstraintSpaceForMathChild(
    const NGBlockNode& parent_node,
    const LogicalSize& child_available_size,
    const NGConstraintSpace& parent_constraint_space,
    const NGLayoutInputNode&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
