// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_extension_ui_handler.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_window.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {

const char kErrorWindowAlreadyExists[] =
    "Can't create more than one window per extension.";
const char kErrorNoExistingWindow[] = "No open window to close.";
const char kErrorNotOnLoginOrLockScreen[] =
    "Windows can only be created on the login and lock screen.";

LoginScreenExtensionUiHandler* g_instance = nullptr;

bool CanUseLoginScreenUiApi(const extensions::Extension* extension) {
  return extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile())
             ->enabled_extensions()
             .Contains(extension->id()) &&
         extension->permissions_data()->HasAPIPermission(
             extensions::APIPermission::kLoginScreenUi);
}

}  // namespace

// static
LoginScreenExtensionUiHandler* LoginScreenExtensionUiHandler::Get(
    bool can_create) {
  if (!g_instance && can_create) {
    std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory =
        std::make_unique<LoginScreenExtensionUiWindowFactory>();
    g_instance = new LoginScreenExtensionUiHandler(std::move(window_factory));
  }
  return g_instance;
}

// static
void LoginScreenExtensionUiHandler::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

LoginScreenExtensionUiHandler::LoginScreenExtensionUiHandler(
    std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory)
    : window_factory_(std::move(window_factory)),
      session_manager_observer_(this),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  UpdateSessionState();
  session_manager_observer_.Add(session_manager::SessionManager::Get());
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile()));
}

LoginScreenExtensionUiHandler::~LoginScreenExtensionUiHandler() = default;

bool LoginScreenExtensionUiHandler::Show(const extensions::Extension* extension,
                                         const std::string& resource_path,
                                         bool can_be_closed_by_user,
                                         std::string* error) {
  DCHECK(CanUseLoginScreenUiApi(extension));
  if (!login_or_lock_screen_active_) {
    *error = kErrorNotOnLoginOrLockScreen;
    return false;
  }
  if (HasOpenWindow(extension->id())) {
    *error = kErrorWindowAlreadyExists;
    return false;
  }

  if (!HasOpenWindow()) {
    ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
        ash::OobeDialogState::EXTENSION_LOGIN);
  }

  LoginScreenExtensionUiWindow::CreateOptions create_options(
      extension->short_name(), extension->GetResourceURL(resource_path),
      can_be_closed_by_user,
      base::BindOnce(
          base::IgnoreResult(
              &LoginScreenExtensionUiHandler::RemoveWindowForExtension),
          weak_ptr_factory_.GetWeakPtr(), extension->id()));
  std::unique_ptr<LoginScreenExtensionUiWindow> window =
      window_factory_->Create(&create_options);
  windows_.emplace(extension->id(), std::move(window));

  return true;
}

bool LoginScreenExtensionUiHandler::Close(
    const extensions::Extension* extension,
    std::string* error) {
  DCHECK(CanUseLoginScreenUiApi(extension));
  if (!RemoveWindowForExtension(extension->id())) {
    *error = kErrorNoExistingWindow;
    return false;
  }
  return true;
}

bool LoginScreenExtensionUiHandler::RemoveWindowForExtension(
    const std::string& extension_id) {
  WindowMap::iterator it = windows_.find(extension_id);
  if (it == windows_.end())
    return false;
  windows_.erase(it);

  if (!HasOpenWindow()) {
    ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
        ash::OobeDialogState::HIDDEN);
  }

  return true;
}

bool LoginScreenExtensionUiHandler::HasOpenWindow(
    const std::string& extension_id) const {
  return windows_.find(extension_id) != windows_.end();
}

bool LoginScreenExtensionUiHandler::HasOpenWindow() const {
  return !windows_.empty();
}

void LoginScreenExtensionUiHandler::UpdateSessionState() {
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
    windows_.clear();
}

void LoginScreenExtensionUiHandler::OnSessionStateChanged() {
  UpdateSessionState();
}

void LoginScreenExtensionUiHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RemoveWindowForExtension(extension->id());
}

}  // namespace chromeos
