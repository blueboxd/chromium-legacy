// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_

#include <string>

#include "base/files/file_path.h"

namespace download {
class DownloadItem;
}

class GURL;

// Native side of DownloadUtils.java.
class DownloadUtils {
 public:
  static base::FilePath GetUriStringForPath(const base::FilePath& file_path);
  static int GetAutoResumptionSizeLimit();
  static void OpenDownload(download::DownloadItem* item, int open_source);
  static std::string RemapGenericMimeType(const std::string& mime_type,
                                          const GURL& url,
                                          const std::string& file_name);
  static bool ShouldAutoOpenDownload(download::DownloadItem* item);
  static bool IsOmaDownloadDescription(const std::string& mime_type);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_UTILS_H_
