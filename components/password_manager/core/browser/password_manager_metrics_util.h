// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace password_manager {

namespace metrics_util {

// Metrics: "PasswordManager.InfoBarResponse"
enum ResponseType {
  NO_RESPONSE = 0,
  REMEMBER_PASSWORD,
  NEVER_REMEMBER_PASSWORD,
  INFOBAR_DISMISSED,
  NUM_RESPONSE_TYPES,
};

// Metrics: "PasswordBubble.DisplayDisposition"
enum UIDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING,
  MANUAL_MANAGE_PASSWORDS,
  MANUAL_BLACKLISTED_OBSOLETE,  // obsolete.
  AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION,
  AUTOMATIC_CREDENTIAL_REQUEST_OBSOLETE,  // obsolete
  AUTOMATIC_SIGNIN_TOAST,
  MANUAL_WITH_PASSWORD_PENDING_UPDATE,
  AUTOMATIC_WITH_PASSWORD_PENDING_UPDATE,
  MANUAL_GENERATED_PASSWORD_CONFIRMATION,
  NUM_DISPLAY_DISPOSITIONS
};

// Metrics: "PasswordManager.UIDismissalReason"
enum UIDismissalReason {
  // We use this to mean both "Bubble lost focus" and "No interaction with the
  // infobar".
  NO_DIRECT_INTERACTION = 0,
  CLICKED_SAVE,
  CLICKED_CANCEL,
  CLICKED_NEVER,
  CLICKED_MANAGE,
  CLICKED_DONE_OBSOLETE,         // obsolete
  CLICKED_UNBLACKLIST_OBSOLETE,  // obsolete.
  CLICKED_OK_OBSOLETE,           // obsolete
  CLICKED_CREDENTIAL_OBSOLETE,   // obsolete.
  AUTO_SIGNIN_TOAST_TIMEOUT,
  AUTO_SIGNIN_TOAST_CLICKED_OBSOLETE,  // obsolete.
  CLICKED_BRAND_NAME_OBSOLETE,         // obsolete.
  CLICKED_PASSWORDS_DASHBOARD,
  NUM_UI_RESPONSES,
};

enum FormDeserializationStatus {
  LOGIN_DATABASE_SUCCESS,
  LOGIN_DATABASE_FAILURE,
  LIBSECRET_SUCCESS,
  LIBSECRET_FAILURE,
  GNOME_SUCCESS,
  GNOME_FAILURE,
  NUM_DESERIALIZATION_STATUSES
};

// Metrics: "PasswordManager.PasswordSyncState"
enum PasswordSyncState {
  SYNCING_OK,
  NOT_SYNCING_FAILED_READ,
  NOT_SYNCING_DUPLICATE_TAGS,
  NOT_SYNCING_SERVER_ERROR,
  NOT_SYNCING_FAILED_CLEANUP,
  NOT_SYNCING_FAILED_DECRYPTION,
  NOT_SYNCING_FAILED_ADD,
  NOT_SYNCING_FAILED_UPDATE,
  NOT_SYNCING_FAILED_METADATA_PERSISTENCE,
  NUM_SYNC_STATES
};

// Metrics: "PasswordManager.ApplySyncChangesState"
enum class ApplySyncChangesState {
  kApplyOK = 0,
  kApplyAddFailed = 1,
  kApplyUpdateFailed = 2,
  kApplyDeleteFailed = 3,
  kApplyMetadataChangesFailed = 4,

  kMaxValue = kApplyMetadataChangesFailed,
};

// Metrics: "PasswordGeneration.SubmissionEvent"
enum PasswordSubmissionEvent {
  PASSWORD_SUBMITTED,
  PASSWORD_SUBMISSION_FAILED,
  PASSWORD_NOT_SUBMITTED,
  PASSWORD_OVERRIDDEN,
  PASSWORD_USED,
  GENERATED_PASSWORD_FORCE_SAVED,
  SUBMISSION_EVENT_ENUM_COUNT
};

enum AutoSigninPromoUserAction {
  AUTO_SIGNIN_NO_ACTION,
  AUTO_SIGNIN_TURN_OFF,
  AUTO_SIGNIN_OK_GOT_IT,
  AUTO_SIGNIN_PROMO_ACTION_COUNT
};

enum AccountChooserUserAction {
  ACCOUNT_CHOOSER_DISMISSED,
  ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN,
  ACCOUNT_CHOOSER_SIGN_IN,
  ACCOUNT_CHOOSER_ACTION_COUNT
};

