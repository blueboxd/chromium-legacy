// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

constexpr int kDialogHeightPx = 640;
constexpr int kDialogWidthPx = 768;
// Id of System Dialog used to show the Add Supervision flow.
std::string& GetDialogId() {
  static base::NoDestructor<std::string> dialog_id;
  return *dialog_id;
}

}  // namespace

namespace chromeos {

const char kAddSupervisionURL[] =
    "https://families.google.com/supervision/setup";
const char kAddSupervisionEventOriginFilter[] = "https://families.google.com";
const char kAddSupervisionFlowType[] = "1";

// AddSupervisionDialog implementations.

// static
void AddSupervisionDialog::Show() {
  SystemWebDialogDelegate* current_instance = GetInstance();
  if (current_instance) {
    // Focus the dialog if it is already there.  Currently, this is
    // effectively a no-op, since the dialog is system-modal, but
    // it's here nonethless so that if the dialog becomes non-modal
    // at some point, the correct focus behavior occurs.
    current_instance->Focus();
    return;
  }
  // Note:  |current_instance|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  current_instance = new AddSupervisionDialog();

  GetDialogId() = current_instance->Id();

  // TODO(danan): This should pass ash_util::GetFramelessInitParams() for
  // extra_params. Currently however, that prevents the dialog from presenting
  // in full screen if tablet mode is enabled. See https://crbug.com/888629.
  chrome::ShowWebDialog(nullptr /* no parent */,
                        ProfileManager::GetPrimaryUserProfile(),
                        current_instance);
}

void AddSupervisionDialog::AddOnCloseCallback(base::OnceClosure callback) {
  on_close_callbacks_.push_back(std::move(callback));
}

ui::ModalType AddSupervisionDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

void AddSupervisionDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthPx, kDialogHeightPx);
}

void AddSupervisionDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == GetInstance());
  GetDialogId() = "";

  // Note: The call below deletes |this|, so there is no further need to keep
  // track of the pointer.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

AddSupervisionDialog::AddSupervisionDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAddSupervisionURL),
                              base::string16()) {}

AddSupervisionDialog::~AddSupervisionDialog() {
  for (auto& callback : on_close_callbacks_)
    std::move(callback).Run();
}

// static
SystemWebDialogDelegate* AddSupervisionDialog::GetInstance() {
  return SystemWebDialogDelegate::FindInstance(GetDialogId());
}

// AddSupervisionUI implementations.

AddSupervisionUI::AddSupervisionUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  // Register the Mojo API handler.
  AddHandlerToRegistry(base::BindRepeating(
      &AddSupervisionUI::BindAddSupervisionHandler, base::Unretained(this)));

  // Set up the basic page framework.
  SetupResources();
}

void AddSupervisionUI::SetupResources() {
  Profile* profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAddSupervisionHost));

  source->AddResourcePath("post_message_api.js",
                          IDR_ADD_SUPERVISION_POST_MESSAGE_API_JS);
  source->AddResourcePath("add_supervision_api_server.js",
                          IDR_ADD_SUPERVISION_API_SERVER_JS);
  source->AddResourcePath("add_supervision.js", IDR_ADD_SUPERVISION_JS);
  source->AddLocalizedString("pageTitle", IDS_ADD_SUPERVISION_PAGE_TITLE);

  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath(
      "chrome/browser/ui/webui/chromeos/add_supervision/"
      "add_supervision.mojom-lite.js",
      IDR_ADD_SUPERVISION_MOJOM_LITE_JS);

  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_ADD_SUPERVISION_HTML);
  source->AddString("webviewUrl", kAddSupervisionURL);
  source->AddString("eventOriginFilter", kAddSupervisionEventOriginFilter);
  source->AddString("platformVersion", base::SysInfo::OperatingSystemVersion());
  source->AddString("flowType", kAddSupervisionFlowType);

  content::WebUIDataSource::Add(profile, source.release());
}

AddSupervisionUI::~AddSupervisionUI() = default;

void AddSupervisionUI::BindAddSupervisionHandler(
    add_supervision::mojom::AddSupervisionHandlerRequest request) {
  mojo_api_handler_.reset(
      new AddSupervisionHandler(std::move(request), web_ui()));
}

}  // namespace chromeos
