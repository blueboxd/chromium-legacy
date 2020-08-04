// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/text_fragment_utils.h"

#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool AreTextFragmentsAllowed(NavigationContext* context) {
  if (!base::FeatureList::IsEnabled(features::kScrollToTextIOS))
    return false;

  WebState* web_state = context->GetWebState();
  if (web_state->HasOpener()) {
    // TODO(crbug.com/1099268): Loosen this restriction if the opener has the
    // same domain.
    return false;
  }

  return context->HasUserGesture() && !context->IsSameDocument();
}

void HandleTextFragments(NavigationContext* context) {
  // TODO(crbug.com/1099268): Parse URL fragment, execute JS using passed
  // params.
}

}  // namespace web
