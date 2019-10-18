// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_
#define CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

// Helper class to read a data url from a BlobDataHandle on IO thread.
class CONTENT_EXPORT DataURLBlobReader {
 public:
  using ReadCompletionCallback = base::OnceCallback<void(GURL)>;

  // Read the data URL from |blob_data_handle|, and call
  // |read_completion_callback| once it completes. If the data URL cannot be
  // retrieved, |read_completion_callback| will be called with an empty URL.
  // This method must be called on the IO thread.
  static void ReadDataURLFromBlob(
      std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
      ReadCompletionCallback read_completion_callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DATA_URL_BLOB_READER_H_
