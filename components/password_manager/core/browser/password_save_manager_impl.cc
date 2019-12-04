// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_save_manager_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/multi_store_password_save_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::ValueElementPair;

namespace password_manager {

namespace {

ValueElementPair PasswordToSave(const PasswordForm& form) {
  if (form.new_password_value.empty()) {
    DCHECK(!form.password_value.empty() || form.IsFederatedCredential());
    return {form.password_value, form.password_element};
  }
  return {form.new_password_value, form.new_password_element};
}

// Helper to get the platform specific identifier by which autofill and password
// manager refer to a field. See http://crbug.com/896594
base::string16 GetPlatformSpecificIdentifier(const FormFieldData& field) {
#if defined(OS_IOS)
  return field.unique_id;
#else
  return field.name;
#endif
}

// Copies field properties masks from the form |from| to the form |to|.
void CopyFieldPropertiesMasks(const FormData& from, FormData* to) {
  // Skip copying if the number of fields is different.
  if (from.fields.size() != to->fields.size())
    return;

  for (size_t i = 0; i < from.fields.size(); ++i) {
    to->fields[i].properties_mask =
        GetPlatformSpecificIdentifier(to->fields[i]) ==
                GetPlatformSpecificIdentifier(from.fields[i])
            ? from.fields[i].properties_mask
            : autofill::FieldPropertiesFlags::ERROR_OCCURRED;
  }
}

// Filter sensitive information, duplicates and |username_value| out from
// |form->all_possible_usernames|.
void SanitizePossibleUsernames(PasswordForm* form) {
  auto& usernames = form->all_possible_usernames;

  // Deduplicate.
  std::sort(usernames.begin(), usernames.end());
  usernames.erase(std::unique(usernames.begin(), usernames.end()),
                  usernames.end());

  // Filter out |form->username_value| and sensitive information.
  const base::string16& username_value = form->username_value;
  base::EraseIf(usernames, [&username_value](const ValueElementPair& pair) {
    return pair.first == username_value ||
           autofill::IsValidCreditCardNumber(pair.first) ||
           autofill::IsSSN(pair.first);
  });
}

}  // namespace

// static
std::unique_ptr<PasswordSaveManagerImpl>
PasswordSaveManagerImpl::CreatePasswordSaveManagerImpl(
    const PasswordManagerClient* client) {
  auto profile_form_saver =
      std::make_unique<FormSaverImpl>(client->GetProfilePasswordStore());

  return base::FeatureList::IsEnabled(
             password_manager::features::kEnablePasswordsAccountStorage)
             ? std::make_unique<MultiStorePasswordSaveManager>(
                   std::move(profile_form_saver),
                   std::make_unique<FormSaverImpl>(
                       client->GetAccountPasswordStore()))
             : std::make_unique<PasswordSaveManagerImpl>(
                   std::move(profile_form_saver));
}

PasswordSaveManagerImpl::PasswordSaveManagerImpl(
    std::unique_ptr<FormSaver> form_saver)
    : form_saver_(std::move(form_saver)) {}

PasswordSaveManagerImpl::~PasswordSaveManagerImpl() = default;

const PasswordForm* PasswordSaveManagerImpl::GetPendingCredentials() const {
  return &pending_credentials_;
}

const base::string16& PasswordSaveManagerImpl::GetGeneratedPassword() const {
  DCHECK(generation_manager_);
  return generation_manager_->generated_password();
}

FormSaver* PasswordSaveManagerImpl::GetFormSaver() const {
  return form_saver_.get();
}

void PasswordSaveManagerImpl::Init(
    PasswordManagerClient* client,
    const FormFetcher* form_fetcher,
    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
    VotesUploader* votes_uploader) {
  client_ = client;
  form_fetcher_ = form_fetcher;
  metrics_recorder_ = metrics_recorder;
  votes_uploader_ = votes_uploader;
}

void PasswordSaveManagerImpl::CreatePendingCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData& observed_form,
    const FormData& submitted_form,
    bool is_http_auth,
    bool is_credential_api_save) {
  DCHECK(votes_uploader_);

  metrics_recorder_->CalculateUserAction(form_fetcher_->GetBestMatches(),
                                         parsed_submitted_form);

  // This function might be called multiple times so set variables that are
  // changed in this function to initial states.
  pending_credentials_state_ = PendingCredentialsState::NONE;
  votes_uploader_->set_password_overridden(false);

  ValueElementPair password_to_save(PasswordToSave(parsed_submitted_form));
  // Look for the actually submitted credentials in the list of previously saved
  // credentials that were available to autofilling.
  const PasswordForm* saved_form = password_manager_util::GetMatchForUpdating(
      parsed_submitted_form, form_fetcher_->GetBestMatches());
  if (saved_form) {
    // A similar credential exists in the store already.
    pending_credentials_ = *saved_form;
    if (pending_credentials_.password_value != password_to_save.first) {
      pending_credentials_state_ = PendingCredentialsState::UPDATE;
      votes_uploader_->set_password_overridden(true);
    } else if (pending_credentials_.is_public_suffix_match) {
      // If the autofilled credentials were a PSL match, store a copy with the
      // current origin and signon realm. This ensures that on the next visit, a
      // precise match is found.
      pending_credentials_state_ = PendingCredentialsState::AUTOMATIC_SAVE;
      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

      // Update |pending_credentials_| in order to be able correctly save it.
      pending_credentials_.origin = parsed_submitted_form.origin;
      pending_credentials_.signon_realm = parsed_submitted_form.signon_realm;
      pending_credentials_.action = parsed_submitted_form.action;
    }
  } else {
    pending_credentials_state_ = PendingCredentialsState::NEW_LOGIN;
    // No stored credentials can be matched to the submitted form. Offer to
    // save new credentials.
    CreatePendingCredentialsForNewCredentials(
        parsed_submitted_form, observed_form, password_to_save.second,
        is_http_auth, is_credential_api_save);
    // Generate username correction votes.
    bool username_correction_found =
        votes_uploader_->FindCorrectedUsernameElement(
            form_fetcher_->GetAllRelevantMatches(),
            parsed_submitted_form.username_value,
            parsed_submitted_form.password_value);
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.UsernameCorrectionFound",
                          username_correction_found);
    if (username_correction_found) {
      metrics_recorder_->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kCorrectedUsernameInForm);
    }
  }
  pending_credentials_.password_value =
      HasGeneratedPassword() ? generation_manager_->generated_password()
                             : password_to_save.first;
  pending_credentials_.preferred = true;
  pending_credentials_.date_last_used = base::Time::Now();
  pending_credentials_.form_has_autofilled_value =
      parsed_submitted_form.form_has_autofilled_value;
  pending_credentials_.all_possible_passwords =
      parsed_submitted_form.all_possible_passwords;
  CopyFieldPropertiesMasks(submitted_form, &pending_credentials_.form_data);

  // If we're dealing with an API-driven provisionally saved form, then take
  // the server provided values. We don't do this for non-API forms, as
  // those will never have those members set.
  if (parsed_submitted_form.type == PasswordForm::Type::kApi) {
    pending_credentials_.skip_zero_click =
        parsed_submitted_form.skip_zero_click;
    pending_credentials_.display_name = parsed_submitted_form.display_name;
    pending_credentials_.federation_origin =
        parsed_submitted_form.federation_origin;
    pending_credentials_.icon_url = parsed_submitted_form.icon_url;
    // It's important to override |signon_realm| for federated credentials
    // because it has format "federation://" + origin_host + "/" +
    // federation_host
    pending_credentials_.signon_realm = parsed_submitted_form.signon_realm;
  }

  if (HasGeneratedPassword())
    pending_credentials_.type = PasswordForm::Type::kGenerated;
}

