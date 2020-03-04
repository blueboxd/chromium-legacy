// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace autofill {

namespace {

class AutofillPopupLayoutModelTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    layout_model_ = std::make_unique<AutofillPopupLayoutModel>(false);
  }

  AutofillPopupLayoutModel* layout_model() { return layout_model_.get(); }

 private:
  std::unique_ptr<AutofillPopupLayoutModel> layout_model_;
};

}  // namespace

}  // namespace autofill
