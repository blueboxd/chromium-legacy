// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_filling.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::PasswordForm;
using autofill::PasswordFormFillData;
using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {
bool PreferredRealmIsFromAndroid(const PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(fill_data.preferred_realm)
      .IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(const PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(login.second.realm)
            .IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

bool ShouldShowInitialPasswordAccountSuggestions() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kFillOnAccountSelect);
}

void Autofill(PasswordManagerClient* client,
              PasswordManagerDriver* driver,
              const PasswordForm& form_for_autofill,
              const std::map<base::string16, const PasswordForm*>& best_matches,
              const std::vector<const PasswordForm*>& federated_matches,
              const PasswordForm& preferred_match,
              bool wait_for_username) {
  DCHECK_EQ(PasswordForm::Scheme::kHtml, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(client)) {
    logger.reset(
        new BrowserSavePasswordProgressLogger(client->GetLogManager()));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }

  PasswordFormFillData fill_data(form_for_autofill, best_matches,
                                 preferred_match, wait_for_username);
  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  UMA_HISTOGRAM_BOOLEAN(
      "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
      ContainsAndroidCredentials(fill_data));
  metrics_util::LogFilledCredentialIsFromAndroidApp(
      PreferredRealmIsFromAndroid(fill_data));
  driver->FillPasswordForm(fill_data);

  client->PasswordWasAutofilled(best_matches, form_for_autofill.origin,
                                &federated_matches);
}

void ShowInitialPasswordAccountSuggestions(
    const PasswordManagerClient& client,
    PasswordManagerDriver* driver,
    const PasswordForm& form_for_autofill,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) {
  DCHECK_EQ(PasswordForm::Scheme::kHtml, preferred_match.scheme);

  std::unique_ptr<BrowserSavePasswordProgressLogger> logger;
  if (password_manager_util::IsLoggingActive(&client)) {
    logger.reset(new BrowserSavePasswordProgressLogger(client.GetLogManager()));
    logger->LogMessage(
        Logger::
            STRING_PASSWORDMANAGER_SHOW_INITIAL_PASSWORD_ACCOUNT_SUGGESTIONS);
  }

  if (logger)
    logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
  driver->ShowInitialPasswordAccountSuggestions(PasswordFormFillData(
      form_for_autofill, best_matches, preferred_match, wait_for_username));
}

}  // namespace

LikelyFormFilling SendFillInformationToRenderer(
    PasswordManagerClient* client,
    PasswordManagerDriver* driver,
    bool is_blacklisted,
    const PasswordForm& observed_form,
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const std::vector<const PasswordForm*>& federated_matches,
    const PasswordForm* preferred_match,
    PasswordFormMetricsRecorder* metrics_recorder) {
  DCHECK(driver);
  DCHECK_EQ(PasswordForm::Scheme::kHtml, observed_form.scheme);

  const bool new_parsing_enabled =
      base::FeatureList::IsEnabled(features::kNewPasswordFormParsing);

  if (best_matches.empty()) {
    driver->InformNoSavedCredentials();
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventNoCredential);
    return LikelyFormFilling::kNoFilling;
  }
  DCHECK(preferred_match);

  // Chrome tries to avoid filling into fields where the user is asked to enter
  // a fresh password. The old condition for filling on load was: "does the
  // form lack a new-password field?" The new one is: "does the form have a
  // current-password field?" because the current-password field is what should
  // be filled. The old condition is used with the old parser, and the new
  // condition with the new one. The new one is not explicitly checked here,
  // because it is implicit in the way filling is done: if there is no current
  // password field ID, then PasswordAutofillAgent has no way to fill the
  // password anywhere.
  const bool form_good_for_filling =
      new_parsing_enabled || !observed_form.IsPossibleChangePasswordForm();

  // If the parser of the NewPasswordFormManager decides that there is no
  // current password field, no filling attempt will be made. In this case the
  // renderer won't treat this as the "first filling" and won't record metrics
  // accordingly. The browser should not do that either.
  const bool no_sign_in_form = !observed_form.HasPasswordElement();

  // Wait for the username before filling passwords in case the
  // FillOnAccountSelectHttp feature is active and the main frame is
  // insecure.
  const bool enable_foas_on_http =
      base::FeatureList::IsEnabled(features::kFillOnAccountSelectHttp) &&
      !client->IsMainFrameSecure();

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, or
  // (2) if it matched using public suffix domain matching, or
  // (3) it would result in unexpected filling in a form with new password
  //     fields.
  // (4) the current main frame origin is insecure and the FOAS on HTTP feature
  //     is active.
  using WaitForUsernameReason =
      PasswordFormMetricsRecorder::WaitForUsernameReason;
  WaitForUsernameReason wait_for_username_reason =
      WaitForUsernameReason::kDontWait;
  if (client->IsIncognito()) {
    wait_for_username_reason = WaitForUsernameReason::kIncognitoMode;
  } else if (preferred_match->is_public_suffix_match) {
    wait_for_username_reason = WaitForUsernameReason::kPublicSuffixMatch;
  } else if (!form_good_for_filling) {
    wait_for_username_reason = WaitForUsernameReason::kFormNotGoodForFilling;
  } else if (no_sign_in_form) {
    // If the parser did not find a current password element, don't fill.
    wait_for_username_reason = WaitForUsernameReason::kFormNotGoodForFilling;
  } else if (enable_foas_on_http) {
    wait_for_username_reason = WaitForUsernameReason::kFoasOnHTTP;
  } else if (autofill::IsTouchToFillEnabled()) {
    wait_for_username_reason = WaitForUsernameReason::kTouchToFill;
  }

  // Record no "FirstWaitForUsernameReason" metrics for a form that is not meant
  // for filling. The renderer won't record a "FirstFillingResult" either.
  if (!no_sign_in_form) {
    metrics_recorder->RecordFirstWaitForUsernameReason(
        wait_for_username_reason);
  }

  bool wait_for_username =
      wait_for_username_reason != WaitForUsernameReason::kDontWait;

  if (wait_for_username) {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionNone);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventBlockedOnInteraction);
  } else {
    metrics_recorder->SetManagerAction(
        PasswordFormMetricsRecorder::kManagerActionAutofilled);
    metrics_recorder->RecordFillEvent(
        PasswordFormMetricsRecorder::kManagerFillEventAutofilled);
    base::RecordAction(base::UserMetricsAction("PasswordManager_Autofilled"));
  }
  if (ShouldShowInitialPasswordAccountSuggestions()) {
    // This is for the fill-on-account-select experiment. Instead of autofilling
    // found usernames and passwords on load, this instructs the renderer to
    // return with any found password forms so a list of password account
    // suggestions can be drawn.
    ShowInitialPasswordAccountSuggestions(*client, driver, observed_form,
                                          best_matches, *preferred_match,
                                          wait_for_username);
    return LikelyFormFilling::kShowInitialAccountSuggestions;
  }

  // If fill-on-account-select is not enabled, continue with autofilling any
  // password forms as traditionally has been done.
  Autofill(client, driver, observed_form, best_matches, federated_matches,
           *preferred_match, wait_for_username);
  return wait_for_username ? LikelyFormFilling::kFillOnAccountSelect
                           : LikelyFormFilling::kFillOnPageLoad;
}

}  // namespace password_manager