void PasswordSaveManagerImpl::Save(const FormData& observed_form,
                                   const PasswordForm& parsed_submitted_form) {
  if (IsPasswordUpdate() &&
      pending_credentials_.type == PasswordForm::Type::kGenerated &&
      !HasGeneratedPassword()) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::Type::kManual;
  }

  if (IsNewLogin()) {
    metrics_util::LogNewlySavedPasswordIsGenerated(
        pending_credentials_.type == PasswordForm::Type::kGenerated);
    SanitizePossibleUsernames(&pending_credentials_);
    pending_credentials_.date_created = base::Time::Now();
    votes_uploader_->SendVotesOnSave(observed_form, parsed_submitted_form,
                                     form_fetcher_->GetBestMatches(),
                                     &pending_credentials_);
    SavePendingToStore(parsed_submitted_form, false /*update*/);
  } else {
    // It sounds wrong that we still update even if the state is NONE. We
    // should double check if this actually necessary. Currently some tests
    // depend on this behavior.
    ProcessUpdate(observed_form, parsed_submitted_form);
    SavePendingToStore(parsed_submitted_form, true /*update*/);
  }

  if (pending_credentials_.times_used == 1 &&
      pending_credentials_.type == PasswordForm::Type::kGenerated) {
    // This also includes PSL matched credentials.
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }
}

