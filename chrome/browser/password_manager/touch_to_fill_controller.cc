// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

using content::WebContents;

TouchToFillController::TouchToFillController(WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

TouchToFillController::TouchToFillController(
    base::WeakPtr<ManualFillingController> mf_controller,
    util::PassKey<TouchToFillControllerTest>)
    : mf_controller_(std::move(mf_controller)) {
  DCHECK(mf_controller_);
}

TouchToFillController::~TouchToFillController() = default;

void TouchToFillController::Show(
    base::span<const password_manager::CredentialPair> credentials,
    base::WeakPtr<password_manager::PasswordManagerDriver> driver) {
  credentials_.assign(credentials.begin(), credentials.end());
  driver_ = std::move(driver);

  autofill::AccessorySheetData::Builder builder(
      autofill::AccessoryTabType::TOUCH_TO_FILL,
      // TODO(crbug.com/957532): Update title once mocks are finalized.
      base::ASCIIToUTF16("Touch to Fill"));

  for (size_t i = 0; i < credentials_.size(); ++i) {
    const auto& credential = credentials_[i];
    if (credential.is_public_suffix_match)
      builder.AddUserInfo(credential.origin_url.spec());
    else
      builder.AddUserInfo();

    std::string field_id = base::NumberToString(i);
    builder.AppendField(credential.username, credential.username, field_id,
                        /*is_obfuscated=*/false,
                        /*is_selectable=*/true);
    builder.AppendField(credential.password, credential.password,
                        std::move(field_id),
                        /*is_obfuscated=*/true,
                        /*is_selectable=*/false);
  }

  GetManualFillingController()->RefreshSuggestions(std::move(builder).Build());
}

void TouchToFillController::OnFillingTriggered(
    const autofill::UserInfo::Field& selection) {
  if (!driver_) {
    LOG(DFATAL) << "|driver_| is not set or has been invalidated.";
    return;
  }

  size_t index = 0;
  if (!base::StringToSizeT(selection.id(), &index)) {
    LOG(DFATAL) << "Failed to convert selection.id(): " << selection.id();
    return;
  }

  if (index >= credentials_.size()) {
    LOG(DFATAL) << "Received invalid suggestion index: " << index;
    return;
  }

  const auto& credential = credentials_[index];
  password_manager::metrics_util::LogFilledCredentialIsFromAndroidApp(
      password_manager::IsValidAndroidFacetURI(credential.origin_url.spec()));
  // Invalidate |driver_| and |credentials_| to ignore future invocations of
  // OnFillingTriggered for the same credentials.
  std::exchange(driver_, nullptr)
      ->FillSuggestion(credential.username, credential.password);
  credentials_.clear();

  mf_controller_->UpdateSourceAvailability(
      ManualFillingController::FillingSource::TOUCH_TO_FILL,
      /*has_suggestions=*/false);
}

void TouchToFillController::OnOptionSelected(
    autofill::AccessoryAction selected_action) {
  // Not applicable for TouchToFillController. All user interactions should
  // result in OnFillingTriggered().
  NOTREACHED();
}

ManualFillingController* TouchToFillController::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_.get();
}
