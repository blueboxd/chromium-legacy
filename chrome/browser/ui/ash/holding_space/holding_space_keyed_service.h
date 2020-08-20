// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/strings/string16.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}

namespace ash {

// Browser context keyed service that:
// *   Manages the temporary holding space per-profile data model.
// *   Serves as an entry point to add holding space items from Chrome.
class HoldingSpaceKeyedService : public KeyedService {
 public:
  HoldingSpaceKeyedService(content::BrowserContext* context,
                           const AccountId& account_id);
  HoldingSpaceKeyedService(const HoldingSpaceKeyedService& other) = delete;
  HoldingSpaceKeyedService& operator=(const HoldingSpaceKeyedService& other) =
      delete;
  ~HoldingSpaceKeyedService() override;

  // Adds a screenshot item backed by the provided absolute file path.
  // The path is expected to be under a mount point path recognized by the file
  // manager app (otherwise, the item will be dropped silently).
  void AddScreenshot(const base::FilePath& screenshot_path,
                     const gfx::ImageSkia& image);

  const HoldingSpaceModel* model_for_testing() const {
    return &holding_space_model_;
  }

 private:
  content::BrowserContext* const browser_context_;
  HoldingSpaceModel holding_space_model_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
