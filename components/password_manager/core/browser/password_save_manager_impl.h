// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_

#include "components/password_manager/core/browser/password_save_manager.h"

namespace password_manager {

class PasswordGenerationManager;

enum class PendingCredentialsState { NONE, NEW_LOGIN, UPDATE, AUTOMATIC_SAVE };

class PasswordSaveManagerImpl : public PasswordSaveManager {
 public:
  PasswordSaveManagerImpl(std::unique_ptr<FormSaver> form_saver);
  ~PasswordSaveManagerImpl() override;

  // Returns a MultiStorePasswordSaveManager if the password account storage
  // feature is enabled. Returns a PasswordSaveManagerImpl otherwise.
  static std::unique_ptr<PasswordSaveManagerImpl> CreatePasswordSaveManagerImpl(
      const PasswordManagerClient* client);

  const autofill::PasswordForm* GetPendingCredentials() const override;
  const base::string16& GetGeneratedPassword() const override;
  FormSaver* GetFormSaver() const override;

  void Init(PasswordManagerClient* client,
            const FormFetcher* form_fetcher,
            scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
            VotesUploader* votes_uploader) override;

  // Create pending credentials from |parsed_submitted_form| and
  // |parsed_observed_form| and |submitted_form|.
  void CreatePendingCredentials(
      const autofill::PasswordForm& parsed_submitted_form,
      const autofill::FormData& observed_form,
      const autofill::FormData& submitted_form,
      bool is_http_auth,
      bool is_credential_api_save) override;

  void Save(const autofill::FormData& observed_form,
            const autofill::PasswordForm& parsed_submitted_form) override;

  void Update(const autofill::PasswordForm& credentials_to_update,
              const autofill::FormData& observed_form,
              const autofill::PasswordForm& parsed_submitted_form) override;

  void PermanentlyBlacklist(
      const PasswordStore::FormDigest& form_digest) override;
  void Unblacklist(const PasswordStore::FormDigest& form_digest) override;

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(autofill::PasswordForm parsed_form) override;

  // Called when user wants to start generation flow for |generated|.
  void GeneratedPasswordAccepted(
      autofill::PasswordForm parsed_form,
      base::WeakPtr<PasswordManagerDriver> driver) override;

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated() override;

  bool IsNewLogin() const override;
  bool IsPasswordUpdate() const override;
  bool HasGeneratedPassword() const override;

  std::unique_ptr<PasswordSaveManager> Clone() override;

#if defined(UNIT_TEST)
  FormSaver* GetFormSaver() { return form_saver_.get(); }
#endif

 protected:
  // Returns the form_saver to be used for generated passwords. Subclasses will
  // override this method to provide different logic for get the form saver.
  virtual FormSaver* GetFormSaverForGeneration();

  virtual void SaveInternal(
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& old_password);

  virtual void UpdateInternal(
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& old_password);

  // FormSaver instance used by |this| to all tasks related to storing
  // credentials.
  const std::unique_ptr<FormSaver> form_saver_;

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |submitted_form_| and |best_matches_|.
  autofill::PasswordForm pending_credentials_;

  PendingCredentialsState pending_credentials_state_ =
      PendingCredentialsState::NONE;

 private:
  // Create pending credentials from provisionally saved form when this form
  // represents credentials that were not previously saved.
  void CreatePendingCredentialsForNewCredentials(
      const autofill::PasswordForm& parsed_submitted_form,
      const autofill::FormData& observed_form,
      const base::string16& password_element,
      bool is_http_auth,
      bool is_credential_api_save);

  // Save/update |pending_credentials_| to the password store.
  void SavePendingToStore(const autofill::PasswordForm& parsed_submitted_form,
                          bool update);

  // Helper for Save in the case there is at least one match for the pending
  // credentials. This sends needed signals to the autofill server, and also
  // triggers some UMA reporting.
  void ProcessUpdate(const autofill::FormData& observed_form,
                     const autofill::PasswordForm& parsed_submitted_form);

  // Handles the user flows related to the generation.
  std::unique_ptr<PasswordGenerationManager> generation_manager_;

  // FormFetcher instance which owns the login data from PasswordStore.
  const FormFetcher* form_fetcher_;

  // Takes care of recording metrics and events for |*this|.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  VotesUploader* votes_uploader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_IMPL_H_
