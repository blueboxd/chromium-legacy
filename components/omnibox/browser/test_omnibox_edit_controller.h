// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_

#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/test_location_bar_model.h"

class TestOmniboxEditController : public OmniboxEditController {
 public:
  TestOmniboxEditController() {}
  TestOmniboxEditController(const TestOmniboxEditController&) = delete;
  TestOmniboxEditController& operator=(const TestOmniboxEditController&) =
      delete;

  // OmniboxEditController:
  TestLocationBarModel* GetLocationBarModel() override;
  const TestLocationBarModel* GetLocationBarModel() const override;

  using OmniboxEditController::destination_url;

 private:
  TestLocationBarModel location_bar_model_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TEST_OMNIBOX_EDIT_CONTROLLER_H_
