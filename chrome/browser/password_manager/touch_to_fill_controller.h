// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AutofillPopupController;
struct Suggestion;
}

namespace content {
class WebContents;
}

class ManualFillingController;

class TouchToFillController
    : public base::SupportsWeakPtr<TouchToFillController>,
      public content::WebContentsUserData<TouchToFillController>,
      public AccessoryController {
 public:
  // Returns a reference to the unique TouchToFillController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called. Only valid to be called if
  // TouchToFillController::AllowedForWebContents(web_contents).
  static TouchToFillController* GetOrCreate(content::WebContents* web_contents);

  // Allow injecting a custom ManualFillingController for testing.
  static std::unique_ptr<TouchToFillController> CreateForTesting(
      base::WeakPtr<ManualFillingController> mf_controller);

  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController() override;

  // Returns true if the touch to fill controller may exist for |web_contents|.
  // Otherwise it returns false.
  static bool AllowedForWebContents(content::WebContents* web_contents);

  // Instructs the controller to show the provided |suggestions| to the user.
  // Invokes AcceptSuggestion() on popup_controller once the user made a
  // selection.
  void Show(base::span<const autofill::Suggestion> suggestions,
            base::WeakPtr<autofill::AutofillPopupController> popup_controller);

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;

 private:
  friend class content::WebContentsUserData<TouchToFillController>;

  explicit TouchToFillController(content::WebContents* web_contents);

  // Constructor corresponding to CreateForTesting().
  explicit TouchToFillController(
      base::WeakPtr<ManualFillingController> mf_controller);

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization is required to break a circular
  // dependency between the constructors of the TouchToFillController and
  // ManualFillingController.
  ManualFillingController* GetManualFillingController();

  // The tab for which this class is scoped.
  content::WebContents* web_contents_ = nullptr;

  // Popup controller passed from the latest invocation of Show().
  base::WeakPtr<autofill::AutofillPopupController> popup_controller_;

  // The manual filling controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
