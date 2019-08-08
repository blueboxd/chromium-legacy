// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_source.h"
#include "net/base/filename_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace content {

namespace {

bool MatchByFilePath(const GURL& url, const base::FilePath file_path) {
  if (!url.SchemeIsFile())
    return false;
  base::FilePath url_file_path;
  return net::FileURLToFilePath(url, &url_file_path) &&
         file_path == url_file_path;
}

}  // namespace

BundledExchangesSource::BundledExchangesSource() {}

BundledExchangesSource::BundledExchangesSource(const base::FilePath& path)
    : file_path(path) {
  DCHECK(!file_path.empty());
}

BundledExchangesSource::BundledExchangesSource(
    const BundledExchangesSource& src) = default;

bool BundledExchangesSource::Match(const GURL& url) const {
  if (!IsValid())
    return false;

  GURL request_url = net::SimplifyUrlForRequest(url);

  if (!file_path.empty())
    return MatchByFilePath(request_url, file_path);

  NOTREACHED();
  return false;
}

bool BundledExchangesSource::IsValid() const {
  return !file_path.empty();
}

}  // namespace content
