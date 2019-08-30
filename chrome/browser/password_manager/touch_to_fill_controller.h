// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/util/type_safety/pass_key.h"
#include "chrome/browser/autofill/accessory_controller.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordManagerDriver;
struct CredentialPair;
}  // namespace password_manager

class ManualFillingController;
class TouchToFillControllerTest;

class TouchToFillController
    : public base::SupportsWeakPtr<TouchToFillController>,
      public AccessoryController {
 public:
  explicit TouchToFillController(content::WebContents* web_contents);
  // Explicitly set the ManualFillingController in unit tests.
  explicit TouchToFillController(
      base::WeakPtr<ManualFillingController> mf_controller,
      util::PassKey<TouchToFillControllerTest>);
  ~TouchToFillController() override;

  // Instructs the controller to show the provided |credentials| to the user.
  // Invokes FillSuggestion() on |driver| once the user made a selection.
  void Show(base::span<const password_manager::CredentialPair> credentials,
            base::WeakPtr<password_manager::PasswordManagerDriver> driver);

  // AccessoryController:
  void OnFillingTriggered(const autofill::UserInfo::Field& selection) override;
  void OnOptionSelected(autofill::AccessoryAction selected_action) override;

 private:
  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization is required to break a circular
  // dependency between the constructors of the TouchToFillController and
  // ManualFillingController.
  ManualFillingController* GetManualFillingController();

  // The tab for which this class is scoped.
  content::WebContents* web_contents_ = nullptr;

  // The manual filling controller object to forward client requests to.
  // TODO(crbug.com/957532): Make TouchToFillController independent of the
  // ManualFillingController.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // Credentials passed from the latest invocation of Show().
  std::vector<password_manager::CredentialPair> credentials_;

  // Driver passed from the latest invocation of Show().
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
