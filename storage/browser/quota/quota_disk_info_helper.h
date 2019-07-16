// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_DISK_INFO_HELPER_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_DISK_INFO_HELPER_H_

namespace storage {

// Interface used by the quota system to gather disk space information.
// Can be overridden in tests.
// Subclasses must be thread-safe.
// QuotaSettings instances own a singleton instance of QuotaDiskInfoHelper.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaDiskInfoHelper {
 public:
  QuotaDiskInfoHelper() = default;
  virtual ~QuotaDiskInfoHelper();

  virtual int64_t AmountOfTotalDiskSpace(const base::FilePath& path) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaDiskInfoHelper);
};  // class QuotaDiskInfoHelper

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_DISK_INFO_HELPER_H_
