// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_handle_tracker.h"

#include "content/browser/web_package/bundled_exchanges_handle.h"
#include "content/browser/web_package/bundled_exchanges_navigation_info.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"

namespace content {

BundledExchangesHandleTracker::BundledExchangesHandleTracker(
    scoped_refptr<BundledExchangesReader> reader,
    const GURL& target_inner_url)
    : reader_(std::move(reader)), target_inner_url_(target_inner_url) {
  DCHECK(reader_);
}

BundledExchangesHandleTracker::~BundledExchangesHandleTracker() = default;

std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandleTracker::MaybeCreateBundledExchangesHandle(
    const GURL& url) {
  if (reader_->HasEntry(url))
    return BundledExchangesHandle::CreateForTrackedNavigation(reader_);
  if (!reader_->source().is_trusted() &&
      url == bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
                 reader_->source().url(), target_inner_url_)) {
    // This happens when the page in an untrustable bundled exchanges file is
    // reloaded.
    return BundledExchangesHandle::CreateForNavigationInfo(
        std::make_unique<BundledExchangesNavigationInfo>(
            reader_->source().Clone(), target_inner_url_));
  }
  return nullptr;
}

}  // namespace content
