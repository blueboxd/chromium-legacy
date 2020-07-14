// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_

#include <memory>
#include <vector>

#include "chrome/browser/sharesheet/sharesheet_action_cache.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace views {
class View;
}

namespace sharesheet {

class SharesheetServiceDelegate;

// The SharesheetService is the root service that provides a sharesheet for
// Chrome desktop.
class SharesheetService : public KeyedService {
 public:
  explicit SharesheetService(Profile* profile);
  ~SharesheetService() override;

  SharesheetService(const SharesheetService&) = delete;
  SharesheetService& operator=(const SharesheetService&) = delete;

  void ShowBubble(views::View* bubble_anchor_view);
  void OnBubbleClosed(uint32_t id);

 private:
  uint32_t delegate_counter_ = 0;
  std::unique_ptr<SharesheetActionCache> sharesheet_action_cache_;

  // Record of all active SharesheetServiceDelegates. These can be retrieved
  // by ShareActions and used as SharesheetControllers to make bubble changes.
  std::vector<std::unique_ptr<SharesheetServiceDelegate>> active_delegates_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
