// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
#define WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_

#include "content/public/browser/download_manager_delegate.h"

namespace weblayer {

class DownloadManagerDelegateImpl : public content::DownloadManagerDelegate {
 public:
  DownloadManagerDelegateImpl();
  ~DownloadManagerDelegateImpl() override;

  // content::DownloadManagerDelegate implementation:
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_MANAGER_DELEGATE_IMPL_H_
