// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "net/base/url_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

AssistantOptInDialog* g_dialog = nullptr;

constexpr int kAssistantOptInDialogWidth = 768;
constexpr int kAssistantOptInDialogHeight = 640;
constexpr int kCaptionBarHeight = 32;
constexpr char kFlowTypeParamKey[] = "flow-type";
constexpr char kCaptionBarHeightParamKey[] = "caption-bar-height";

GURL CreateAssistantOptInURL(ash::FlowType type) {
  GURL gurl(chrome::kChromeUIAssistantOptInURL);
  gurl = net::AppendQueryParameter(
      gurl, kFlowTypeParamKey, base::NumberToString(static_cast<int>(type)));
  gurl = net::AppendQueryParameter(gurl, kCaptionBarHeightParamKey,
                                   base::NumberToString(kCaptionBarHeight));
  return gurl;
}

void DisablePolymer2(content::URLDataSource* shared_source) {
  if (shared_source)
    shared_source->DisablePolymer2ForHost(chrome::kChromeUIAssistantOptInHost);
}

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  auto assistant_handler =
      std::make_unique<AssistantOptInFlowScreenHandler>(&js_calls_container_);
  assistant_handler_ptr_ = assistant_handler.get();
  web_ui->AddMessageHandler(std::move(assistant_handler));
  assistant_handler_ptr_->set_on_initialized(base::BindOnce(
      &AssistantOptInUI::Initialize, weak_factory_.GetWeakPtr()));
  assistant_handler_ptr_->SetupAssistantConnection();

  base::DictionaryValue localized_strings;
  assistant_handler_ptr_->GetLocalizedStrings(&localized_strings);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->AddResourcePath("assistant_logo.png", IDR_ASSISTANT_LOGO_PNG);
  source->AddBoolean("hotwordDspAvailable", chromeos::IsHotwordDspAvailable());
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Do not zoom for Assistant opt-in web contents.
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_ui->GetWebContents());
  DCHECK(zoom_map);
  zoom_map->SetZoomLevelForHost(web_ui->GetWebContents()->GetURL().host(), 0);

  // If allowed, request that the shared resources send this page Polymer 1
  // resources instead of Polymer 2.
  // TODO (https://crbug.com/739611): Remove this exception by migrating to
  // Polymer 2.
  if (base::FeatureList::IsEnabled(features::kWebUIPolymer2Exceptions)) {
    content::URLDataSource::GetSourceForURL(
        Profile::FromWebUI(web_ui),
        GURL("chrome://resources/polymer/v1_0/polymer/polymer.html"),
        base::BindOnce(DisablePolymer2));
  }
}

AssistantOptInUI::~AssistantOptInUI() = default;

void AssistantOptInUI::OnDialogClosed() {
  if (assistant_handler_ptr_) {
    assistant_handler_ptr_->OnDialogClosed();
  }
}

void AssistantOptInUI::Initialize() {
  js_calls_container_.ExecuteDeferredJSCalls(web_ui());
}

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show(
    ash::FlowType type,
    ash::AssistantSetup::StartAssistantOptInFlowCallback callback) {
  // Check session state here to prevent timing issue -- session state might
  // have changed during the mojom calls to launch the opt-in dalog.
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }
  if (g_dialog) {
    g_dialog->Focus();
    std::move(callback).Run(false);
    return;
  }
  g_dialog = new AssistantOptInDialog(type, std::move(callback));

  g_dialog->ShowSystemDialog();
}

AssistantOptInDialog::AssistantOptInDialog(
    ash::FlowType type,
    ash::AssistantSetup::StartAssistantOptInFlowCallback callback)
    : SystemWebDialogDelegate(CreateAssistantOptInURL(type), base::string16()),
      callback_(std::move(callback)) {}

AssistantOptInDialog::~AssistantOptInDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

void AssistantOptInDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kAssistantOptInDialogWidth,
                kAssistantOptInDialogHeight - kCaptionBarHeight);
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

void AssistantOptInDialog::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  assistant_ui_ = static_cast<AssistantOptInUI*>(webui->GetController());
}

void AssistantOptInDialog::OnDialogClosed(const std::string& json_retval) {
  if (assistant_ui_)
    assistant_ui_->OnDialogClosed();

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const bool completed =
      prefs->GetBoolean(arc::prefs::kVoiceInteractionEnabled) &&
      (prefs->GetInteger(assistant::prefs::kAssistantConsentStatus) ==
       assistant::prefs::ConsentStatus::kActivityControlAccepted);
  std::move(callback_).Run(completed);
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

}  // namespace chromeos
