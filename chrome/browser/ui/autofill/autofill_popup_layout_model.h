// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "chrome/browser/ui/autofill/popup_view_common.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class ImageSkia;
}

namespace autofill {

// Helper class which keeps tracks of popup bounds and related view information.
// TODO(mathp): investigate moving ownership of this class to the view.
class AutofillPopupLayoutModel {
 public:
  explicit AutofillPopupLayoutModel(bool is_credit_card_popup);

  ~AutofillPopupLayoutModel();

#if !defined(OS_ANDROID)
  // Returns the icon image of the given |suggestion|.
  static gfx::ImageSkia GetIconImage(const autofill::Suggestion& suggestion);

  // Returns the store indicator icon image of the given |suggestion|.
  static gfx::ImageSkia GetStoreIndicatorIconImage(
      const autofill::Suggestion& suggestion);
#endif

  bool is_credit_card_popup() const { return is_credit_card_popup_; }

  // Gets the resource value for the given resource, returning 0 if the
  // resource isn't recognized.
  static int GetIconResourceID(const std::string& resource_name);

  // Allows the provision of another implementation of view_common, for use in
  // unit tests where using the real thing could cause crashes.
  void SetUpForTesting(std::unique_ptr<PopupViewCommon> view_common);

 private:
#if !defined(OS_ANDROID)
  static gfx::ImageSkia GetIconImageByName(const std::string& icon_str);
#endif

  std::unique_ptr<PopupViewCommon> view_common_;

  const bool is_credit_card_popup_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupLayoutModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_LAYOUT_MODEL_H_
