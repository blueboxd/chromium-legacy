// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include "chrome/browser/sharing/click_to_call/feature.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "url/url_constants.h"

bool ShouldOfferClickToCall(content::BrowserContext* browser_context,
                            const GURL& url) {
  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service &&
         base::FeatureList::IsEnabled(kClickToCallUI) &&
         url.SchemeIs(url::kTelScheme) && !url.GetContent().empty();
}
