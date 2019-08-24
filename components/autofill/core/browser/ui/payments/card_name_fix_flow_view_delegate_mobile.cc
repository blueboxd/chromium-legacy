// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view_delegate_mobile.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardNameFixFlowViewDelegateMobile::CardNameFixFlowViewDelegateMobile(
    const base::string16& inferred_cardholder_name,
    base::OnceCallback<void(const base::string16&)> upload_save_card_callback)
    : inferred_cardholder_name_(inferred_cardholder_name),
      upload_save_card_callback_(std::move(upload_save_card_callback)),
      shown_(false),
      had_user_interaction_(false) {
  DCHECK(!upload_save_card_callback_.is_null());
  AutofillMetrics::LogSaveCardCardholderNamePrefilled(
      !inferred_cardholder_name_.empty());
}

CardNameFixFlowViewDelegateMobile::~CardNameFixFlowViewDelegateMobile() {
  if (shown_ && !had_user_interaction_)
    AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
        AutofillMetrics::
            CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION);
}

int CardNameFixFlowViewDelegateMobile::GetIconId() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
#else
  return 0;
#endif
}

base::string16 CardNameFixFlowViewDelegateMobile::GetTitleText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_CARDHOLDER_NAME_FIX_FLOW_HEADER);
}

base::string16 CardNameFixFlowViewDelegateMobile::GetInferredCardHolderName()
    const {
  return inferred_cardholder_name_;
}

base::string16 CardNameFixFlowViewDelegateMobile::GetSaveButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL);
}

void CardNameFixFlowViewDelegateMobile::Accept(const base::string16& name) {
  std::move(upload_save_card_callback_).Run(name);
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_ACCEPTED);
  had_user_interaction_ = true;
  AutofillMetrics::LogSaveCardCardholderNameWasEdited(
      inferred_cardholder_name_ != name);
}

void CardNameFixFlowViewDelegateMobile::Dismissed() {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_DISMISSED);
  had_user_interaction_ = true;
}

void CardNameFixFlowViewDelegateMobile::Shown() {
  AutofillMetrics::LogCardholderNameFixFlowPromptEvent(
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_SHOWN);
  shown_ = true;
}

}  // namespace autofill
