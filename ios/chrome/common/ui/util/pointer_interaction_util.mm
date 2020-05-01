// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(__IPHONE_13_4)

UIButtonPointerStyleProvider CreateDefaultEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK_EQ(button.frame.size.width, button.frame.size.height)
        << "Pointer shape cannot be a circle since button is not square";
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:button.frame
                                cornerRadius:button.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };
}

UIButtonPointerStyleProvider CreateLiftEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4)) {
  return ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                          UIPointerShape* proposedShape) {
    DCHECK_EQ(button.frame.size.width, button.frame.size.height)
        << "Pointer shape cannot be a circle since button is not square";
    UITargetedPreview* preview =
        [[UITargetedPreview alloc] initWithView:button];
    UIPointerLiftEffect* effect =
        [UIPointerLiftEffect effectWithPreview:preview];
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:button.frame
                                cornerRadius:button.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:effect shape:shape];
  };
}

#endif  // defined(__IPHONE_13_4)
