// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/colors/incognito_color_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace color {

UIColor* IncognitoDynamicColor(BOOL isIncognito,
                               UIColor* dynamicColor,
                               UIColor* incognitoColor) {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return dynamicColor;
  }
#endif
  return isIncognito ? incognitoColor : dynamicColor;
}

}  // namespace color
