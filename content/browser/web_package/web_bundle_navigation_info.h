// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_NAVIGATION_INFO_H_
#define CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_NAVIGATION_INFO_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {

class WebBundleSource;
class WebBundleReader;

// A class that holds necessary information for navigation in a
// WebBundle.
class WebBundleNavigationInfo {
 public:
  WebBundleNavigationInfo(std::unique_ptr<WebBundleSource> source,
                          const GURL& target_inner_url,
                          base::WeakPtr<WebBundleReader> weak_reader);
  ~WebBundleNavigationInfo();

  const WebBundleSource& source() const;
  const GURL& target_inner_url() const;
  const base::WeakPtr<WebBundleReader>& GetReader() const;

  std::unique_ptr<WebBundleNavigationInfo> Clone() const;

 private:
  std::unique_ptr<WebBundleSource> source_;
  const GURL target_inner_url_;
  const base::WeakPtr<WebBundleReader> weak_reader_;

  DISALLOW_COPY_AND_ASSIGN(WebBundleNavigationInfo);
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_WEB_BUNDLE_NAVIGATION_INFO_H_
