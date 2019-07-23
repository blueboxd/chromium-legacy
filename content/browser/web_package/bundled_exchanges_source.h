// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// A struct to abstract required information to access a BundledExchanges.
struct CONTENT_EXPORT BundledExchangesSource {
  explicit BundledExchangesSource(const base::FilePath& file_path);
  explicit BundledExchangesSource(const BundledExchangesSource& src);

  const base::FilePath file_path;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
