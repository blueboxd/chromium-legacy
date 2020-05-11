// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/fake_prerender_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakePrerenderService::FakePrerenderService() = default;

FakePrerenderService::~FakePrerenderService() = default;

void FakePrerenderService::SetDelegate(id<PreloadControllerDelegate> delegate) {
}

void FakePrerenderService::StartPrerender(const GURL& url,
                                          const web::Referrer& referrer,
                                          ui::PageTransition transition,
                                          bool immediately) {}

bool FakePrerenderService::MaybeLoadPrerenderedURL(
    const GURL& url,
    ui::PageTransition transition,
    Browser* browser) {
  return false;
}

bool FakePrerenderService::IsLoadingPrerender() {
  return false;
}

void FakePrerenderService::CancelPrerender() {}

bool FakePrerenderService::HasPrerenderForUrl(const GURL& url) {
  return false;
}

bool FakePrerenderService::IsWebStatePrerendered(web::WebState* web_state) {
  return web_state == prerender_web_state_;
}
