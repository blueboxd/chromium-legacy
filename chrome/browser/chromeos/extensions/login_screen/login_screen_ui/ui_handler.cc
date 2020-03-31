// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/ui_handler.h"

#include <algorithm>
#include <iterator>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/create_options.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/window.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {

const char kErrorWindowAlreadyExists[] =
    "Login screen extension UI already in use.";
const char kErrorNoExistingWindow[] = "No open window to close.";
const char kErrorNotOnLoginOrLockScreen[] =
    "Windows can only be created on the login and lock screen.";

struct ExtensionNameMapping {
  const char* extension_id;
  const char* extension_name;
};

// Hardcoded extension names to be used in the window's dialog title.
// Intentionally not using |extension->name()| here to prevent a compromised
// extension from being able to control the dialog title's content.
// This list has to be sorted for quick access using std::lower_bound.
const ExtensionNameMapping kExtensionNameMappings[] = {
    {"bnfoibgpjolimhppjmligmcgklpboloj", "Imprivata"},
    {"cdgickkdpbekbnalbmpgochbninibkko", "Imprivata"},
    {"dbknmmkopacopifbkgookcdbhfnggjjh", "Imprivata"},
    {"ddcjglpbfbibgepfffpklmpihphbcdco", "Imprivata"},
    {"dhodapiemamlmhlhblgcibabhdkohlen", "Imprivata"},
    {"dlahpllbhpbkfnoiedkgombmegnnjopi", "Imprivata"},
    {"lpimkpkllnkdlcigdbgmabfplniahkgm", "Imprivata"},
    {"oclffehlkdgibkainkilopaalpdobkan", "LoginScreenUi test extension"},
    {"odehonhhkcjnbeaomlodfkjaecbmhklm", "Imprivata"},
    {"olnmflhcfkifkgbiegcoabineoknmbjc", "Imprivata"},
    {"phjobickjiififdadeoepbdaciefacfj", "Imprivata"},
    {"pkeacbojooejnjolgjdecbpnloibpafm", "Imprivata"},
    {"pmhiabnkkchjeaehcodceadhdpfejmmd", "Imprivata"},
};
const ExtensionNameMapping* kExtensionNameMappingsEnd =
    std::end(kExtensionNameMappings);

std::string GetHardcodedExtensionName(const extensions::Extension* extension) {
  DCHECK(std::is_sorted(kExtensionNameMappings, kExtensionNameMappingsEnd,
                        [](const ExtensionNameMapping& entry1,
                           const ExtensionNameMapping& entry2) {
                          return strcmp(entry1.extension_id,
                                        entry2.extension_id) < 0;
                        }));

  const char* extension_id = extension->id().c_str();
  const ExtensionNameMapping* entry = std::lower_bound(
      kExtensionNameMappings, kExtensionNameMappingsEnd, extension_id,
      [](const ExtensionNameMapping& entry, const char* key) {
        return strcmp(entry.extension_id, key) < 0;
      });
  if (entry == kExtensionNameMappingsEnd ||
      strcmp(entry->extension_id, extension_id) != 0) {
    NOTREACHED();
    return "UNKNOWN EXTENSION";
  }
  return entry->extension_name;
}

bool CanUseLoginScreenUiApi(const extensions::Extension* extension) {
  return extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile())
             ->enabled_extensions()
             .Contains(extension->id()) &&
         extension->permissions_data()->HasAPIPermission(
             extensions::APIPermission::kLoginScreenUi) &&
         InstallAttributes::Get()->IsEnterpriseManaged();
}

login_screen_extension_ui::UiHandler* g_instance = nullptr;

}  // namespace

namespace login_screen_extension_ui {

ExtensionIdToWindowMapping::ExtensionIdToWindowMapping(
    const std::string& extension_id,
    std::unique_ptr<Window> window)
    : extension_id(extension_id), window(std::move(window)) {}

ExtensionIdToWindowMapping::~ExtensionIdToWindowMapping() = default;

// static
UiHandler* UiHandler::Get(bool can_create) {
  if (!g_instance && can_create) {
    std::unique_ptr<WindowFactory> window_factory =
        std::make_unique<WindowFactory>();
    g_instance = new UiHandler(std::move(window_factory));
  }
  return g_instance;
}

// static
void UiHandler::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

UiHandler::UiHandler(std::unique_ptr<WindowFactory> window_factory)
    : window_factory_(std::move(window_factory)) {
  UpdateSessionState();
  session_manager_observer_.Add(session_manager::SessionManager::Get());
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile()));
}

UiHandler::~UiHandler() = default;

bool UiHandler::Show(const extensions::Extension* extension,
                     const std::string& resource_path,
                     bool can_be_closed_by_user,
                     std::string* error) {
  CHECK(CanUseLoginScreenUiApi(extension));
  if (!login_or_lock_screen_active_) {
    *error = kErrorNotOnLoginOrLockScreen;
    return false;
  }
  if (current_window_) {
    *error = kErrorWindowAlreadyExists;
    return false;
  }

  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::EXTENSION_LOGIN);

  CreateOptions create_options(
      GetHardcodedExtensionName(extension),
      extension->GetResourceURL(resource_path), can_be_closed_by_user,
      base::BindOnce(base::IgnoreResult(&UiHandler::RemoveWindowForExtension),
                     weak_ptr_factory_.GetWeakPtr(), extension->id()));

  current_window_ = std::make_unique<ExtensionIdToWindowMapping>(
      extension->id(), window_factory_->Create(&create_options));

  return true;
}

bool UiHandler::Close(const extensions::Extension* extension,
                      std::string* error) {
  CHECK(CanUseLoginScreenUiApi(extension));
  if (!RemoveWindowForExtension(extension->id())) {
    *error = kErrorNoExistingWindow;
    return false;
  }
  return true;
}

bool UiHandler::RemoveWindowForExtension(const std::string& extension_id) {
  if (!HasOpenWindow(extension_id))
    return false;

  current_window_.reset(nullptr);

  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::HIDDEN);

  return true;
}

bool UiHandler::HasOpenWindow(const std::string& extension_id) const {
  return current_window_ && current_window_->extension_id == extension_id;
}

void UiHandler::UpdateSessionState() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  bool new_login_or_lock_screen_active =
      state == session_manager::SessionState::LOGIN_PRIMARY ||
      state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE ||
      state == session_manager::SessionState::LOCKED;
  if (login_or_lock_screen_active_ == new_login_or_lock_screen_active)
    return;

  login_or_lock_screen_active_ = new_login_or_lock_screen_active;

  if (!login_or_lock_screen_active_)
    current_window_.reset(nullptr);
}

void UiHandler::OnSessionStateChanged() {
  UpdateSessionState();
}

void UiHandler::OnExtensionUninstalled(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension,
                                       extensions::UninstallReason reason) {
  HandleExtensionUnloadOrUinstall(extension);
}

void UiHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  HandleExtensionUnloadOrUinstall(extension);
}

void UiHandler::HandleExtensionUnloadOrUinstall(
    const extensions::Extension* extension) {
  RemoveWindowForExtension(extension->id());
}

Window* UiHandler::GetWindowForTesting(const std::string& extension_id) {
  if (!HasOpenWindow(extension_id))
    return nullptr;

  return current_window_->window.get();
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
