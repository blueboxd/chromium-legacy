// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_

#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

bool ShouldOfferClickToCall(content::BrowserContext* browser_context,
                            const GURL& url);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_UTILS_H_
