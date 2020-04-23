// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

#include "content/public/browser/browser_thread.h"
#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "weblayer/browser/safe_browsing/safe_browsing_subresource_helper.h"

using content::BrowserThread;

namespace weblayer {

SafeBrowsingUIManager::SafeBrowsingUIManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SafeBrowsingUIManager::~SafeBrowsingUIManager() = default;

void SafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(timvolodine): figure out if we want to send any threat reporting here.
  // Note the base implementation does not send anything.
}

safe_browsing::BaseBlockingPage*
SafeBrowsingUIManager::CreateBlockingPageForSubresource(
    content::WebContents* contents,
    const GURL& blocked_url,
    const UnsafeResource& unsafe_resource) {
  SafeBrowsingSubresourceHelper::CreateForWebContents(contents, this);
  SafeBrowsingBlockingPage* blocking_page =
      SafeBrowsingBlockingPage::CreateBlockingPage(this, contents, blocked_url,
                                                   unsafe_resource);
  return blocking_page;
}

}  // namespace weblayer
