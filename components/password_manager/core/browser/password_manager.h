// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/form_submission_observer.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {
struct FormData;
class FormStructure;
}

namespace password_manager {

class BrowserSavePasswordProgressLogger;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordFormManager;
class NewPasswordFormManager;

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the PasswordStore. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public FormSubmissionObserver {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void RegisterLocalPrefs(PrefRegistrySimple* registry);

  explicit PasswordManager(PasswordManagerClient* client);
  ~PasswordManager() override;

  void GenerationAvailableForForm(const autofill::PasswordForm& form);

  // Notifies the renderer to start the generation flow or pops up additional UI
  // in case there is a danger to overwrite an existing password.
  void OnGeneratedPasswordAccepted(PasswordManagerDriver* driver,
                                   const autofill::FormData& form_data,
                                   uint32_t generation_element_id,
                                   const base::string16& password);

  // Presaves the form with generated password. |driver| is needed to find the
  // matched form manager.
  void OnPresaveGeneratedPassword(PasswordManagerDriver* driver,
                                  const autofill::PasswordForm& form);

  // Stops treating a password as generated. |driver| is needed to find the
  // matched form manager.
  void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver,
                                   const autofill::PasswordForm& form);

  // Update the generation element and whether generation was triggered
  // manually.
  void SetGenerationElementAndReasonForForm(
      PasswordManagerDriver* driver,
      const autofill::PasswordForm& form,
      const base::string16& generation_element,
      bool is_manually_triggered);

  // TODO(isherman): This should not be public, but is currently being used by
  // the LoginPrompt code.
  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  // |driver| is optional and if it's given it should be a driver that
  // corresponds to a frame from which |form| comes from.
  void ProvisionallySavePassword(const autofill::PasswordForm& form,
                                 const PasswordManagerDriver* driver);

  // FormSubmissionObserver:
  void DidNavigateMainFrame(bool form_may_be_submitted) override;

  // Handles password forms being parsed.
  void OnPasswordFormsParsed(PasswordManagerDriver* driver,
                             const std::vector<autofill::PasswordForm>& forms);

  // Handles password forms being rendered.
  void OnPasswordFormsRendered(
      PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading);

  // Handles a password form being submitted.
  void OnPasswordFormSubmitted(PasswordManagerDriver* driver,
                               const autofill::PasswordForm& password_form);

  // Handles a password form being submitted, assumes that submission is
  // successful and does not do any checks on success of submission.
  // For example, this is called if |password_form| was filled
  // upon in-page navigation. This often means history.pushState being
  // called from JavaScript.
  void OnPasswordFormSubmittedNoChecks(
      PasswordManagerDriver* driver,
      const autofill::PasswordForm& password_form);

  // Called when a user changed a value in a non-password field. The field is in
  // a frame corresponding to |driver| and has a renderer id |renderer_id|.
  // |value| is the current value of the field.
  void OnUserModifiedNonPasswordField(
      password_manager::PasswordManagerDriver* driver,
      int32_t renderer_id,
      const base::string16& value);

  // Handles a request to show manual fallback for password saving, i.e. the
  // omnibox icon with the anchored hidden prompt.
  void ShowManualFallbackForSaving(PasswordManagerDriver* driver,
                                   const autofill::PasswordForm& password_form);

  // Handles a request to hide manual fallback for password saving.
  void HideManualFallbackForSaving();

  void ProcessAutofillPredictions(
      PasswordManagerDriver* driver,
      const std::vector<autofill::FormStructure*>& forms);

  // Causes all |pending_login_managers_| to query the password store again.
  // Results in updating the fill information on the page.
  void UpdateFormManagers();

  // Cleans the state by removing all the PasswordFormManager instances and
  // visible forms.
  void DropFormManagers();

  // Returns true if password element is detected on the current page.
  bool IsPasswordFieldDetectedOnPage();

  PasswordManagerClient* client() { return client_; }

#if defined(UNIT_TEST)
  // TODO(crbug.com/639786): Replace using this by quering the factory for
  // mocked PasswordFormManagers.
  const std::vector<std::unique_ptr<PasswordFormManager>>&
  pending_login_managers() const {
    return pending_login_managers_;
  }

  const PasswordFormManager* provisional_save_manager() const {
    return provisional_save_manager_.get();
  }

  const std::vector<std::unique_ptr<NewPasswordFormManager>>& form_managers()
      const {
    return form_managers_;
  }

