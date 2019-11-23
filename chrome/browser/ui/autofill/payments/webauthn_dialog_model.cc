// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model_observer.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

WebauthnDialogModel::WebauthnDialogModel() {
  state_ = kOffer;
}

WebauthnDialogModel::~WebauthnDialogModel() = default;

void WebauthnDialogModel::SetDialogState(DialogState state) {
  state_ = state;
  for (WebauthnDialogModelObserver& observer : observers_)
    observer.OnDialogStateChanged();
}

void WebauthnDialogModel::AddObserver(WebauthnDialogModelObserver* observer) {
  observers_.AddObserver(observer);
}

void WebauthnDialogModel::RemoveObserver(
    WebauthnDialogModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WebauthnDialogModel::IsActivityIndicatorVisible() const {
  return state_ == DialogState::kPending;
}

bool WebauthnDialogModel::IsBackButtonVisible() const {
  return false;
}

bool WebauthnDialogModel::IsCancelButtonVisible() const {
  return true;
}

base::string16 WebauthnDialogModel::GetCancelButtonLabel() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_CANCEL_BUTTON_LABEL_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  return base::string16();
}

bool WebauthnDialogModel::IsAcceptButtonVisible() const {
  return state_ == DialogState::kOffer || state_ == DialogState::kPending;
}

bool WebauthnDialogModel::IsAcceptButtonEnabled() const {
  return state_ != DialogState::kPending;
}

base::string16 WebauthnDialogModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_OK_BUTTON_LABEL);
}

const gfx::VectorIcon& WebauthnDialogModel::GetStepIllustration(
    ImageColorScheme color_scheme) const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return color_scheme == ImageColorScheme::kDark
                 ? kWebauthnDialogHeaderDarkIcon
                 : kWebauthnDialogHeaderIcon;
    case DialogState::kError:
      return color_scheme == ImageColorScheme::kDark ? kWebauthnErrorDarkIcon
                                                     : kWebauthnErrorIcon;
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return gfx::kNoneIcon;
}

base::string16 WebauthnDialogModel::GetStepTitle() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_TITLE_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::string16 WebauthnDialogModel::GetStepDescription() const {
  switch (state_) {
    case DialogState::kOffer:
    case DialogState::kPending:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION);
    case DialogState::kError:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WEBAUTHN_OPT_IN_DIALOG_INSTRUCTION_ERROR);
    case DialogState::kInactive:
    case DialogState::kUnknown:
      break;
  }
  NOTREACHED();
  return base::string16();
}

base::Optional<base::string16> WebauthnDialogModel::GetAdditionalDescription()
    const {
  return base::nullopt;
}

ui::MenuModel* WebauthnDialogModel::GetOtherTransportsMenuModel() {
  return nullptr;
}

}  // namespace autofill
