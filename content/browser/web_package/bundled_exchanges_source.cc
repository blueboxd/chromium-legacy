// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_source.h"

namespace content {

BundledExchangesSource::BundledExchangesSource(const base::FilePath& path)
    : file_path(path) {
  DCHECK(!file_path.empty());
}

BundledExchangesSource::BundledExchangesSource(
    const BundledExchangesSource& src) = default;

}  // namespace content
