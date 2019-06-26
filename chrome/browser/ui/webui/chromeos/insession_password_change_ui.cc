// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_handler_chromeos.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

PasswordChangeDialog* g_dialog = nullptr;

std::string GetPasswordChangeUrl(Profile* profile) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSamlPasswordChangeUrl)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kSamlPasswordChangeUrl);
  }

  const policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (user_cloud_policy_manager) {
    const enterprise_management::PolicyData* policy =
        user_cloud_policy_manager->core()->store()->policy();
    if (policy->has_change_password_uri()) {
      return policy->change_password_uri();
    }
  }

  return SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs())
      .password_change_url();
}

base::string16 GetManagementNotice(Profile* profile) {
  base::string16 host = base::UTF8ToUTF16(
      net::GetHostAndOptionalPort(GURL(GetPasswordChangeUrl(profile))));
  DCHECK(!host.empty());
  return l10n_util::GetStringFUTF16(IDS_LOGIN_SAML_PASSWORD_CHANGE_NOTICE,
                                    host);
}

constexpr int kMaxDialogWidth = 768;
constexpr int kMaxDialogHeight = 640;

gfx::Size GetPasswordChangeDialogSize() {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size = gfx::Size(std::min(display_size.width(), kMaxDialogWidth),
                           std::min(display_size.height(), kMaxDialogHeight));

  return display_size;
}

}  // namespace

PasswordChangeDialog::PasswordChangeDialog(const base::string16& title)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIPasswordChangeUrl), title) {
}

PasswordChangeDialog::~PasswordChangeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

void PasswordChangeDialog::GetDialogSize(gfx::Size* size) const {
  *size = GetPasswordChangeDialogSize();
}

void PasswordChangeDialog::Show(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new PasswordChangeDialog(GetManagementNotice(profile));
  g_dialog->ShowSystemDialog();
}

InSessionPasswordChangeUI::InSessionPasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordChangeHost);

  web_ui->AddMessageHandler(std::make_unique<InSessionPasswordChangeHandler>(
      GetPasswordChangeUrl(profile)));

  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_PASSWORD_CHANGE_HTML);

  source->AddResourcePath("password_change.css", IDR_PASSWORD_CHANGE_CSS);
  source->AddResourcePath("authenticator.js",
                          IDR_PASSWORD_CHANGE_AUTHENTICATOR_JS);
  source->AddResourcePath("password_change.js", IDR_PASSWORD_CHANGE_JS);

  content::WebUIDataSource::Add(profile, source);
}

InSessionPasswordChangeUI::~InSessionPasswordChangeUI() = default;

InSessionConfirmPasswordChangeUI::InSessionConfirmPasswordChangeUI(
    content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIConfirmPasswordChangeHost);

  static constexpr LocalizedString kLocalizedStrings[] = {
      {"title", IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_TITLE},
      {"bothPasswordsPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_BOTH_PASSWORDS_PROMPT},
      {"oldPasswordPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_OLD_PASSWORD_PROMPT},
      {"newPasswordPrompt",
       IDS_PASSWORD_CHANGE_CONFIRM_DIALOG_NEW_PASSWORD_PROMPT},
      {"oldPassword", IDS_PASSWORD_CHANGE_OLD_PASSWORD_LABEL},
      {"newPassword", IDS_PASSWORD_CHANGE_NEW_PASSWORD_LABEL},
      {"confirmNewPassword", IDS_PASSWORD_CHANGE_CONFIRM_NEW_PASSWORD_LABEL},
      {"matchError", IDS_PASSWORD_CHANGE_PASSWORDS_DONT_MATCH},
      {"save", IDS_PASSWORD_CHANGE_CONFIRM_SAVE_BUTTON}};

  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_CONFIRM_PASSWORD_CHANGE_HTML);
  source->AddResourcePath("confirm_password_change.js",
                          IDR_CONFIRM_PASSWORD_CHANGE_JS);

  // TODO(https://crbug.com/930109): Add JS and message handler to handle tap
  // on the save button, errors, etc.

  content::WebUIDataSource::Add(profile, source);
}

InSessionConfirmPasswordChangeUI::~InSessionConfirmPasswordChangeUI() = default;

}  // namespace chromeos