void PasswordSaveManagerImpl::Update(
    const PasswordForm& credentials_to_update,
    const FormData& observed_form,
    const PasswordForm& parsed_submitted_form) {
  base::string16 password_to_save = pending_credentials_.password_value;
  bool skip_zero_click = pending_credentials_.skip_zero_click;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.skip_zero_click = skip_zero_click;
  pending_credentials_.preferred = true;
  pending_credentials_.date_last_used = base::Time::Now();

  pending_credentials_state_ = PendingCredentialsState::UPDATE;

  ProcessUpdate(observed_form, parsed_submitted_form);
  SavePendingToStore(parsed_submitted_form, true /*update*/);
}

void PasswordSaveManagerImpl::PermanentlyBlacklist(
    const PasswordStore::FormDigest& form_digest) {
  DCHECK(!client_->IsIncognito());
  form_saver_->PermanentlyBlacklist(form_digest);
}

void PasswordSaveManagerImpl::Unblacklist(
    const PasswordStore::FormDigest& form_digest) {
  form_saver_->Unblacklist(form_digest);
}

void PasswordSaveManagerImpl::PresaveGeneratedPassword(
    PasswordForm parsed_form) {
  if (!HasGeneratedPassword()) {
    generation_manager_ = std::make_unique<PasswordGenerationManager>(client_);
    votes_uploader_->set_generated_password_changed(false);
    metrics_recorder_->SetGeneratedPasswordStatus(
        PasswordFormMetricsRecorder::GeneratedPasswordStatus::
            kPasswordAccepted);
  } else {
    // If the password is already generated and a new value to presave differs
    // from the presaved one, then mark that the generated password was
    // changed. If a user recovers the original generated password, it will be
    // recorded as a password change.
    if (generation_manager_->generated_password() !=
        parsed_form.password_value) {
      votes_uploader_->set_generated_password_changed(true);
      metrics_recorder_->SetGeneratedPasswordStatus(
          PasswordFormMetricsRecorder::GeneratedPasswordStatus::
              kPasswordEdited);
    }
  }
  votes_uploader_->set_has_generated_password(true);

  generation_manager_->PresaveGeneratedPassword(
      std::move(parsed_form), form_fetcher_->GetAllRelevantMatches(),
      GetFormSaverForGeneration());
}

void PasswordSaveManagerImpl::GeneratedPasswordAccepted(
    PasswordForm parsed_form,
    base::WeakPtr<PasswordManagerDriver> driver) {
  generation_manager_ = std::make_unique<PasswordGenerationManager>(client_);
  generation_manager_->GeneratedPasswordAccepted(std::move(parsed_form),
                                                 *form_fetcher_, driver);
}

void PasswordSaveManagerImpl::PasswordNoLongerGenerated() {
  DCHECK(generation_manager_);
  generation_manager_->PasswordNoLongerGenerated(GetFormSaverForGeneration());
  generation_manager_.reset();

  votes_uploader_->set_has_generated_password(false);
  votes_uploader_->set_generated_password_changed(false);
  metrics_recorder_->SetGeneratedPasswordStatus(
      PasswordFormMetricsRecorder::GeneratedPasswordStatus::kPasswordDeleted);
}

bool PasswordSaveManagerImpl::IsNewLogin() const {
  return pending_credentials_state_ == PendingCredentialsState::NEW_LOGIN ||
         pending_credentials_state_ == PendingCredentialsState::AUTOMATIC_SAVE;
}

bool PasswordSaveManagerImpl::IsPasswordUpdate() const {
  return pending_credentials_state_ == PendingCredentialsState::UPDATE;
}

bool PasswordSaveManagerImpl::HasGeneratedPassword() const {
  return generation_manager_ && generation_manager_->HasGeneratedPassword();
}

