// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/local_state_mixin.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace chromeos {

namespace {

class TestMainExtraPart : public ChromeBrowserMainExtraParts {
 public:
  explicit TestMainExtraPart(LocalStateMixin::Delegate* delegate)
      : delegate_(delegate) {}
  ~TestMainExtraPart() override = default;

  // ChromeBrowserMainExtraParts:
  void PostEarlyInitialization() override { delegate_->SetUpLocalState(); }

 private:
  LocalStateMixin::Delegate* const delegate_;
};

}  // namespace

LocalStateMixin::LocalStateMixin(InProcessBrowserTestMixinHost* host,
                                 Delegate* delegate)
    : InProcessBrowserTestMixin(host), delegate_(delegate) {}

LocalStateMixin::~LocalStateMixin() = default;

void LocalStateMixin::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  // |browser_main_parts| take ownership of TestUserRegistrationMainExtra.
  static_cast<ChromeBrowserMainParts*>(browser_main_parts)
      ->AddParts(new TestMainExtraPart(delegate_));
}

}  // namespace chromeos
