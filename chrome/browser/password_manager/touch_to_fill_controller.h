// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
class PasswordManagerDriver;
struct CredentialPair;
}  // namespace password_manager

class TouchToFillController {
 public:
  explicit TouchToFillController(content::WebContents* web_contents);
  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController();

  // Instructs the controller to show the provided |credentials| to the user.
  // Invokes FillSuggestion() on |driver| once the user made a selection.
  void Show(base::span<const password_manager::CredentialPair> credentials,
            base::WeakPtr<password_manager::PasswordManagerDriver> driver);

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

#if defined(UNIT_TEST)
  void set_view(std::unique_ptr<TouchToFillView> view) {
    view_ = std::move(view);
  }
#endif

 private:
  // Weak pointer to the current WebContents. This is safe because the lifetime
  // of this class is tied to ChromePasswordManagerClient, which implements
  // WebContentsUserData.
  content::WebContents* web_contents_ = nullptr;

  // View used to communicate with the Android frontend. Lazily instantiated so
  // that it can be injected by tests.
  std::unique_ptr<TouchToFillView> view_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
