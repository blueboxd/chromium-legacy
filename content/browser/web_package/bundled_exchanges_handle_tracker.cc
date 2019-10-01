// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_handle_tracker.h"

#include "content/browser/web_package/bundled_exchanges_handle.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "url/gurl.h"

namespace content {

BundledExchangesHandleTracker::BundledExchangesHandleTracker(
    scoped_refptr<BundledExchangesReader> reader)
    : reader_(std::move(reader)) {
  DCHECK(reader_);
}

BundledExchangesHandleTracker::~BundledExchangesHandleTracker() = default;

std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandleTracker::MaybeCreateBundledExchangesHandle(
    const GURL& url) {
  if (!reader_->HasEntry(url))
    return nullptr;
  return BundledExchangesHandle::CreateForTrackedNavigation(reader_);
}

}  // namespace content
