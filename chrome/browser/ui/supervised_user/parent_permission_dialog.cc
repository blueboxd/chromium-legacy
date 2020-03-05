// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace {
// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
const gfx::ImageSkia& GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  return is_app ? extensions::util::GetDefaultAppIcon()
                : extensions::util::GetDefaultExtensionIcon();
}

signin::IdentityManager* test_identity_manager = nullptr;
}  // namespace

ParentPermissionDialog::ParentPermissionDialog(Profile* profile,
                                               DoneCallback callback)
    : profile_(profile), callback_(std::move(callback)) {
  DCHECK(callback_);
  DCHECK(profile_);
  DCHECK(profile_->IsChild());
}

ParentPermissionDialog::~ParentPermissionDialog() {
  // Close the underlying widget if this object is deleted.
  if (close_dialog_view_callback_)
    std::move(close_dialog_view_callback_).Run();
}

void ParentPermissionDialog::ShowPrompt(content::WebContents* web_contents,
                                        const base::string16& message,
                                        const SkBitmap& icon) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  message_ = message;

  if (!icon.isNull()) {
    const gfx::Image image = gfx::Image::CreateFrom1xBitmap(icon);
    if (!image.IsEmpty())
      icon_ = *image.ToImageSkia();
  }
  LoadParentEmailAddresses();
  ShowPromptInternal(false /* show_password_incorrect */);
}

void ParentPermissionDialog::ShowPromptForExtensionInstallation(
    content::WebContents* web_contents,
    const extensions::Extension* extension,
    const SkBitmap& fallback_icon) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  extension_ = extension;

  if (!fallback_icon.isNull()) {
    const gfx::Image image = gfx::Image::CreateFrom1xBitmap(fallback_icon);
    if (!image.IsEmpty())
      icon_ = *image.ToImageSkia();
  }

  LoadParentEmailAddresses();
  // Prompt is shown after extension icon is loaded.
  LoadExtensionIcon();
}

bool ParentPermissionDialog::CredentialWasInvalid() const {
  return invalid_credential_received_;
}

// static
void ParentPermissionDialog::SetFakeIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  test_identity_manager = identity_manager;
}

void ParentPermissionDialog::LoadParentEmailAddresses() {
  // Get the parents' email addresses.  There can be a max of 2 parent email
  // addresses, the primary and the secondary.
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_);

  base::string16 primary_parent_email =
      base::UTF8ToUTF16(service->GetCustodianEmailAddress());
  if (!primary_parent_email.empty())
    parent_permission_email_addresses_.push_back(primary_parent_email);

  base::string16 secondary_parent_email =
      base::UTF8ToUTF16(service->GetSecondCustodianEmailAddress());
  if (!secondary_parent_email.empty())
    parent_permission_email_addresses_.push_back(secondary_parent_email);

  if (parent_permission_email_addresses_.empty()) {
    // TODO(danan):  Add UMA stat for this failure.
    // https://crbug.com/1049418
    SendResult(Result::kParentPermissionFailed);
  }
}

void ParentPermissionDialog::OnExtensionIconLoaded(const gfx::Image& image) {
  // The order of preference for the icon to use is:
  //  1. Icon loaded from extension, if not empty.
  //  2. Icon passed in params, if not empty.
  //  3. Default Icon.
  if (!image.IsEmpty()) {
    // Use the image that was loaded from the extension if it's not empty
    icon_ = *image.ToImageSkia();
  } else if (icon_.isNull()) {
    // If the params icon is empty, use a default icon.:
    icon_ = GetDefaultIconBitmapForMaxScaleFactor(extension_->is_app());
  }

  ShowPromptInternal(false /* show_password_incorrect */);
}

void ParentPermissionDialog::LoadExtensionIcon() {
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension_, extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);

  // Load the image asynchronously. The response will be sent to
  // OnExtensionIconLoaded.
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile_);

  std::vector<extensions::ImageLoader::ImageRepresentation> images_list;
  images_list.push_back(extensions::ImageLoader::ImageRepresentation(
      image, extensions::ImageLoader::ImageRepresentation::NEVER_RESIZE,
      gfx::Size(),
      ui::GetScaleFactorForNativeView(web_contents_->GetNativeView())));
  loader->LoadImagesAsync(
      extension_, images_list,
      base::BindOnce(&ParentPermissionDialog::OnExtensionIconLoaded,
                     weak_factory_.GetWeakPtr()));
}

