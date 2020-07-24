// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/strings/string16.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace ash {

// Browser context keyed service that:
// *   Manages the temporary holding space per-profile data model.
// *   Serves as an entry point to add holding space items from Chrome.
class HoldingSpaceKeyedService : public KeyedService {
 public:
  explicit HoldingSpaceKeyedService(content::BrowserContext* context);
  HoldingSpaceKeyedService(const HoldingSpaceKeyedService& other) = delete;
  HoldingSpaceKeyedService& operator=(const HoldingSpaceKeyedService& other) =
      delete;
  ~HoldingSpaceKeyedService() override;

  // Sets the holding space model managed by this service as the active
  // model.
  void ActivateModel();

  // Adds a text item to the service's holding space model.
  // |text|: The item's text value.
  void AddTextItem(const base::string16& text);

  const HoldingSpaceModel* model_for_testing() const {
    return &holding_space_model_;
  }

 private:
  HoldingSpaceModel holding_space_model_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