  PasswordFormManagerInterface* GetSubmittedManagerForTest() const {
    return GetSubmittedManager();
  }

#endif

  // Reports the priority of a PasswordGenerationRequirementsSpec for a
  // generated password. See
  // PasswordFormMetricsRecorder::ReportSpecPriorityForGeneratedPassword.
  void ReportSpecPriorityForGeneratedPassword(
      const autofill::PasswordForm& password_form,
      uint32_t spec_priority);

  // Reports the success from the renderer's PasswordAutofillAgent to fill
  // credentials into a site. This may be called multiple times, but only
  // the first result will be recorded for each PasswordFormManager.
  void LogFirstFillingResult(PasswordManagerDriver* driver,
                             uint32_t form_renderer_id,
                             int32_t result);

  // Notifies that Credential Management API function store() is called.
  void NotifyStorePasswordCalled();

#if defined(OS_IOS)
  // TODO(https://crbug.com/866444): Use these methods instead olds ones when
  // the old parser is gone.

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
  // |form_identifier| and the field value is |field_value|. |driver|
  // corresponds to the form parent frame.
  void UpdateGeneratedPasswordOnUserInput(
      const base::string16& form_identifier,
      const base::string16& field_identifier,
      const base::string16& field_value);

  // Stops treating a password as generated. |driver| corresponds to the
  // form parent frame.
  void OnPasswordNoLongerGenerated(PasswordManagerDriver* driver);

#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(
      PasswordManagerTest,
      ShouldBlockPasswordForSameOriginButDifferentSchemeTest);

  // Clones |matched_manager| and keeps it as |provisional_save_manager_|.
  // |form| is saved provisionally to |provisional_save_manager_|.
  void ProvisionallySaveManager(const autofill::PasswordForm& form,
                                PasswordFormManager* matched_manager,
                                BrowserSavePasswordProgressLogger* logger);

  // Returns true if there is a form manager for a submitted form and this form
  // manager contains the submitted credentials suitable for automatic save
  // prompt, not for manual fallback only.
  bool IsAutomaticSavePromptAvailable();

  // Returns true if there already exists a provisionally saved password form
  // from the origin |origin|, but with a different and secure scheme.
  // This prevents a potential attack where users can be tricked into saving
  // unwanted credentials, see http://crbug.com/571580 and [1] for details.
  //
  // [1] docs.google.com/document/d/1ei3PcUNMdgmSKaWSb-A4KhowLXaBMFxDdt5hvU_0YY8
  bool ShouldBlockPasswordForSameOriginButDifferentScheme(
      const GURL& origin) const;

  // Called when the login was deemed successful. It handles the special case
  // when the provisionally saved password is a sync credential, and otherwise
  // asks the user about saving the password or saves it directly, as
  // appropriate.
  void OnLoginSuccessful();

  // Helper function called inside OnLoginSuccessful() to save password hash
  // data from |submitted_manager| for password reuse detection purpose.
  void MaybeSavePasswordHash(
      const PasswordFormManagerInterface& submitted_manager);

  // Checks for every form in |forms| whether |pending_login_managers_| already
  // contain a manager for that form. If not, adds a manager for each such form.
  void CreatePendingLoginManagers(
      PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& forms);

  // Checks for every form in |forms| whether |form_managers_| already contain a
  // manager for that form. If not, adds a manager for each such form.
  void CreateFormManagers(PasswordManagerDriver* driver,
                          const std::vector<autofill::PasswordForm>& forms);

  // Create NewPasswordFormManager for |form|, adds the newly created one to
  // |form_managers_| and returns it.
  NewPasswordFormManager* CreateFormManager(PasswordManagerDriver* driver,
                                            const autofill::FormData& forms);

  // Passes |form| to NewPasswordFormManager that manages it for using it after
  // detecting submission success for saving. |driver| is needed to determine
  // the match. If the function is called multiple times, only the form from the
  // last call is provisionally saved. Multiple calls is possible because it is
  // called on any user keystroke. If there is no NewPasswordFormManager that
  // manages |form|, the new one is created. If |is_manual_fallback| is true
  // and the matched form manager has not recieved yet response from the
  // password store, then nullptr is returned. Returns manager which manages
  // |form|.
  // |is_gaia_with_skip_save_password_form| is true iff this is Gaia form which
  // should be skipped on saving.
  // TODO(https://crbug.com/949519): move |is_gaia_with_skip_save_password_form|
  // from PasswordForm to FormData, and remove it from arguments.
  NewPasswordFormManager* ProvisionallySaveForm(const autofill::FormData& form,
                                                PasswordManagerDriver* driver,
                                                bool is_manual_fallback);

  // Returns the best match in |pending_login_managers_| for |form|. May return
  // nullptr if no match exists.
  PasswordFormManager* GetMatchingPendingManager(
      const autofill::PasswordForm& form);

  // Returns the form manager that corresponds to the submitted form. It might
  // be nullptr if there is no submitted form.
  // TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
  // gone.
  PasswordFormManagerInterface* GetSubmittedManager() const;

  // Returns the form manager that corresponds to the submitted form. It also
  // sets |submitted_form_manager_| to nullptr.
  // TODO(https://crbug.com/831123): Remove when the old PasswordFormManager is
  // gone.
  std::unique_ptr<PasswordFormManagerForUI> MoveOwnedSubmittedManager();

  // Records provisional save failure using current |client_| and
  // |main_frame_url_|.
  void RecordProvisionalSaveFailure(
      PasswordManagerMetricsRecorder::ProvisionalSaveFailure failure,
      const GURL& form_origin,
      BrowserSavePasswordProgressLogger* logger);

  scoped_refptr<PasswordFormMetricsRecorder>
  GetMetricRecorderFromNewPasswordFormManager(
      const autofill::FormData& form,
      const PasswordManagerDriver* driver);

  // Returns the manager which manages |form|. |driver| is needed to determine
  // the match. Returns nullptr when no matched manager is found.
  PasswordFormManagerInterface* GetMatchedManager(
      const PasswordManagerDriver* driver,
      const autofill::PasswordForm& form);

  // Returns the manager which manages |form|. |driver| is needed to determine
  // the match. Returns nullptr when no matched manager is found.
  NewPasswordFormManager* GetMatchedManager(const PasswordManagerDriver* driver,
                                            const autofill::FormData& form);

  // Log a frame (main frame, iframe) of a submitted password form.
  void ReportSubmittedFormFrameMetric(const PasswordManagerDriver* driver,
                                      const autofill::PasswordForm& form);

  // Note about how a PasswordFormManager can transition from
  // pending_login_managers_ to provisional_save_manager_ and the infobar.
  //
  // 1. form "seen"
  //       |                                             new
  //       |                                               ___ Infobar
  // pending_login -- form submit --> provisional_save ___/
  //             ^                            |           \___ (update DB)
  //             |                           fail
  //             |-----------<------<---------|          !new
  //
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.

  std::vector<std::unique_ptr<PasswordFormManager>> pending_login_managers_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  std::unique_ptr<PasswordFormManager> provisional_save_manager_;

  // NewPasswordFormManager transition schemes:
  // 1. HTML submission with navigation afterwads.
  // form "seen"
  //      |
  // form_managers -- submit --> (is_submitted = true) -- navigation --
  //
  //                                                          __ Prompt.
  // owned_submitted_form_manager --> (successful submission) __ Automatic save.
  //
  // 2.Other submssions detection types (XHR, frame detached, in-page
  // navigation etc) without navigation.
  // form "seen"
  //      |
  // form_managers -- successful submission (success is detected in the Render)
  //                            ____ Prompt.
  //  --> (is_submitted = true) ---- Automatic save.

  // Contains one NewPasswordFormManager per each form on the page.
  // When a form is "seen" on a page, a NewPasswordFormManager is created
  // and stored in this collection until user navigates away from page.
  std::vector<std::unique_ptr<NewPasswordFormManager>> form_managers_;

  // Corresponds to the submitted form, after navigion away before submission
  // success detection is finished.
  std::unique_ptr<NewPasswordFormManager> owned_submitted_form_manager_;

  // The embedder-level client. Must outlive this class.
  PasswordManagerClient* const client_;

  // Records all visible forms seen during a page load, in all frames of the
  // page. When the page stops loading, the password manager checks if one of
  // the recorded forms matches the login form from the previous page
  // (to see if the login was a failure), and clears the vector.
  std::vector<autofill::PasswordForm> all_visible_forms_;

  // Server predictions for the forms on the page.
  std::map<autofill::FormSignature, FormPredictions> predictions_;

  // The user-visible URL from the last time a password was provisionally saved.
  GURL main_frame_url_;

  // True if Credential Management API function store() was called. In this case
  // PasswordManager does not need to show a save/update prompt since
  // CredentialManagerImpl takes care of it.
  bool store_password_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