// Metrics: "PasswordManager.Mediation{Silent,Optional,Required}"
enum class CredentialManagerGetResult {
  // The promise is rejected.
  kRejected,
  // Auto sign-in is not allowed in the current context.
  kNoneZeroClickOff,
  // No matching credentials found.
  kNoneEmptyStore,
  // User mediation required due to > 1 matching credentials.
  kNoneManyCredentials,
  // User mediation required due to the signed out state.
  kNoneSignedOut,
  // User mediation required due to pending first run experience dialog.
  kNoneFirstRun,
  // Return empty credential for whatever reason.
  kNone,
  // Return a credential from the account chooser.
  kAccountChooser,
  // User is auto signed in.
  kAutoSignIn,
  kMaxValue = kAutoSignIn,
};

// Metrics: "PasswordManager.HttpPasswordMigrationMode"
enum HttpPasswordMigrationMode {
  HTTP_PASSWORD_MIGRATION_MODE_MOVE,
  HTTP_PASSWORD_MIGRATION_MODE_COPY,
  HTTP_PASSWORD_MIGRATION_MODE_COUNT
};

enum PasswordReusePasswordFieldDetected {
  NO_PASSWORD_FIELD,
  HAS_PASSWORD_FIELD,
  PASSWORD_REUSE_PASSWORD_FIELD_DETECTED_COUNT
};

// Recorded into a UMA histogram, so order of enumerators should not be changed.
enum class SubmittedFormFrame {
  MAIN_FRAME,
  IFRAME_WITH_SAME_URL_AS_MAIN_FRAME,
  IFRAME_WITH_DIFFERENT_URL_SAME_SIGNON_REALM_AS_MAIN_FRAME,
  IFRAME_WITH_DIFFERENT_SIGNON_REALM,
  SUBMITTED_FORM_FRAME_COUNT
};

// Metrics: "PasswordManager.AccessPasswordInSettings"
enum AccessPasswordInSettingsEvent {
  ACCESS_PASSWORD_VIEWED = 0,
  ACCESS_PASSWORD_COPIED = 1,
  ACCESS_PASSWORD_COUNT
};

// Metrics: PasswordManager.ReauthToAccessPasswordInSettings
enum ReauthToAccessPasswordInSettingsEvent {
  REAUTH_SUCCESS = 0,
  REAUTH_FAILURE = 1,
  REAUTH_SKIPPED = 2,
  REAUTH_COUNT
};

// Specifies the type of PasswordFormManagers and derived classes to distinguish
// the context in which a PasswordFormManager is being created and used.
enum class CredentialSourceType {
  kUnknown,
  // This is used for form based credential management (PasswordFormManager).
  kPasswordManager,
  // This is used for credential management API based credential management
  // (CredentialManagerPasswordFormManager).
  kCredentialManagementAPI
};

// Metrics: PasswordManager.DeleteCorruptedPasswordsResult
// Metrics: PasswordManager.DeleteUndecryptableLoginsReturnValue
// A passwords is considered corrupted if it's stored locally using lost
// encryption key.
enum class DeleteCorruptedPasswordsResult {
  // No corrupted entries were deleted.
  kSuccessNoDeletions = 0,
  // There were corrupted entries that were successfully deleted.
  kSuccessPasswordsDeleted = 1,
  // There was at least one corrupted entry that failed to be removed (it's
  // possible that other corrupted entries were deleted).
  kItemFailure = 2,
  // Encryption is unavailable, it's impossible to determine which entries are
  // corrupted.
  kEncryptionUnavailable = 3,
  kMaxValue = kEncryptionUnavailable,
};

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
enum class SyncPasswordHashChange {
  SAVED_ON_CHROME_SIGNIN,
  SAVED_IN_CONTENT_AREA,
  CLEARED_ON_CHROME_SIGNOUT,
  CHANGED_IN_CONTENT_AREA,
  NOT_SYNC_PASSWORD_CHANGE,
  SAVED_SYNC_PASSWORD_CHANGE_COUNT
};

enum class IsSyncPasswordHashSaved {
  NOT_SAVED,
  SAVED_VIA_STRING_PREF,
  SAVED_VIA_LIST_PREF,
  IS_SYNC_PASSWORD_HASH_SAVED_COUNT
};
#endif

