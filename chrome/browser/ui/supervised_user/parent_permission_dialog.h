// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_
#define CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class GoogleServiceAuthError;
class Profile;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

namespace internal {
struct ParentPermissionDialogViewResult;
}

// ParentPermissionDialog provides a dialog that will prompt a child user's
// parent(s) for their permission for action.  The parent(s) approve the action
// by entering their Google password, which is then verified using the Google
// Reauthentication API's child to parent delegation mode.  The prompt can only
// be shown if the user is a child.  Otherwise, the prompt will fail.
//
// Clients should provide a ParentPermissionDialog::DoneCallback to
// receive the results of the dialog.
// Example Usage:
// ParentPermissionDialog::DoneCallback callback = base::BindOnce(
//            &ParentPermissionDialogBrowserTest::OnParentPermissionDialogDone,
//            weak_ptr_factory_.GetWeakPtr()))
// ParentPermissionDialog dialog(profile, std::move(callback));
// SkBitmap icon = LoadMyIcon();
// dialog.ShowPrompt(web_contents, "Allow your child to purchase X?", icon);
//
//
// This dialog is currently used to display content relevant for a parent to
// provide permission for the installation of an extension. Using the
// ShowPromptForExtensionInstallation() method below.
//
// This class is not thread safe.
class ParentPermissionDialog : public GaiaAuthConsumer {
 public:
  enum class Result {
    kParentPermissionReceived,
    kParentPermissionCanceled,
    kParentPermissionFailed,
  };

  using DoneCallback = base::OnceCallback<void(Result result)>;

  ParentPermissionDialog(Profile* profile, DoneCallback callback);

  ParentPermissionDialog(const ParentPermissionDialog&) = delete;
  ParentPermissionDialog& operator=(const ParentPermissionDialog&) = delete;

  ~ParentPermissionDialog() override;

  // Shows the Parent Permission Dialog.
  // |message| specifies the text to be shown in the dialog.
  // |icon| specifies the icon to be displayed. It can be empty.
  void ShowPrompt(content::WebContents* web_contents,
                  const base::string16& message,
                  const SkBitmap& icon);

  // Shows the Parent Permission Dialog for the specified extension
  // installation. The dialog's message will be generated from the extension
  // itself. |fallback_icon| can be empty.  If it is set, it will be used as a
  // backup in the event that the extension's icon couldn't be loaded from the
  // extension itself. If it is empty, and the icon couldn't be loaded from the
  // extension, a default generic extension icon will be displayed.
  void ShowPromptForExtensionInstallation(
      content::WebContents* web_contents,
      const extensions::Extension* extension,
      const SkBitmap& fallback_icon);

  // Sets whether the prompt is shown again automatically after an
  // incorrect credential.  This defaults to true, and is only disabled for
  // testing.  Without this, the test will infinitely repeatedly re-prompt
  // for a password when it is incorrect.
  void set_reprompt_after_incorrect_credential(
      bool reprompt_after_incorrect_credential) {
    reprompt_after_incorrect_credential_ = reprompt_after_incorrect_credential;
  }

  static void SetFakeIdentityManagerForTesting(
      signin::IdentityManager* identity_manager);

  // Only used for testing. Returns true if an invalid credential was received.
  bool CredentialWasInvalid() const;

 private:
  void LoadParentEmailAddresses();
  void OnExtensionIconLoaded(const gfx::Image& image);
  void LoadExtensionIcon();

  // Shows prompt internally.  If |show_password_incorrect| is true, a message
  // will be displayed indicating that.
  void ShowPromptInternal(bool show_password_incorrect);

  // Called when the parent permission prompt UI finishes, but before the
  // ReAuth process starts.
  void OnParentPermissionPromptDone(
      internal::ParentPermissionDialogViewResult result);

  // Called to handle the case when a user clicks the Accept button in the
  // dialog.
  void HandleParentPermissionDialogAccepted(
      internal::ParentPermissionDialogViewResult result);

  // Given an email address of the child's parent, return the parents'
  // obfuscated gaia id.
  std::string GetParentObfuscatedGaiaID(
      const base::string16& parent_email) const;

  // Starts the Reauth-scoped OAuth access token fetch process.
  void StartReAuthAccessTokenFetch(const std::string& parent_obfuscated_gaia_id,
                                   const std::string& parent_credential);

  // Handles the result of the access token
  void OnAccessTokenFetchComplete(const std::string& parent_obfuscated_gaia_id,
                                  const std::string& parent_credential,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // Starts the Parent Reauth proof token fetch process.
  void StartParentReAuthProofTokenFetch(
      const std::string& child_access_token,
      const std::string& parent_obfuscated_gaia_id,
      const std::string& credential);
  void SendResult(Result result);

  // GaiaAuthConsumer
  void OnReAuthProofTokenSuccess(
      const std::string& reauth_proof_token) override;
  void OnReAuthProofTokenFailure(
      const GaiaAuthConsumer::ReAuthProofTokenStatus error) override;

  std::vector<base::string16> parent_permission_email_addresses_;

  std::unique_ptr<GaiaAuthFetcher> reauth_token_fetcher_;

  // Used to fetch OAuth2 access tokens.
  signin::IdentityManager* identity_manager_ = nullptr;
  std::unique_ptr<signin::AccessTokenFetcher> oauth2_access_token_fetcher_;

  Profile* const profile_;
  DoneCallback callback_;

  const extensions::Extension* extension_ = nullptr;
  gfx::ImageSkia icon_;
  base::string16 message_;
  content::WebContents* web_contents_ = nullptr;

  // If true, the prompt will be shown again after an incorrect password
  // is entered.
  bool reprompt_after_incorrect_credential_ = true;

  // Callback to call to close the underlying dialog view.
  base::OnceClosure close_dialog_view_callback_;

  bool invalid_credential_received_ = false;

  base::WeakPtrFactory<ParentPermissionDialog> weak_factory_{this};
};

// NOTE: DO NOT USE the following code directly. It is an implementation detail
// of the dialog. Instead use ParentPermissionDialog above.

namespace internal {

// Internal struct use by the view that implements the dialog to
// communicate the result status of the dialog UI itself.
struct ParentPermissionDialogViewResult {
 public:
  enum class Status {
    kAccepted,
    kCanceled,
    kUnknown,
  };

  Status status = Status::kUnknown;
  base::string16 selected_parent_permission_email;
  base::string16 parent_permission_credential;
};

}  // namespace internal

// Implemented by the platform specific ui code to actually show the dialog.
// |window| should be the window to which the dialog is modal.  It comes from
// whatever widget is associated with opening the parent permission dialog.
// Returns a closure that should be used to close the dialog view if the caller
// disappears.  If |show_parent_password_incorrect| is set to true, then the
// dialog will also display a "Password Incorrect" message.
base::OnceClosure ShowParentPermissionDialog(
    Profile* profile,
    gfx::NativeWindow window,
    const std::vector<base::string16>& parent_permission_email_addresses,
    bool show_parent_password_incorrect,
    const gfx::ImageSkia& icon,
    const base::string16& message,
    const extensions::Extension* extension,
    base::OnceCallback<void(internal::ParentPermissionDialogViewResult result)>
        view_done_callback);

// Only to be used by tests.  Sets the next status returned by the dialog
// widget.
void SetAutoConfirmParentPermissionDialogForTest(
    internal::ParentPermissionDialogViewResult::Status status);

#endif  // CHROME_BROWSER_UI_SUPERVISED_USER_PARENT_PERMISSION_DIALOG_H_
