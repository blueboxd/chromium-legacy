// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_POINTER_INTERACTION_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_POINTER_INTERACTION_UTIL_H_

#import <UIKit/UIKit.h>

#if defined(__IPHONE_13_4)
// Returns a pointer style provider that has the default hover effect and a
// circle pointer shape.
UIButtonPointerStyleProvider CreateDefaultEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4));

// Returns a pointer style provider that has the lift hover effect and a circle
// pointer shape.
UIButtonPointerStyleProvider CreateLiftEffectCirclePointerStyleProvider()
    API_AVAILABLE(ios(13.4));
#endif  // defined(__IPHONE_13_4)

#endif  // IOS_CHROME_COMMON_UI_UTIL_POINTER_INTERACTION_UTIL_H_