// Specifies the context in which the "Show all saved passwords" fallback is
// shown or accepted.
// Metrics:
// - PasswordManager.ShowAllSavedPasswordsAcceptedContext
// - PasswordManager.ShowAllSavedPasswordsShownContext
enum ShowAllSavedPasswordsContext {
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_NONE,
  // The "Show all saved passwords..." fallback is shown below a list of
  // available passwords.
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_PASSWORD,
  // Obsolete.
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_MANUAL_FALLBACK_DEPRECATED,
  // The "Show all saved  passwords..." fallback is shown in context menu.
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU,
  SHOW_ALL_SAVED_PASSWORDS_CONTEXT_COUNT
};

// Metrics: "PasswordManager.CertificateErrorsWhileSeeingForms"
enum class CertificateError {
  NONE = 0,
  OTHER = 1,
  AUTHORITY_INVALID = 2,
  DATE_INVALID = 3,
  COMMON_NAME_INVALID = 4,
  WEAK_SIGNATURE_ALGORITHM = 5,
  COUNT
};

// Used in UMA histograms, please do NOT reorder.
// Metric: "PasswordManager.ReusedPasswordType".
enum class PasswordType {
  // Passwords saved by password manager.
  SAVED_PASSWORD = 0,
  // Passwords used for Chrome sign-in and is closest ("blessed") to be set to
  // sync when signed into multiple profiles if user wants to set up sync.
  // The primary account is equivalent to the "sync account" if this profile has
  // enabled sync.
  PRIMARY_ACCOUNT_PASSWORD = 1,
  // Other Gaia passwords used in Chrome other than the sync password.
  OTHER_GAIA_PASSWORD = 2,
  // Passwords captured from enterprise login page.
  ENTERPRISE_PASSWORD = 3,
  // Unknown password type. Used by downstream code to indicate there was not a
  // password reuse.
  PASSWORD_TYPE_UNKNOWN = 4,
  PASSWORD_TYPE_COUNT
};

enum class LinuxBackendMigrationStatus {
  // No migration was attempted (this value should not occur).
  kNotAttempted = 0,
  // The last attempt was not completed.
  kDeprecatedFailed = 1,
  // All the data is in the encrypted loginDB.
  kCopiedAll = 2,
  // The standard login database is encrypted.
  kLoginDBReplaced = 3,
  // The migration is about to be attempted.
  kStarted = 4,
  // No access to the native backend.
  kPostponed = 5,
  // Could not create or write into the temporary file. Deprecated and replaced
  // by more precise errors.
  kDeprecatedFailedCreatedEncrypted = 6,
  // Could not read from the native backend.
  kFailedAccessNative = 7,
  // Could not replace old database.
  kFailedReplace = 8,
  // Could not initialise the temporary encrypted database.
  kFailedInitEncrypted,
  // Could not reset th temporary encrypted database.
  kFailedRecreateEncrypted,
  // Could not add entries into the temporary encrypted database.
  kFailedWriteToEncrypted,
  kMaxValue = kFailedWriteToEncrypted
};

// Type of the password drop-down shown on focus field.
enum class PasswordDropdownState {
  // The passwords are listed and maybe the "Show all" button.
  kStandard = 0,
  // The drop down shows passwords and "Generate password" item.
  kStandardGenerate = 1,
  kMaxValue = kStandardGenerate
};

// Type of the item the user selects in the password drop-down.
enum class PasswordDropdownSelectedOption {
  // User selected a credential to fill.
  kPassword = 0,
  // User decided to open the password list.
  kShowAll = 1,
  // User selected to generate a password.
  kGenerate = 2,
  kMaxValue = kGenerate
};

// Used in UMA histograms, please do NOT reorder.
// Metric: "KeyboardAccessory.GenerationDialogChoice.{Automatic, Manual}".
enum class GenerationDialogChoice {
  // The user accepted the generated password.
  kAccepted = 0,
  // The user rejected the generated password.
  kRejected = 1,
  kMaxValue = kRejected
};

// Type of the conflict with existing credentials when starting password
// generation.
enum class GenerationPresaveConflict {
  // Credential can be presaved as is.
  kNoUsernameConflict = 0,
  // Credential can be presaved without username.
  kNoConflictWithEmptyUsername = 1,
  // Credential should overwrite one without username.
  kConflictWithEmptyUsername = 2,
  kMaxValue = kConflictWithEmptyUsername
};

