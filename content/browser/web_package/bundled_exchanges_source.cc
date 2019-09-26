// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_source.h"

#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace content {

// static
std::unique_ptr<BundledExchangesSource>
BundledExchangesSource::MaybeCreateFromTrustedFileUrl(const GURL& url) {
#if defined(OS_ANDROID)
  if (url.SchemeIs(url::kContentScheme)) {
    const base::FilePath file_path = base::FilePath(url.spec());
    return base::WrapUnique(
        new BundledExchangesSource(/*is_trusted=*/true, file_path, url));
  }
#endif
  DCHECK(url.SchemeIsFile());
  base::FilePath file_path;
  if (!net::FileURLToFilePath(url, &file_path))
    return nullptr;
  return base::WrapUnique(
      new BundledExchangesSource(/*is_trusted=*/true, file_path, url));
}

// static
std::unique_ptr<BundledExchangesSource>
BundledExchangesSource::MaybeCreateFromFileUrl(const GURL& url) {
  base::FilePath file_path;
  if (url.SchemeIsFile()) {
    if (net::FileURLToFilePath(url, &file_path)) {
      return base::WrapUnique(
          new BundledExchangesSource(/*is_trusted=*/false, file_path, url));
    }
  }
#if defined(OS_ANDROID)
  if (url.SchemeIs(url::kContentScheme)) {
    return base::WrapUnique(new BundledExchangesSource(
        /*is_trusted=*/false, base::FilePath(url.spec()), url));
  }
#endif
  return nullptr;
}

std::unique_ptr<BundledExchangesSource> BundledExchangesSource::Clone() const {
  return base::WrapUnique(
      new BundledExchangesSource(is_trusted_, file_path_, url_));
}

std::unique_ptr<base::File> BundledExchangesSource::OpenFile() const {
#if defined(OS_ANDROID)
  if (file_path_.IsContentUri()) {
    return std::make_unique<base::File>(
        base::OpenContentUriForRead(file_path_));
  }
#endif
  return std::make_unique<base::File>(
      file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

BundledExchangesSource::BundledExchangesSource(bool is_trusted,
                                               const base::FilePath& file_path,
                                               const GURL& url)
    : is_trusted_(is_trusted), file_path_(file_path), url_(url) {}

}  // namespace content
