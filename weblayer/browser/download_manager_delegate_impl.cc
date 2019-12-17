// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_manager_delegate_impl.h"

#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

DownloadManagerDelegateImpl::DownloadManagerDelegateImpl() = default;
DownloadManagerDelegateImpl::~DownloadManagerDelegateImpl() = default;

bool DownloadManagerDelegateImpl::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
  // If there's no DownloadDelegate, the download is simply dropped.
  auto* tab = TabImpl::FromWebContents(web_contents);
  if (!tab)
    return true;

  DownloadDelegate* delegate = tab->download_delegate();
  if (!delegate)
    return true;

  return delegate->InterceptDownload(url, user_agent, content_disposition,
                                     mime_type, content_length);
}

}  // namespace weblayer
