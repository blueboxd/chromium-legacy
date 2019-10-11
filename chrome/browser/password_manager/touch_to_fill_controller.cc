// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

using password_manager::CredentialPair;
using password_manager::PasswordManagerDriver;

TouchToFillController::TouchToFillController(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

TouchToFillController::~TouchToFillController() = default;

void TouchToFillController::Show(base::span<const CredentialPair> credentials,
                                 base::WeakPtr<PasswordManagerDriver> driver) {
  DCHECK(!driver_ || driver_.get() == driver.get());
  driver_ = std::move(driver);

  if (!view_)
    view_ = TouchToFillViewFactory::Create(this);

  const GURL& url = driver_->GetLastCommittedURL();
  view_->Show(url_formatter::FormatUrlForSecurityDisplay(
                  url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS),
              TouchToFillView::IsOriginSecure(
                  network::IsUrlPotentiallyTrustworthy(url)),
              credentials);
}

void TouchToFillController::OnCredentialSelected(
    const CredentialPair& credential) {
  if (!driver_)
    return;

  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      password_manager::IsValidAndroidFacetURI(credential.origin_url.spec()));
  driver_->FillSuggestion(credential.username, credential.password);
  std::exchange(driver_, nullptr)->TouchToFillDismissed();
}

void TouchToFillController::OnDismiss() {
  if (!driver_)
    return;

  std::exchange(driver_, nullptr)->TouchToFillDismissed();
}

gfx::NativeView TouchToFillController::GetNativeView() {
  return web_contents_->GetNativeView();
}
