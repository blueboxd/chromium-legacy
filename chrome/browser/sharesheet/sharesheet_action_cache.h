// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_

namespace sharesheet {

// The SharesheetActionCache facilitates communication between ShareActions
// and the SharesheetService.
class SharesheetActionCache {
 public:
  SharesheetActionCache();
  ~SharesheetActionCache();

  SharesheetActionCache(const SharesheetActionCache&) = delete;
  SharesheetActionCache& operator=(const SharesheetActionCache&) = delete;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_
