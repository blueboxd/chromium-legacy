// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/sharesheet/sharesheet_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace apps {
class AppServiceProxy;
}

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

  void ShowBubble(views::View* bubble_anchor_view,
                  apps::mojom::IntentPtr intent);
  void OnBubbleClosed(uint32_t id, const base::string16& active_action);
  void OnTargetSelected(uint32_t delegate_id,
                        const base::string16& target_name,
                        const TargetType type,
                        apps::mojom::IntentPtr intent,
                        views::View* share_action_view);
  SharesheetServiceDelegate* GetDelegate(uint32_t delegate_id);

 private:
  uint32_t delegate_counter_ = 0;
  std::unique_ptr<SharesheetActionCache> sharesheet_action_cache_;
  apps::AppServiceProxy* app_service_proxy_;

  // Record of all active SharesheetServiceDelegates. These can be retrieved
  // by ShareActions and used as SharesheetControllers to make bubble changes.
  std::vector<std::unique_ptr<SharesheetServiceDelegate>> active_delegates_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