// A version of the UMA_HISTOGRAM_BOOLEAN macro that allows the |name|
// to vary over the program's runtime.
void LogUMAHistogramBoolean(const std::string& name, bool sample);

// Log the |reason| a user dismissed the password manager UI except save/update
// bubbles.
void LogGeneralUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the save password bubble.
void LogSaveUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the update password bubble.
void LogUpdateUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the update password bubble when resolving a
// conflict during generation.
void LogPresavedUpdateUIDismissalReason(UIDismissalReason reason);

// Log the appropriate display disposition.
void LogUIDisplayDisposition(UIDisplayDisposition disposition);

// Log if a saved FormData was deserialized correctly.
void LogFormDataDeserializationStatus(FormDeserializationStatus status);

// When a credential was filled, log whether it came from an Android app.
void LogFilledCredentialIsFromAndroidApp(bool from_android);

// Log what's preventing passwords from syncing.
void LogPasswordSyncState(PasswordSyncState state);

// Log what's preventing passwords from applying sync changes.
void LogApplySyncChangesState(ApplySyncChangesState state);

// Log submission events related to generation.
void LogPasswordGenerationSubmissionEvent(PasswordSubmissionEvent event);

// Log when password generation is available for a particular form.
void LogPasswordGenerationAvailableSubmissionEvent(
    PasswordSubmissionEvent event);

// Log a user action on showing the autosignin first run experience.
void LogAutoSigninPromoUserAction(AutoSigninPromoUserAction action);

// Log a user action on showing the account chooser for one or many accounts.
void LogAccountChooserUserActionOneAccount(AccountChooserUserAction action);
void LogAccountChooserUserActionManyAccounts(AccountChooserUserAction action);

// Logs number of passwords migrated from HTTP to HTTPS.
void LogCountHttpMigratedPasswords(int count);

// Logs mode of HTTP password migration.
void LogHttpPasswordMigrationMode(HttpPasswordMigrationMode mode);

// Log the result of navigator.credentials.get.
void LogCredentialManagerGetResult(CredentialManagerGetResult result,
                                   CredentialMediationRequirement mediation);

// Log the password reuse.
void LogPasswordReuse(int password_length,
                      int saved_passwords,
                      int number_matches,
                      bool password_field_detected,
                      PasswordType reused_password_type);

// Log the context in which the "Show all saved passwords" fallback was shown.
void LogContextOfShowAllSavedPasswordsShown(
    ShowAllSavedPasswordsContext context);

// Log the context in which the "Show all saved passwords" fallback was
// accepted.
void LogContextOfShowAllSavedPasswordsAccepted(
    ShowAllSavedPasswordsContext context);

// Log the type of the password dropdown when it's shown.
void LogPasswordDropdownShown(PasswordDropdownState state, bool off_the_record);

// Log the type of the password dropdown suggestion when chosen.
void LogPasswordDropdownItemSelected(PasswordDropdownSelectedOption type,
                                     bool off_the_record);

// Log a password successful submission event.
void LogPasswordSuccessfulSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Log a password successful submission event for accepted by user password save
// or update.
void LogPasswordAcceptedSaveUpdateSubmissionIndicatorEvent(
    autofill::mojom::SubmissionIndicatorEvent event);

// Log a frame of a submitted password form.
void LogSubmittedFormFrame(SubmittedFormFrame frame);

// Log a return value of LoginDatabase::DeleteUndecryptableLogins method.
void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result);

// Log a result of removing passwords that couldn't be decrypted with the
// present encryption key on MacOS.
void LogDeleteCorruptedPasswordsResult(DeleteCorruptedPasswordsResult result);

// Log whether a saved password was generated.
void LogNewlySavedPasswordIsGenerated(bool value);

// Log whether the generated password was accepted or rejected for generation of
// |type| (automatic or manual).
void LogGenerationDialogChoice(
    GenerationDialogChoice choice,
    autofill::password_generation::PasswordGenerationType type);

// Log whether there is a conflict with existing credentials when presaving
// a generated password.
void LogGenerationPresaveConflict(GenerationPresaveConflict value);

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Log a save sync password change event.
void LogSyncPasswordHashChange(SyncPasswordHashChange event);

// Log whether a sync password hash saved.
void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection);

// Log the number of Gaia password hashes saved, and the number of enterprise
// password hashes saved.
void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    size_t enterprise_hash_count);

#endif

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
