// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/common/password_manager_features.h"

using content::WebContents;

// static
TouchToFillController* TouchToFillController::GetOrCreate(
    WebContents* web_contents) {
  DCHECK(web_contents) << "Need valid WebContents to attach controller to!";
  DCHECK(TouchToFillController::AllowedForWebContents(web_contents));

  TouchToFillController::CreateForWebContents(web_contents);
  return TouchToFillController::FromWebContents(web_contents);
}

// static
std::unique_ptr<TouchToFillController> TouchToFillController::CreateForTesting(
    base::WeakPtr<ManualFillingController> mf_controller) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(new TouchToFillController(mf_controller));
}

TouchToFillController::~TouchToFillController() = default;

// static
bool TouchToFillController::AllowedForWebContents(WebContents* web_contents) {
  return base::FeatureList::IsEnabled(
      password_manager::features::kTouchToFillAndroid);
}

void TouchToFillController::Show(
    const std::vector<autofill::Suggestion>& suggestions) {
  autofill::AccessorySheetData::Builder builder(
      autofill::AccessoryTabType::TOUCH_TO_FILL,
      // TODO(crbug.com/957532): Update title once mocks are finalized.
      base::ASCIIToUTF16("Touch to Fill"));
  for (const auto& suggestion : suggestions) {
    // This needs to stay in sync with how the PasswordAutofillManager creates
    // Suggestions out of PasswordFormFillData.
    const base::string16& username = suggestion.value;
    const base::string16& password = suggestion.additional_label;
    // This is only set if the credential's realm differs from the realm of the
    // form.
    const base::string16& maybe_realm = suggestion.label;

    builder.AddUserInfo().AppendSimpleField(username);
    builder.AppendField(password, password, /*is_obfuscated=*/true,
                        /*is_selectable=*/false);
    builder.AppendField(maybe_realm, maybe_realm, /*is_obfuscated=*/false,
                        /*is_selectable=*/false);
  }

  GetManualFillingController()->RefreshSuggestions(std::move(builder).Build());
}

void TouchToFillController::OnFillingTriggered(
    const autofill::UserInfo::Field& selection) {
  // TODO(crbug.com/957532): Implement this method.
  NOTIMPLEMENTED();
}

void TouchToFillController::OnOptionSelected(
    autofill::AccessoryAction selected_action) {
  // TODO(crbug.com/957532): Implement this method.
  NOTIMPLEMENTED();
}

TouchToFillController::TouchToFillController(WebContents* web_contents)
    : web_contents_(web_contents) {}

TouchToFillController::TouchToFillController(
    base::WeakPtr<ManualFillingController> mf_controller)
    : mf_controller_(std::move(mf_controller)) {
  DCHECK(mf_controller_);
}

ManualFillingController* TouchToFillController::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_.get();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TouchToFillController)
