// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/saml/password_expiry_notification.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/confirm_password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_handler.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/urgent_password_expiry_notification_handler.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
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

ConfirmPasswordChangeDialog* g_confirm_dialog = nullptr;

UrgentPasswordExpiryNotificationDialog* g_notification_dialog = nullptr;

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

base::string16 GetHostedHeaderText(const std::string& password_change_url) {
  base::string16 host =
      base::UTF8ToUTF16(net::GetHostAndOptionalPort(GURL(password_change_url)));
  DCHECK(!host.empty());
  return l10n_util::GetStringFUTF16(IDS_LOGIN_SAML_PASSWORD_CHANGE_NOTICE,
                                    host);
}

constexpr int kMaxPasswordChangeDialogWidth = 768;
constexpr int kMaxPasswordChangeDialogHeight = 640;

// TODO(https://crbug.com/930109): Change these numbers depending on what is
// shown in the dialog.
constexpr int kMaxConfirmPasswordChangeDialogWidth = 520;
constexpr int kMaxConfirmPasswordChangeDialogHeight = 380;

constexpr int kMaxNotificationDialogWidth = 768;
constexpr int kMaxNotificationDialogHeight = 640;

// Given a desired width and height, returns the same size if it fits on screen,
// or the closest possible size that will fit on the screen.
gfx::Size FitSizeToDisplay(int max_width, int max_height) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size = gfx::Size(std::min(display_size.width(), max_width),
                           std::min(display_size.height(), max_height));

  return display_size;
}

}  // namespace

PasswordChangeDialog::PasswordChangeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIPasswordChangeUrl),
                              /*title=*/base::string16()) {}

PasswordChangeDialog::~PasswordChangeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

void PasswordChangeDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(kMaxPasswordChangeDialogWidth,
                           kMaxPasswordChangeDialogHeight);
}

void PasswordChangeDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::ModalType PasswordChangeDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

// static
void PasswordChangeDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new PasswordChangeDialog();
  g_dialog->ShowSystemDialog();
}

// static
void PasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog)
    g_dialog->Close();
}

PasswordChangeUI::PasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordChangeHost);

  const std::string password_change_url = GetPasswordChangeUrl(profile);
  web_ui->AddMessageHandler(
      std::make_unique<PasswordChangeHandler>(password_change_url));

  source->AddString("hostedHeader", GetHostedHeaderText(password_change_url));
  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_PASSWORD_CHANGE_HTML);

  source->AddResourcePath("password_change.css", IDR_PASSWORD_CHANGE_CSS);
  source->AddResourcePath("authenticator.js",
                          IDR_PASSWORD_CHANGE_AUTHENTICATOR_JS);
  source->AddResourcePath("password_change.js", IDR_PASSWORD_CHANGE_JS);

  content::WebUIDataSource::Add(profile, source);
}

PasswordChangeUI::~PasswordChangeUI() = default;

// static
void ConfirmPasswordChangeDialog::Show(const std::string& scraped_old_password,
                                       const std::string& scraped_new_password,
                                       bool show_spinner_initially) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog) {
    g_confirm_dialog->Focus();
    return;
  }
  g_confirm_dialog = new ConfirmPasswordChangeDialog(
      scraped_old_password, scraped_new_password, show_spinner_initially);
  g_confirm_dialog->ShowSystemDialog();
}

// static
void ConfirmPasswordChangeDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_confirm_dialog)
    g_confirm_dialog->Close();
}

ConfirmPasswordChangeDialog::ConfirmPasswordChangeDialog(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password,
    bool show_spinner_initially)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIConfirmPasswordChangeUrl),
                              /*title=*/base::string16()),
      scraped_old_password_(scraped_old_password),
      scraped_new_password_(scraped_new_password),
      show_spinner_initially_(show_spinner_initially) {}

ConfirmPasswordChangeDialog::~ConfirmPasswordChangeDialog() {
  DCHECK_EQ(this, g_confirm_dialog);
  g_confirm_dialog = nullptr;
}

void ConfirmPasswordChangeDialog::GetDialogSize(gfx::Size* size) const {
  *size = FitSizeToDisplay(kMaxConfirmPasswordChangeDialogWidth,
                           kMaxConfirmPasswordChangeDialogHeight);
}

std::string ConfirmPasswordChangeDialog::GetDialogArgs() const {
  // TODO(https://crbug.com/930109): Configure the embedded UI to only display
  // prompts for the passwords that were not scraped.
  std::string data;
  base::DictionaryValue dialog_args;
  dialog_args.SetBoolean("showSpinnerInitially", show_spinner_initially_);
  base::JSONWriter::Write(dialog_args, &data);
  return data;
}

void ConfirmPasswordChangeDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::ModalType ConfirmPasswordChangeDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

ConfirmPasswordChangeUI::ConfirmPasswordChangeUI(content::WebUI* web_ui)
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

  web_ui->AddMessageHandler(std::make_unique<ConfirmPasswordChangeHandler>());

  content::WebUIDataSource::Add(profile, source);
}

ConfirmPasswordChangeUI::~ConfirmPasswordChangeUI() = default;

UrgentPasswordExpiryNotificationDialog::UrgentPasswordExpiryNotificationDialog()
    : SystemWebDialogDelegate(
          GURL(chrome::kChromeUIUrgentPasswordExpiryNotificationUrl),
          /*title=*/base::string16()) {}

UrgentPasswordExpiryNotificationDialog::
    ~UrgentPasswordExpiryNotificationDialog() {
  DCHECK_EQ(this, g_notification_dialog);
  g_notification_dialog = nullptr;
}

void UrgentPasswordExpiryNotificationDialog::GetDialogSize(
    gfx::Size* size) const {
  *size = FitSizeToDisplay(kMaxNotificationDialogWidth,
                           kMaxNotificationDialogHeight);
}

void UrgentPasswordExpiryNotificationDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
}

ui::ModalType UrgentPasswordExpiryNotificationDialog::GetDialogModalType()
    const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

// static
void UrgentPasswordExpiryNotificationDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog) {
    g_notification_dialog->Focus();
    return;
  }
  g_notification_dialog = new UrgentPasswordExpiryNotificationDialog();
  g_notification_dialog->ShowSystemDialog();
}

// static
void UrgentPasswordExpiryNotificationDialog::Dismiss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_notification_dialog)
    g_notification_dialog->Close();
}

UrgentPasswordExpiryNotificationUI::UrgentPasswordExpiryNotificationUI(
    content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  CHECK(prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled));

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIUrgentPasswordExpiryNotificationHost);

  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (attrs.has_expiration_time()) {
    const base::Time expiration_time = attrs.expiration_time();
    source->AddString("initialTitle", PasswordExpiryNotification::GetTitleText(
                                          expiration_time - base::Time::Now()));
    source->AddString("expirationTime",
                      base::NumberToString(expiration_time.ToJsTime()));
  }

  static constexpr LocalizedString kLocalizedStrings[] = {
      {"body", IDS_PASSWORD_EXPIRY_CALL_TO_ACTION_CRITICAL},
      {"button", IDS_OK}};
  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_URGENT_PASSWORD_EXPIRY_NOTIFICATION_HTML);
  source->AddResourcePath("urgent_password_expiry_notification.js",
                          IDR_URGENT_PASSWORD_EXPIRY_NOTIFICATION_JS);

  web_ui->AddMessageHandler(
      std::make_unique<UrgentPasswordExpiryNotificationHandler>());

  content::WebUIDataSource::Add(profile, source);
}

UrgentPasswordExpiryNotificationUI::~UrgentPasswordExpiryNotificationUI() =
    default;

}  // namespace chromeos
