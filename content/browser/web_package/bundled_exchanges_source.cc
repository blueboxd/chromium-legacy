// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_source.h"

#include "base/memory/ptr_util.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace content {

// static
std::unique_ptr<BundledExchangesSource>
BundledExchangesSource::CreateFromTrustedFileUrl(const GURL& url) {
  DCHECK(url.SchemeIsFile());
  base::FilePath file_path;
  if (!net::FileURLToFilePath(url, &file_path))
    return nullptr;
  return base::WrapUnique(new BundledExchangesSource(true, file_path, url));
}

std::unique_ptr<BundledExchangesSource> BundledExchangesSource::Clone() const {
  return base::WrapUnique(
      new BundledExchangesSource(is_trusted_, file_path_, url_));
}

BundledExchangesSource::BundledExchangesSource(bool is_trusted,
                                               const base::FilePath& file_path,
                                               const GURL& url)
    : is_trusted_(is_trusted), file_path_(file_path), url_(url) {}

}  // namespace content
