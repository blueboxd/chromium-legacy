// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"

namespace storage {
class BlobStorageContext;
}

namespace content {

class ResourceContext;

storage::BlobStorageContext* BlobStorageContextGetter(
    ResourceContext* resource_context);

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_UTILS_H_
