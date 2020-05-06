// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_

#include <stddef.h>

#include <string>

#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace password_manager {

namespace metrics_util {

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
  AUTOMATIC_SAVE_UNSYNCED_CREDENTIALS_LOCALLY,
  AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER,
  AUTOMATIC_MOVE_TO_ACCOUNT_STORE,
  NUM_DISPLAY_DISPOSITIONS,
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

// Enum representing the different leak detection dialogs shown to the user.
// Corresponds to LeakDetectionDialogType suffix in histograms.xml.
enum class LeakDialogType {
  // The user is asked to visit the Password Checkup.
  kCheckup = 0,
  // The user is asked to change the password for the current site.
  kChange = 1,
  // The user is asked to visit the Password Checkup and change the password for
  // the current site.
  kCheckupAndChange = 2,
  kMaxValue = kCheckupAndChange,
};

// Enum recording the dismissal reason of the data breach dialog which is shown
// in case a credential is reported as leaked. Needs to stay in sync with the
// PasswordLeakDetectionDialogDismissalReason enum in enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LeakDialogDismissalReason {
  kNoDirectInteraction = 0,
  kClickedClose = 1,
  kClickedCheckPasswords = 2,
  kClickedOk = 3,
  kMaxValue = kClickedOk,
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
  // No credentials are returned in incognito mode.
  kNoneIncognito,
  kMaxValue = kNoneIncognito,
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
  ACCESS_PASSWORD_EDITED = 2,
  ACCESS_PASSWORD_COUNT
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with
// "PasswordManager.ReauthResult" in enums.xml.
// Metrics: PasswordManager.ReauthToAccessPasswordInSettings
enum class ReauthResult {
  kSuccess = 0,
  kFailure = 1,
  kSkipped = 2,
  kMaxValue = kSkipped,
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
enum class GaiaPasswordHashChange {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Password hash saved event where the account is used to sign in to Chrome
  // (syncing).
  SAVED_ON_CHROME_SIGNIN = 0,
  // Password hash saved in content area.
  SAVED_IN_CONTENT_AREA = 1,
  // Clear password hash when the account is signed out of Chrome.
  CLEARED_ON_CHROME_SIGNOUT = 2,
  // Password hash changed event where the account is used to sign in to Chrome
  // (syncing).
  CHANGED_IN_CONTENT_AREA = 3,
  // Password hash changed event where the account is not syncing.
  NOT_SYNC_PASSWORD_CHANGE = 4,
  // Password hash change event for non-GAIA enterprise accounts.
  NON_GAIA_ENTERPRISE_PASSWORD_CHANGE = 5,
  SAVED_SYNC_PASSWORD_CHANGE_COUNT = 6,
  kMaxValue = SAVED_SYNC_PASSWORD_CHANGE_COUNT,
};

enum class IsSyncPasswordHashSaved {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  NOT_SAVED = 0,
  SAVED_VIA_STRING_PREF = 1,
  SAVED_VIA_LIST_PREF = 2,
  IS_SYNC_PASSWORD_HASH_SAVED_COUNT = 3,
  kMaxValue = IS_SYNC_PASSWORD_HASH_SAVED_COUNT,
};
#endif

// Specifies the context in which the "Show all saved passwords" fallback is
// shown or accepted.
// Metrics:
// - PasswordManager.ShowAllSavedPasswordsAcceptedContext
// - PasswordManager.ShowAllSavedPasswordsShownContext
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShowAllSavedPasswordsContext {
  kNone = 0,
  // The "Show all saved passwords..." fallback is shown below a list of
  // available passwords.
  kPassword = 1,
  // Obsolete.
  kManualFallbackDeprecated = 2,
  // The "Show all saved  passwords..." fallback is shown in context menu.
  kContextMenu = 3,
  kMaxValue = kContextMenu,
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
  kDeprecatedCopiedAll = 2,
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
  kDeprecatedFailedAccessNative = 7,
  // Could not replace old database.
  kFailedReplace = 8,
  // Could not initialise the temporary encrypted database.
  kFailedInitEncrypted,
  // Could not reset th temporary encrypted database.
  kDeprecatedFailedRecreateEncrypted,
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

// Represents the state of the user wrt. sign-in and account-scoped storage.
// Used for metrics. Always keep this enum in sync with the corresponding
// histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUserState {
  // Signed-out user (and no account storage opt-in exists).
  kSignedOutUser,
  // Signed-out user, but an account storage opt-in exists.
  kSignedOutAccountStoreUser,
  // Signed-in user, not opted in to the account storage (but will save
  // passwords to the account storage by default).
  kSignedInUser,
  // Signed-in user, not opted in to the account storage, and has explicitly
  // chosen to save passwords only on the device.
  kSignedInUserSavingLocally,
  // Signed-in user, opted in to the account storage, and saving passwords to
  // the account storage.
  kSignedInAccountStoreUser,
  // Signed-in user and opted in to the account storage, but has chosen to save
  // passwords only on the device.
  kSignedInAccountStoreUserSavingLocally,
  // Syncing user.
  kSyncUser,
};
std::string GetPasswordAccountStorageUserStateHistogramSuffix(
    PasswordAccountStorageUserState user_state);

// The usage level of the account-scoped password storage. This is essentially
// a less-detailed version of PasswordAccountStorageUserState, for metrics that
// don't need the fully-detailed breakdown. Always keep this enum in sync with
// the corresponding histogram_suffixes in histograms.xml!
enum class PasswordAccountStorageUsageLevel {
  // The user is not using the account-scoped password storage. Either they're
  // not signed in, or they haven't opted in to the account storage.
  kNotUsingAccountStorage,
  // The user is signed in and has opted in to the account storage.
  kUsingAccountStorage,
  // The user has enabled Sync.
  kSyncing,
};
std::string GetPasswordAccountStorageUsageLevelHistogramSuffix(
    PasswordAccountStorageUsageLevel usage_level);

// Log the |reason| a user dismissed the password manager UI except save/update
// bubbles.
void LogGeneralUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the save password bubble.
void LogSaveUIDismissalReason(UIDismissalReason reason);

// Log the |reason| a user dismissed the update password bubble.
void LogUpdateUIDismissalReason(UIDismissalReason reason);

// Log the |type| of a leak dialog shown to the user and the |reason| why it was
// dismissed.
void LogLeakDialogTypeAndDismissalReason(LeakDialogType type,
                                         LeakDialogDismissalReason reason);

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

// Logs the result of a re-auth challenge in the password settings.
void LogPasswordSettingsReauthResult(ReauthResult result);

// Log a return value of LoginDatabase::DeleteUndecryptableLogins method.
void LogDeleteUndecryptableLoginsReturnValue(
    DeleteCorruptedPasswordsResult result);

// Log a result of removing passwords that couldn't be decrypted with the
// present encryption key on MacOS.
void LogDeleteCorruptedPasswordsResult(DeleteCorruptedPasswordsResult result);

// Log whether a saved password was generated.
void LogNewlySavedPasswordIsGenerated(
    bool value,
    PasswordAccountStorageUsageLevel account_storage_usage_level);

// Log whether the generated password was accepted or rejected for generation of
// |type| (automatic or manual).
void LogGenerationDialogChoice(
    GenerationDialogChoice choice,
    autofill::password_generation::PasswordGenerationType type);

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
// Log a save gaia password change event.
void LogGaiaPasswordHashChange(GaiaPasswordHashChange event,
                               bool is_sync_password);

// Log whether a sync password hash saved.
void LogIsSyncPasswordHashSaved(IsSyncPasswordHashSaved state,
                                bool is_under_advanced_protection);

// Log the number of Gaia password hashes saved, and the number of enterprise
// password hashes saved. Currently only called on profile start up.
void LogProtectedPasswordHashCounts(size_t gaia_hash_count,
                                    size_t enterprise_hash_count,
                                    bool does_primary_account_exists,
                                    bool is_signed_in);

#endif

}  // namespace metrics_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_UTIL_H_