std::unique_ptr<PasswordSaveManager> PasswordSaveManagerImpl::Clone() {
  auto result = std::make_unique<PasswordSaveManagerImpl>(form_saver_->Clone());

  if (generation_manager_)
    result->generation_manager_ = generation_manager_->Clone();

  result->pending_credentials_ = pending_credentials_;
  result->pending_credentials_state_ = pending_credentials_state_;
  return result;
}

void PasswordSaveManagerImpl::CreatePendingCredentialsForNewCredentials(
    const PasswordForm& parsed_submitted_form,
    const FormData& observed_form,
    const base::string16& password_element,
    bool is_http_auth,
    bool is_credential_api_save) {
  if (is_http_auth || is_credential_api_save) {
    pending_credentials_ = parsed_submitted_form;
    return;
  }

  pending_credentials_ = parsed_submitted_form;
  pending_credentials_.form_data = observed_form;
  // The password value will be filled in later, remove any garbage for now.
  pending_credentials_.password_value.clear();
  // The password element should be determined earlier in |PasswordToSave|.
  pending_credentials_.password_element = password_element;
  // The new password's value and element name should be empty.
  pending_credentials_.new_password_value.clear();
  pending_credentials_.new_password_element.clear();
}

void PasswordSaveManagerImpl::SavePendingToStore(
    const PasswordForm& parsed_submitted_form,
    bool update) {
  const PasswordForm* saved_form = password_manager_util::GetMatchForUpdating(
      parsed_submitted_form, form_fetcher_->GetBestMatches());
  if ((update || IsPasswordUpdate()) &&
      !pending_credentials_.IsFederatedCredential()) {
    DCHECK(saved_form);
  }
  base::string16 old_password =
      saved_form ? saved_form->password_value : base::string16();
  if (HasGeneratedPassword()) {
    generation_manager_->CommitGeneratedPassword(
        pending_credentials_, form_fetcher_->GetAllRelevantMatches(),
        old_password, GetFormSaverForGeneration());
  } else if (update) {
    UpdateInternal(form_fetcher_->GetAllRelevantMatches(), old_password);
  } else {
    SaveInternal(form_fetcher_->GetAllRelevantMatches(), old_password);
  }
}

void PasswordSaveManagerImpl::ProcessUpdate(
    const FormData& observed_form,
    const PasswordForm& parsed_submitted_form) {
  DCHECK_EQ(FormFetcher::State::NOT_WAITING, form_fetcher_->GetState());
  DCHECK(form_fetcher_->GetPreferredMatch() ||
         pending_credentials_.IsFederatedCredential());
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(pending_credentials_.preferred);
  DCHECK(!client_->IsIncognito());

  password_manager_util::UpdateMetadataForUsage(&pending_credentials_);

  base::RecordAction(
      base::UserMetricsAction("PasswordManager_LoginFollowingAutofill"));

  // Check to see if this form is a candidate for password generation.
  // Do not send votes on change password forms, since they were already sent
  // in Update() method.
  if (!parsed_submitted_form.IsPossibleChangePasswordForm()) {
    votes_uploader_->SendVoteOnCredentialsReuse(
        observed_form, parsed_submitted_form, &pending_credentials_);
  }
  if (IsPasswordUpdate()) {
    votes_uploader_->UploadPasswordVote(
        parsed_submitted_form, parsed_submitted_form, autofill::NEW_PASSWORD,
        FormStructure(pending_credentials_.form_data).FormSignatureAsStr());
  }

  if (pending_credentials_.times_used == 1) {
    votes_uploader_->UploadFirstLoginVotes(form_fetcher_->GetBestMatches(),
                                           pending_credentials_,
                                           parsed_submitted_form);
  }
}

FormSaver* PasswordSaveManagerImpl::GetFormSaverForGeneration() {
  DCHECK(form_saver_);
  return form_saver_.get();
}

void PasswordSaveManagerImpl::SaveInternal(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  form_saver_->Save(pending_credentials_, matches, old_password);
}

void PasswordSaveManagerImpl::UpdateInternal(
    const std::vector<const PasswordForm*>& matches,
    const base::string16& old_password) {
  form_saver_->Update(pending_credentials_, matches, old_password);
}

}  // namespace password_manager
