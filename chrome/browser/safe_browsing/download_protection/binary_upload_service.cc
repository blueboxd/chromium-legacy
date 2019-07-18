// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"

#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

void BinaryUploadService::UploadForDeepScanning(
    BinaryUploadService::Request request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NOTREACHED();
}

}  // namespace safe_browsing