void ParentPermissionDialog::ShowPromptInternal(bool show_password_incorrect) {
  close_dialog_view_callback_ = ShowParentPermissionDialog(
      profile_, web_contents_->GetTopLevelNativeWindow(),
      parent_permission_email_addresses_, show_password_incorrect, icon_,
      message_, extension_,
      base::BindOnce(&ParentPermissionDialog::OnParentPermissionPromptDone,
                     weak_factory_.GetWeakPtr()));
}

void ParentPermissionDialog::OnParentPermissionPromptDone(
    internal::ParentPermissionDialogViewResult result) {
  if (result.status ==
      internal::ParentPermissionDialogViewResult::Status::kAccepted) {
    HandleParentPermissionDialogAccepted(result);
  } else {
    SendResult(Result::kParentPermissionCanceled);
  }
}

void ParentPermissionDialog::HandleParentPermissionDialogAccepted(
    internal::ParentPermissionDialogViewResult result) {
  std::string parent_obfuscated_gaia_id =
      GetParentObfuscatedGaiaID(result.selected_parent_permission_email);
  std::string parent_credential =
      base::UTF16ToUTF8(result.parent_permission_credential);
  StartReAuthAccessTokenFetch(parent_obfuscated_gaia_id, parent_credential);
}

std::string ParentPermissionDialog::GetParentObfuscatedGaiaID(
    const base::string16& parent_email) const {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(profile_);

  if (service->GetCustodianEmailAddress() == base::UTF16ToUTF8(parent_email))
    return service->GetCustodianObfuscatedGaiaId();

  if (service->GetSecondCustodianEmailAddress() ==
      base::UTF16ToUTF8(parent_email)) {
    return service->GetSecondCustodianObfuscatedGaiaId();
  }

  NOTREACHED()
      << "Tried to get obfuscated gaia id for a non-custodian email address";
  return "";
}

void ParentPermissionDialog::StartReAuthAccessTokenFetch(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential) {
  // The first step of ReAuth is to fetch an OAuth2 access token for the
  // Reauth API scope.
  if (test_identity_manager)
    identity_manager_ = test_identity_manager;
  else
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  DCHECK(!oauth2_access_token_fetcher_);
  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(),
          "chrome_webstore_private_api", scopes,
          base::BindOnce(&ParentPermissionDialog::OnAccessTokenFetchComplete,
                         weak_factory_.GetWeakPtr(), parent_obfuscated_gaia_id,
                         parent_credential),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void ParentPermissionDialog::OnAccessTokenFetchComplete(
    const std::string& parent_obfuscated_gaia_id,
    const std::string& parent_credential,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    SendResult(Result::kParentPermissionFailed);
    return;
  }

  // Now that we have the OAuth2 access token, we use it when we attempt
  // to fetch the ReAuthProof token (RAPT) for the parent.
  StartParentReAuthProofTokenFetch(
      access_token_info.token, parent_obfuscated_gaia_id, parent_credential);
}

void ParentPermissionDialog::StartParentReAuthProofTokenFetch(
    const std::string& child_access_token,
    const std::string& parent_obfuscated_gaia_id,
    const std::string& credential) {
  reauth_token_fetcher_ = std::make_unique<GaiaAuthFetcher>(
      this, gaia::GaiaSource::kChromeOS, profile_->GetURLLoaderFactory());
  reauth_token_fetcher_->StartCreateReAuthProofTokenForParent(
      child_access_token, parent_obfuscated_gaia_id, credential);
}

void ParentPermissionDialog::SendResult(Result result) {
  std::move(callback_).Run(result);
}

void ParentPermissionDialog::OnReAuthProofTokenSuccess(
    const std::string& reauth_proof_token) {
  SendResult(Result::kParentPermissionReceived);
}

void ParentPermissionDialog::OnReAuthProofTokenFailure(
    const GaiaAuthConsumer::ReAuthProofTokenStatus error) {
  reauth_token_fetcher_.reset();
  if (error == GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant) {
    // Signal to tests that invalid credential was received.
    invalid_credential_received_ = true;

    // If invalid password was entered, and the dialog is configured to
    // re-prompt  show the dialog again with the invalid password error message.
    // prompt again, this time with a password error message.
    if (reprompt_after_incorrect_credential_)
      ShowPromptInternal(true /* show password incorrect */);
    else
      SendResult(Result::kParentPermissionFailed);  // Fail immediately if not
                                                    // reprompting.

  } else {
    SendResult(Result::kParentPermissionFailed);
  }
}
