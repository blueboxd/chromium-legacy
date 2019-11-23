// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_store.h"

namespace autofill {
struct FormData;
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

class PasswordManagerClient;
class FormFetcher;
class VotesUploader;
class FormSaver;
class PasswordFormMetricsRecorder;
class PasswordManagerDriver;

// Implementations of this interface should encapsulate the password Save/Update
// logic. One implementation of this class will provide the Save/Update logic in
// case of multiple password stores. This ensures that the PasswordFormManager
// stays agnostic to whether one password store or multiple password stores are
// active. While FormSaver abstracts the implementation of different
// operations (e.g. Save()), PasswordSaveManager is responsible for deciding
// what and where to Save().
class PasswordSaveManager {
 public:
  PasswordSaveManager() = default;
  virtual ~PasswordSaveManager() = default;

  virtual void Init(PasswordManagerClient* client,
                    const FormFetcher* form_fetcher,
                    scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
                    VotesUploader* votes_uploader) = 0;

  virtual const autofill::PasswordForm* GetPendingCredentials() const = 0;

  virtual const base::string16& GetGeneratedPassword() const = 0;

  virtual FormSaver* GetFormSaver() const = 0;

  // Create pending credentials from |parsed_submitted_form| and
  // |parsed_observed_form| and |submitted_form|.
  virtual void CreatePendingCredentials(
      const autofill::PasswordForm& parsed_submitted_form,
      const autofill::FormData& observed_form,
      const autofill::FormData& submitted_form,
      bool is_http_auth,
      bool is_credential_api_save) = 0;

  virtual void Save(const autofill::FormData& observed_form,
                    const autofill::PasswordForm& parsed_submitted_form) = 0;

  virtual void Update(const autofill::PasswordForm& credentials_to_update,
                      const autofill::FormData& observed_form,
                      const autofill::PasswordForm& parsed_submitted_form) = 0;

  virtual void PermanentlyBlacklist(
      const PasswordStore::FormDigest& form_digest) = 0;
  virtual void Unblacklist(const PasswordStore::FormDigest& form_digest) = 0;

  // Called when generated password is accepted or changed by user.
  virtual void PresaveGeneratedPassword(autofill::PasswordForm parsed_form) = 0;

  // Called when user wants to start generation flow for |generated|.
  virtual void GeneratedPasswordAccepted(
      autofill::PasswordForm parsed_form,
      base::WeakPtr<PasswordManagerDriver> driver) = 0;

  // Signals that the user cancels password generation.
  virtual void PasswordNoLongerGenerated() = 0;

  virtual bool IsNewLogin() const = 0;
  virtual bool IsPasswordUpdate() const = 0;
  virtual bool HasGeneratedPassword() const = 0;

  virtual std::unique_ptr<PasswordSaveManager> Clone() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordSaveManager);
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SAVE_MANAGER_H_
