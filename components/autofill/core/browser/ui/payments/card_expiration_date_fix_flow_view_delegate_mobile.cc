// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view_delegate_mobile.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardExpirationDateFixFlowViewDelegateMobile::
    CardExpirationDateFixFlowViewDelegateMobile(
        const CreditCard& card,
        base::OnceCallback<void(const base::string16&, const base::string16&)>
            upload_save_card_callback)
    : upload_save_card_callback_(std::move(upload_save_card_callback)),
      shown_(false),
      had_user_interaction_(false),
      card_label_(card.NetworkAndLastFourDigits()) {
  DCHECK(!upload_save_card_callback_.is_null());
}

CardExpirationDateFixFlowViewDelegateMobile::
    ~CardExpirationDateFixFlowViewDelegateMobile() {
  if (shown_ && !had_user_interaction_)
    AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
        AutofillMetrics::ExpirationDateFixFlowPromptEvent::
            EXPIRATION_DATE_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION);
}

int CardExpirationDateFixFlowViewDelegateMobile::GetIconId() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER;
#else
  return 0;
#endif
}

base::string16 CardExpirationDateFixFlowViewDelegateMobile::GetTitleText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_CARD_UPDATE_EXPIRATION_DATE_TITLE);
}

base::string16 CardExpirationDateFixFlowViewDelegateMobile::GetSaveButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FIX_FLOW_PROMPT_SAVE_CARD_LABEL);
}

void CardExpirationDateFixFlowViewDelegateMobile::Accept(
    const base::string16& month,
    const base::string16& year) {
  std::move(upload_save_card_callback_).Run(month, year);
  AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
      AutofillMetrics::ExpirationDateFixFlowPromptEvent::
          EXPIRATION_DATE_FIX_FLOW_PROMPT_ACCEPTED);
  had_user_interaction_ = true;
}

void CardExpirationDateFixFlowViewDelegateMobile::Dismissed() {
  AutofillMetrics::LogExpirationDateFixFlowPromptEvent(
      AutofillMetrics::ExpirationDateFixFlowPromptEvent::
          EXPIRATION_DATE_FIX_FLOW_PROMPT_DISMISSED);
  had_user_interaction_ = true;
}

void CardExpirationDateFixFlowViewDelegateMobile::Shown() {
  AutofillMetrics::LogExpirationDateFixFlowPromptShown();
  shown_ = true;
}

}  // namespace autofill
