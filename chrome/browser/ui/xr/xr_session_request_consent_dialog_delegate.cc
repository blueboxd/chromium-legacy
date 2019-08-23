// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/xr/xr_session_request_consent_dialog_delegate.h"

#include <utility>

#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/vr/metrics/consent_flow_metrics_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

namespace vr {

XrSessionRequestConsentDialogDelegate::XrSessionRequestConsentDialogDelegate(
    content::WebContents* web_contents,
    XrConsentPromptLevel consent_level,
    base::OnceCallback<void(XrConsentPromptLevel, bool)> response_callback)
    : TabModalConfirmDialogDelegate(web_contents),
      response_callback_(std::move(response_callback)),
      consent_level_(consent_level),
      url_(web_contents->GetLastCommittedURL()),
      metrics_helper_(
          ConsentFlowMetricsHelper::InitFromWebContents(web_contents)) {}

XrSessionRequestConsentDialogDelegate::
    ~XrSessionRequestConsentDialogDelegate() = default;

base::string16 XrSessionRequestConsentDialogDelegate::GetTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_XR_CONSENT_DIALOG_TITLE,
      url_formatter::FormatUrlForSecurityDisplay(
          url_, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
}

base::string16 XrSessionRequestConsentDialogDelegate::GetDialogMessage() {
  switch (consent_level_) {
    case XrConsentPromptLevel::kDefault:
      return l10n_util::GetStringUTF16(
          IDS_XR_CONSENT_DIALOG_DESCRIPTION_DEFAULT);
    case XrConsentPromptLevel::kVRFeatures:
      return l10n_util::GetStringUTF16(
          IDS_XR_CONSENT_DIALOG_DESCRIPTION_PHYSICAL_FEATURES);
    case XrConsentPromptLevel::kVRFloorPlan:
      return l10n_util::GetStringUTF16(
          IDS_XR_CONSENT_DIALOG_DESCRIPTION_FLOOR_PLAN);
    case XrConsentPromptLevel::kNone:
      NOTREACHED();
      return l10n_util::GetStringUTF16(
          IDS_XR_CONSENT_DIALOG_DESCRIPTION_DEFAULT);
  }
}

base::string16 XrSessionRequestConsentDialogDelegate::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      IDS_XR_CONSENT_DIALOG_BUTTON_ALLOW_AND_ENTER_VR);
}

base::string16 XrSessionRequestConsentDialogDelegate::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_XR_CONSENT_DIALOG_BUTTON_DENY_VR);
}

void XrSessionRequestConsentDialogDelegate::OnAccepted() {
  OnUserActionTaken(true);
  metrics_helper_->LogUserAction(ConsentDialogAction::kUserAllowed);
  metrics_helper_->LogConsentFlowDurationWhenConsentGranted();
}

void XrSessionRequestConsentDialogDelegate::OnCanceled() {
  OnUserActionTaken(false);
  metrics_helper_->LogUserAction(ConsentDialogAction::kUserDenied);
  metrics_helper_->LogConsentFlowDurationWhenConsentNotGranted();
}

void XrSessionRequestConsentDialogDelegate::OnClosed() {
  OnUserActionTaken(false);
  metrics_helper_->LogUserAction(ConsentDialogAction::kUserAbortedConsentFlow);
  metrics_helper_->LogConsentFlowDurationWhenUserAborted();
}

void XrSessionRequestConsentDialogDelegate::OnUserActionTaken(bool allow) {
  std::move(response_callback_).Run(consent_level_, allow);
  metrics_helper_->OnDialogClosedWithConsent(url_.GetAsReferrer().spec(),
                                             allow);
}

void XrSessionRequestConsentDialogDelegate::OnShowDialog() {
  metrics_helper_->OnShowDialog();
}

base::Optional<int>
XrSessionRequestConsentDialogDelegate::GetDefaultDialogButton() {
  return ui::DIALOG_BUTTON_OK;
}

base::Optional<int>
XrSessionRequestConsentDialogDelegate::GetInitiallyFocusedButton() {
  return ui::DIALOG_BUTTON_NONE;
}

}  // namespace vr
