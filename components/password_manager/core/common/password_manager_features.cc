// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

// NOTE: It is strongly recommended to use UpperCamelCase style for feature
//       names, e.g. "MyGreatFeature".
namespace features {

// Enables the editing of passwords in chrome://settings/passwords, i.e. the
// Desktop passwords settings page.
const base::Feature kEditPasswordsInDesktopSettings = {
    "EditPasswordsInDesktopSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Recovers lost passwords on Mac by deleting the ones that cannot be decrypted
// with the present encryption key from the Keychain.
const base::Feature kDeleteCorruptedPasswords = {
    "DeleteCorruptedPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const base::Feature kFillOnAccountSelect = {"fill-on-account-select",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the experiment for the password manager to only fill on account
// selection when the password form was served over HTTP.
const base::Feature kFillOnAccountSelectHttp = {
    "FillOnAccountSelectHttp", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces password generation to ask user to update the password instead of
// overwriting silently.
const base::Feature kGenerationNoOverwrites = {
    "GenerationNoOverwrites", base::FEATURE_DISABLED_BY_DEFAULT};

// Integration with Google's Password Manager for signed-in and sync users.
const base::Feature kGooglePasswordManager = {
    "google-password-manager", base::FEATURE_DISABLED_BY_DEFAULT};

// Analyses the credentials submitted by user for leak detection.
const base::Feature kLeakDetection = {"PasswordLeakDetection",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to offer manual password generation in the accessory sheet
// on Android.
const base::Feature kManualPasswordGenerationAndroid{
    "ManualPasswordGenerationAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Performs a one-off migration (with retries) from a native backend into
// logindb. Passwords are served from the new location.
const base::Feature kMigrateLinuxToLoginDB = {"migrate-linux-to-logindb",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new password form parsing mechanism for filling passwords, details in
// https://goo.gl/QodPH1
const base::Feature kNewPasswordFormParsing = {
    "new-password-form-parsing", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new password form parsing mechanism for saving passwords, details in
// https://goo.gl/QodPH1
const base::Feature kNewPasswordFormParsingForSaving = {
    "new-password-form-parsing-for-saving", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new password form parsing mechanism for saving passwords and disables
// the old parser, details in https://goo.gl/QodPH1
const base::Feature kOnlyNewParser = {"only-new-password-form-parsing",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables editing saved passwords for Android.
const base::Feature kPasswordEditingAndroid = {
    "PasswordEditingAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the ability to import passwords from Chrome's settings page.
const base::Feature kPasswordImport = {"PasswordImport",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the password manager onboarding experience is shown
// on Android.
const base::Feature kPasswordManagerOnboardingAndroid = {
    "PasswordManagerOnboardingAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Deletes entries from local database on Mac which cannot be decrypted when
// merging data with Sync.
const base::Feature kRecoverPasswordsForSyncUsers = {
    "RecoverPasswordsForSyncUsers", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the touch to fill feature for Android.
const base::Feature kTouchToFillAndroid = {"TouchToFillAndroid",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support of filling and saving on username first flow.
const base::Feature kUsernameFirstFlow = {"UsernameFirstFlow",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Field trial identifier for password generation requirements.
const char* kGenerationRequirementsFieldTrial =
    "PasswordGenerationRequirements";

// The file version number of password requirements files. If the prefix length
// changes, this version number needs to be updated.
// Default to 0 in order to get an empty requirements file.
const char* kGenerationRequirementsVersion = "version";

// Length of a hash prefix of domain names. This is used to shard domains
// across multiple files.
// Default to 0 in order to put all domain names into the same shard.
const char* kGenerationRequirementsPrefixLength = "prefix_length";

// Timeout (in milliseconds) for password requirements lookups. As this is a
// network request in the background that does not block the UI, the impact of
// high values is not strong.
// Default to 5000 ms.
const char* kGenerationRequirementsTimeout = "timeout";

}  // namespace features

}  // namespace password_manager
