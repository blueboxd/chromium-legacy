// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_form_user_action.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/votes_uploader.h"

namespace password_manager {

class FormSaver;
class PasswordFormMetricsRecorder;
class PasswordGenerationState;
class PasswordManagerClient;
class PasswordManagerDriver;

// This class helps with filling the observed form and with saving/updating the
// stored information about it. It is aimed to replace PasswordFormManager and
// to be renamed in new Password Manager design. Details
// go/new-cpm-design-refactoring.
class NewPasswordFormManager : public PasswordFormManagerInterface,
                               public FormFetcher::Consumer {
 public:
  // TODO(crbug.com/621355): So far, |form_fetcher| can be null. In that case
  // |this| creates an instance of it itself (meant for production code). Once
  // the fetcher is shared between PasswordFormManager instances, it will be
  // required that |form_fetcher| is not null. |form_saver| is used to
  // save/update the form. |metrics_recorder| records metrics for |*this|. If
  // null a new instance will be created.
  NewPasswordFormManager(
      PasswordManagerClient* client,
      const base::WeakPtr<PasswordManagerDriver>& driver,
      const autofill::FormData& observed_form,
      FormFetcher* form_fetcher,
      std::unique_ptr<FormSaver> form_saver,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder);

  // Constructor for http authentication (aka basic authentication).
  NewPasswordFormManager(PasswordManagerClient* client,
                         PasswordStore::FormDigest observed_http_auth_digest,
                         FormFetcher* form_fetcher,
                         std::unique_ptr<FormSaver> form_saver);

  ~NewPasswordFormManager() override;

  // The upper limit on how many times Chrome will try to autofill the same
  // form.
  static constexpr int kMaxTimesAutofill = 5;

  // Compares |observed_form_| with |form| and returns true if they are the
  // same and if |driver| is the same as |driver_|.
  bool DoesManage(const autofill::FormData& form,
                  const PasswordManagerDriver* driver) const;

  // Returns whether the form identified by |form_renderer_id| and |driver|
  // is managed by this password form manager. Don't call this on iOS.
  bool DoesManageAccordingToRendererId(
      uint32_t form_renderer_id,
      const PasswordManagerDriver* driver) const;

  // Check that |submitted_form_| is equal to |form| from the user point of
  // view. It is used for detecting that a form is reappeared after navigation
  // for success detection.
  bool IsEqualToSubmittedForm(const autofill::FormData& form) const;

  // If |submitted_form| is managed by *this (i.e. DoesManage returns true for
  // |submitted_form| and |driver|) then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  // |is_gaia_with_skip_save_password_form| is true iff this is Gaia form which
  // should be skipped on saving.
  bool ProvisionallySave(const autofill::FormData& submitted_form,
                         const PasswordManagerDriver* driver,
                         bool is_gaia_with_skip_save_password_form);

  // If |submitted_form| is managed by *this then saves |submitted_form| to
  // |submitted_form_| field, sets |is_submitted| = true and returns true.
  // Otherwise returns false.
  bool ProvisionallySaveHttpAuthForm(
      const autofill::PasswordForm& submitted_form);

  bool is_submitted() { return is_submitted_; }
  void set_not_submitted() { is_submitted_ = false; }

  void set_old_parsing_result(const autofill::PasswordForm& form) {
    old_parsing_result_ = form;
  }

  // Returns true if |*this| manages http authentication.
  bool IsHttpAuth() const;

  // Returns true if |*this| manages saving with Credentials API. This class is
  // not used for filling with Credentials API.
  bool IsCredentialAPISave() const;

  // Returns scheme of the observed form or http authentication.
  autofill::PasswordForm::Scheme GetScheme() const;

  // Selects from |predictions| predictions that corresponds to
  // |observed_form_|, initiates filling and stores predictions in
  // |predictions_|.
  void ProcessServerPredictions(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

  // Sends fill data to the renderer.
  void Fill();

  // Sends fill data to the renderer to fill |observed_form|.
  void FillForm(const autofill::FormData& observed_form);

  // Sends the request to prefill the generated password or pops up an
  // additional UI in case of possible override.
  void OnGeneratedPasswordAccepted(autofill::FormData form_data,
                                   uint32_t generation_element_id,
                                   const base::string16& password);

  // PasswordFormManagerForUI:
  const GURL& GetOrigin() const override;
  const std::map<base::string16, const autofill::PasswordForm*>&
  GetBestMatches() const override;
  std::vector<const autofill::PasswordForm*> GetFederatedMatches()
      const override;
  const autofill::PasswordForm& GetPendingCredentials() const override;
  metrics_util::CredentialSourceType GetCredentialSource() override;
  PasswordFormMetricsRecorder* GetMetricsRecorder() override;
  base::span<const autofill::PasswordForm* const> GetBlacklistedMatches()
      const override;
  base::span<const InteractionsStats> GetInteractionsStats() const override;
  bool IsBlacklisted() const override;

  void Save() override;
  void Update(const autofill::PasswordForm& credentials_to_update) override;
  void UpdateUsername(const base::string16& new_username) override;
  void UpdatePasswordValue(const base::string16& new_password) override;

  void OnNopeUpdateClicked() override;
  void OnNeverClicked() override;
  void OnNoInteraction(bool is_update) override;
  void PermanentlyBlacklist() override;
  void OnPasswordsRevealed() override;

  // PasswordFormManagerInterface:
  bool IsNewLogin() const override;
  FormFetcher* GetFormFetcher() override;
  bool IsPendingCredentialsPublicSuffixMatch() const override;
  void PresaveGeneratedPassword(const autofill::PasswordForm& form) override;
  void PasswordNoLongerGenerated() override;
  bool HasGeneratedPassword() const override;
  void SetGenerationPopupWasShown(bool generation_popup_was_shown,
                                  bool is_manual_generation) override;
  void SetGenerationElement(const base::string16& generation_element) override;
  bool IsPossibleChangePasswordFormWithoutUsername() const override;
  bool IsPasswordUpdate() const override;
  std::vector<base::WeakPtr<PasswordManagerDriver>> GetDrivers() const override;
  const autofill::PasswordForm* GetSubmittedForm() const override;

#if defined(OS_IOS)
  // Presaves the form with |generated_password|. This function is called once
  // when the user accepts the generated password. The password was generated in
  // the field with identifier |generation_element|. |driver| corresponds to the
  // |form| parent frame.
  void PresaveGeneratedPassword(PasswordManagerDriver* driver,
                                const autofill::FormData& form,
                                const base::string16& generated_password,
                                const base::string16& generation_element);

  // Updates the presaved credential with the generated password when the user
  // types in field with |field_identifier|, which is in form with
  // |form_identifier| and the field value is |field_value|. Return true if
  // |*this| manages a form with name |form_identifier|.
  bool UpdateGeneratedPasswordOnUserInput(
      const base::string16& form_identifier,
      const base::string16& field_identifier,
      const base::string16& field_value);
#endif  // defined(OS_IOS)

  // Create a copy of |*this| which can be passed to the code handling
  // save-password related UI. This omits some parts of the internal data, so
  // the result is not identical to the original.
  // TODO(crbug.com/739366): Replace with translating one appropriate class into
  // another one.
  std::unique_ptr<NewPasswordFormManager> Clone();

#if defined(UNIT_TEST)
  static void set_wait_for_server_predictions_for_filling(bool value) {
    wait_for_server_predictions_for_filling_ = value;
  }

  FormSaver* form_saver() { return form_saver_.get(); }
#endif

  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder() {
    return metrics_recorder_;
  }

 protected:
  // Constructor for Credentials API.
  NewPasswordFormManager(PasswordManagerClient* client,
                         std::unique_ptr<autofill::PasswordForm> saved_form,
                         std::unique_ptr<FormFetcher> form_fetcher,
                         std::unique_ptr<FormSaver> form_saver);

  // FormFetcher::Consumer:
  void OnFetchCompleted() override;

  // Create pending credentials from |parsed_submitted_form_| and forms received
  // from the password store.
  void CreatePendingCredentials();

 private:
  // Delegating constructor.
  NewPasswordFormManager(
      PasswordManagerClient* client,
      FormFetcher* form_fetcher,
      std::unique_ptr<FormSaver> form_saver,
      scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder,
      PasswordStore::FormDigest form_digest);

  // Compares |parsed_form| with |old_parsing_result_| and records UKM metric.
  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  void RecordMetricOnCompareParsingResult(
      const autofill::PasswordForm& parsed_form);

  // Records the status of readonly fields during parsing, combined with the
  // overall success of the parsing. It reports through two different metrics,
  // depending on whether |mode| indicates parsing for saving or filling.
  void RecordMetricOnReadonly(
      FormDataParser::ReadonlyPasswordFields readonly_status,
      bool parsing_successful,
      FormDataParser::Mode mode);

  // Report the time between receiving credentials from the password store and
  // the autofill server responding to the lookup request.
  void ReportTimeBetweenStoreAndServerUMA();

  // Create pending credentials from provisionally saved form when this form
  // represents credentials that were not previosly saved.
  void CreatePendingCredentialsForNewCredentials(
      const autofill::PasswordForm& submitted_password_form,
      const base::string16& password_element);

  void SetPasswordOverridden(bool password_overridden) {
    password_overridden_ = password_overridden;
    votes_uploader_.set_password_overridden(password_overridden);
  }

  // Helper for Save in the case there is at least one match for the pending
  // credentials. This sends needed signals to the autofill server, and also
  // triggers some UMA reporting.
  void ProcessUpdate();

  // Sends fill data to the http auth popup.
  void FillHttpAuth();

  // Helper function for calling form parsing and logging results if logging is
  // active.
  std::unique_ptr<autofill::PasswordForm> ParseFormAndMakeLogging(
      const autofill::FormData& form,
      FormDataParser::Mode mode);

  void PresaveGeneratedPasswordInternal(
      const autofill::FormData& form,
      const base::string16& generated_password);

  // Calculates FillingAssistance metric for |submitted_form|. The metric is
  // recorded in case when the successful submission is detected.
  void CalculateFillingAssistanceMetric(
      const autofill::FormData& submitted_form);

  // Returns all the credentials for the origin (essentially, |best_matches_|
  // and |not_best_matches_|).
  std::vector<const autofill::PasswordForm*> GetAllMatches() const;

  // Save/update |pending_credentials_| to the password store.
  void SavePendingToStore(bool update);

  // The client which implements embedder-specific PasswordManager operations.
  PasswordManagerClient* client_;

  base::WeakPtr<PasswordManagerDriver> driver_;

  // TODO(https://crbug.com/943045): use std::variant for keeping
  // |observed_form_| and |observed_not_web_form_digest_|.
  autofill::FormData observed_form_;

  // Used for retrieving credentials in case http authentication or Credentials
  // API.
  base::Optional<PasswordStore::FormDigest> observed_not_web_form_digest_;

  // Set of nonblacklisted PasswordForms from the DB that best match the form
  // being managed by |this|, indexed by username. The PasswordForms are owned
  // by |form_fetcher_|.
  std::map<base::string16, const autofill::PasswordForm*> best_matches_;

  // Set of forms from PasswordStore that correspond to the current site and
  // that are not in |best_matches_|. They are owned by |form_fetcher_|.
  // It is leftover from the old PasswordFormManager.
  // TODO(https://crbug.com/831123): update all places where it is used with
  // saved credentials from |form_fetcher_|.
  std::vector<const autofill::PasswordForm*> not_best_matches_;

  // Set of blacklisted forms from the PasswordStore that best match the current
  // form. They are owned by |form_fetcher_|.
  std::vector<const autofill::PasswordForm*> blacklisted_matches_;

  // If the observed form gets blacklisted through |this|, the blacklist entry
  // gets stored in |new_blacklisted_| until data is potentially refreshed by
  // reading from PasswordStore again. |blacklisted_matches_| will contain
  // |new_blacklisted_.get()| in that case. The PasswordForm will usually get
  // accessed via |blacklisted_matches_|, this unique_ptr is only used to store
  // it (unlike the rest of forms being pointed to in |blacklisted_matches_|,
  // which are owned by |form_fetcher_|).
  std::unique_ptr<autofill::PasswordForm> new_blacklisted_;

  // Convenience pointer to entry in best_matches_ that is marked
  // as preferred. This is only allowed to be null if there are no best matches
  // at all, since there will always be one preferred login when there are
  // multiple matches (when first saved, a login is marked preferred).
  const autofill::PasswordForm* preferred_match_ = nullptr;

  // Takes care of recording metrics and events for |*this|.
  scoped_refptr<PasswordFormMetricsRecorder> metrics_recorder_;

  // When not null, then this is the object which |form_fetcher_| points to.
  std::unique_ptr<FormFetcher> owned_form_fetcher_;

  // FormFetcher instance which owns the login data from PasswordStore.
  FormFetcher* form_fetcher_;

  // FormSaver instance used by |this| to all tasks related to storing
  // credentials.
  const std::unique_ptr<FormSaver> form_saver_;

  VotesUploader votes_uploader_;

  // |is_submitted_| = true means that a submission of the managed form was seen
  // and then |submitted_form_| contains the submitted form.
  bool is_submitted_ = false;
  autofill::FormData submitted_form_;
  std::unique_ptr<autofill::PasswordForm> parsed_submitted_form_;

  // Stores updated credentials when the form was submitted but success is still
  // unknown. This variable contains credentials that are ready to be written
  // (saved or updated) to a password store. It is calculated based on
  // |submitted_form_| and |best_matches_|.
  autofill::PasswordForm pending_credentials_;

  // Whether |pending_credentials_| stores a credential that should be added
  // to the password store. False means it's a pure update to the existing ones.
  // TODO(crbug/831123): this value only makes sense internally. Remove public
  // dependencies on it.
  bool is_new_login_ = true;

  // Handles the user flows related to the generation.
  std::unique_ptr<PasswordGenerationState> generation_state_;

  // Whether a saved password was overridden. The flag is true when there is a
  // credential in the store that will get a new password value.
  bool password_overridden_ = false;

  // If Chrome has already autofilled a few times, it is probable that autofill
  // is triggered by programmatic changes in the page. We set a maximum number
  // of times that Chrome will autofill to avoid being stuck in an infinite
  // loop.
  int autofills_left_ = kMaxTimesAutofill;

  // True until server predictions received or waiting for them timed out.
  bool waiting_for_server_predictions_ = false;

  // Controls whether to wait or not server before filling. It is used in tests.
  static bool wait_for_server_predictions_for_filling_;

  // Used for comparison metrics.
  // TODO(https://crbug.com/831123): Remove it when the old form parsing is
  // removed.
  autofill::PasswordForm old_parsing_result_;

  // Time when stored credentials are received from the store. Used for metrics.
  base::TimeTicks received_stored_credentials_time_;

  // Used to transform FormData into PasswordForms.
  FormDataParser parser_;

  base::WeakPtrFactory<NewPasswordFormManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NewPasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_NEW_PASSWORD_FORM_MANAGER_H_
