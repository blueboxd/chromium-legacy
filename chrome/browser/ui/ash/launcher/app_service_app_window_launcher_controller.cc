// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_controller.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

AppServiceAppWindowLauncherController::AppServiceAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(owner->profile())),
      app_service_instance_helper_(
          std::make_unique<AppServiceInstanceRegistryHelper>(owner->profile())),
      crostini_tracker_(
          std::make_unique<AppServiceAppWindowCrostiniTracker>()) {
  aura::Env::GetInstance()->AddObserver(this);
  DCHECK(proxy_);
  DCHECK(app_service_instance_helper_);
  DCHECK(crostini_tracker_);
  Observe(&proxy_->InstanceRegistry());
}

AppServiceAppWindowLauncherController::
    ~AppServiceAppWindowLauncherController() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void AppServiceAppWindowLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  if (proxy_)
    Observe(nullptr);

  auto* new_proxy =
      apps::AppServiceProxyFactory::GetForProfile(owner()->profile());
  DCHECK(new_proxy);

  // Deactivates the running app windows in InstanceRegistry for the inactive
  // user, and activates the app windows for the active user.
  for (const auto& window_item : aura_window_to_app_window_) {
    AppWindowBase* app_window = window_item.second.get();
    const std::string app_id = app_window->shelf_id().app_id;
    // Skips ARC apps, because task id is used for ARC apps.
    if (app_id == ash::kInternalAppIdKeyboardShortcutViewer ||
        proxy_->AppRegistryCache().GetAppType(app_id) ==
            apps::mojom::AppType::kArc ||
        new_proxy->AppRegistryCache().GetAppType(app_id) ==
            apps::mojom::AppType::kArc) {
      continue;
    }

    if (!new_proxy->InstanceRegistry()
             .GetWindows(app_window->shelf_id().app_id)
             .empty()) {
      AddToShelf(app_window);
    } else {
      RemoveFromShelf(app_window);
    }
  }

  proxy_ = new_proxy;
  Observe(&proxy_->InstanceRegistry());

  app_service_instance_helper_->ActiveUserChanged();
}

void AppServiceAppWindowLauncherController::OnWindowInitialized(
    aura::Window* window) {
  // An app window has type WINDOW_TYPE_NORMAL, a WindowDelegate and
  // is a top level views widget. Tooltips, menus, and other kinds of transient
  // windows that can't activate are filtered out.
  if (window->type() != aura::client::WINDOW_TYPE_NORMAL || !window->delegate())
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (!widget || !widget->is_top_level())
    return;

  observed_windows_.Add(window);
}

void AppServiceAppWindowLauncherController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key != ash::kShelfIDKey)
    return;

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  if (shelf_id.IsNull())
    return;

  DCHECK(proxy_);
  if (proxy_->AppRegistryCache().GetAppType(shelf_id.app_id) !=
      apps::mojom::AppType::kBuiltIn)
    return;

  app_service_instance_helper_->OnInstances(shelf_id.app_id, window,
                                            shelf_id.launch_id,
                                            apps::InstanceState::kUnknown);

  RegisterAppWindow(window, shelf_id);
}

void AppServiceAppWindowLauncherController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // Skip OnWindowVisibilityChanged for ancestors/descendants.
  if (!observed_windows_.IsObserving(window))
    return;

  ash::ShelfID shelf_id = GetShelfId(window);
  if (shelf_id.IsNull())
    return;

  // Update |state|. The app must be started, and running state. If visible,
  // set it as |kVisible|, otherwise, clear the visible bit.
  apps::InstanceState state = apps::InstanceState::kUnknown;
  proxy_->InstanceRegistry().ForOneInstance(
      window,
      [&state](const apps::InstanceUpdate& update) { state = update.State(); });
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state = (visible) ? static_cast<apps::InstanceState>(
                          state | apps::InstanceState::kVisible)
                    : static_cast<apps::InstanceState>(
                          state & ~(apps::InstanceState::kVisible));

  app_service_instance_helper_->OnInstances(shelf_id.app_id, window,
                                            shelf_id.launch_id, state);

  if (!visible || shelf_id.app_id == extension_misc::kChromeAppId)
    return;

  RegisterAppWindow(window, shelf_id);

  crostini_tracker_->OnWindowVisibilityChanging(window, shelf_id.app_id);
}

void AppServiceAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);

  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
    // Delete the instance from InstanceRegistry.
  app_service_instance_helper_->OnInstances(
      shelf_id.app_id, window, std::string(), apps::InstanceState::kDestroyed);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  RemoveFromShelf(app_window_it->second.get());

  if (!shelf_id.IsNull())
    crostini_tracker_->OnWindowDestroying(shelf_id.app_id);

  aura_window_to_app_window_.erase(app_window_it);
}

void AppServiceAppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  AppWindowLauncherController::OnWindowActivated(reason, new_active,
                                                 old_active);

  SetWindowActivated(new_active, /*active*/ true);
  SetWindowActivated(old_active, /*active*/ false);
}

void AppServiceAppWindowLauncherController::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  aura::Window* window = update.Window();
  if (!observed_windows_.IsObserving(window))
    return;

  // This is the first update for the given window.
  if (update.StateIsNull() &&
      (update.State() & apps::InstanceState::kDestroyed) ==
          apps::InstanceState::kUnknown) {
    std::string app_id = update.AppId();
    window->SetProperty(ash::kAppIDKey, update.AppId());
    ash::ShelfID shelf_id(update.AppId(), update.LaunchId());
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
    return;
  }

  // Launch id is updated, so constructs a new shelf id.
  if (update.LaunchIdChanged()) {
    ash::ShelfID shelf_id(update.AppId(), update.LaunchId());
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  }
}

void AppServiceAppWindowLauncherController::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* instance_registry) {
  Observe(nullptr);
}

void AppServiceAppWindowLauncherController::SetWindowActivated(
    aura::Window* window,
    bool active) {
  if (!window)
    return;

  // If the instance is exist, get the current app_id, launch_id and state.
  ash::ShelfID shelf_id;
  apps::InstanceState state = apps::InstanceState::kUnknown;
  bool instance_exist = proxy_->InstanceRegistry().ForOneInstance(
      window, [&shelf_id, &state](const apps::InstanceUpdate& update) {
        shelf_id = ash::ShelfID(update.AppId(), update.LaunchId());
        state = update.State();
      });
  if (!instance_exist) {
    shelf_id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  }

  // When sets the instance active state, the instance should be in started and
  // running state.
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);

  state = (active)
              ? static_cast<apps::InstanceState>(state |
                                                 apps::InstanceState::kActive)
              : static_cast<apps::InstanceState>(state &
                                                 ~apps::InstanceState::kActive);
  app_service_instance_helper_->OnInstances(shelf_id.app_id, window,
                                            std::string(), state);
}

void AppServiceAppWindowLauncherController::RegisterAppWindow(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // For Chrome apps for Web apps, if the window is opened in a browser tab, we
  // don't need to register app window, because
  // BrowserShortcutLauncherItemController sets the window's property. If
  // register app window for the app opened in a browser tab, the window is
  // added to aura_window_to_app_window_, and when the window is destroyed, it
  // could cause crash in RemoveFromShelf, because
  // BrowserShortcutLauncherItemController manages the window, and sets
  // related window properties, so it could cause the conflict settings.
  if (IsOpenedInBrowserTab(shelf_id.app_id))
    return;

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  auto app_window_ptr = std::make_unique<AppWindowBase>(shelf_id, widget);
  AppWindowBase* app_window = app_window_ptr.get();
  aura_window_to_app_window_[window] = std::move(app_window_ptr);

  AddToShelf(app_window);
}

void AppServiceAppWindowLauncherController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);

  app_window->SetController(nullptr);
}

void AppServiceAppWindowLauncherController::AddToShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would add window to controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(shelf_id);
  if (item_controller == nullptr) {
    auto controller =
        std::make_unique<AppWindowLauncherItemController>(shelf_id);
    item_controller = controller.get();
    if (!owner()->GetItem(shelf_id)) {
      owner()->CreateAppLauncherItem(std::move(controller),
                                     ash::STATUS_RUNNING);
    } else {
      owner()->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                   std::move(controller));
      owner()->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
    }
  }

  item_controller->AddWindow(app_window);
  app_window->SetController(item_controller);
}

void AppServiceAppWindowLauncherController::RemoveFromShelf(
    AppWindowBase* app_window) {
  const ash::ShelfID shelf_id = app_window->shelf_id();
  // Internal Camera app does not have own window. Either ARC or extension
  // window controller would remove window from controller.
  if (shelf_id.app_id == ash::kInternalAppIdCamera)
    return;

  UnregisterAppWindow(app_window);

  // Check if we may close controller now, at this point we can safely remove
  // controllers without window.
  AppWindowLauncherItemController* item_controller =
      owner()->shelf_model()->GetAppWindowLauncherItemController(
          app_window->shelf_id());

  if (item_controller != nullptr && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

AppWindowLauncherItemController*
AppServiceAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  if (!window)
    return nullptr;

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* app_window = app_window_it->second.get();
  if (app_window == nullptr)
    return nullptr;

  return app_window->controller();
}

void AppServiceAppWindowLauncherController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    UnregisterAppWindow(it.second.get());
  }
}

bool AppServiceAppWindowLauncherController::IsOpenedInBrowserTab(
    const std::string& app_id) {
  apps::mojom::AppType app_type = proxy_->AppRegistryCache().GetAppType(app_id);
  if (app_type != apps::mojom::AppType::kExtension &&
      app_type != apps::mojom::AppType::kWeb)
    return false;

  for (auto* browser : *BrowserList::GetInstance()) {
    if (!browser->is_type_app()) {
      continue;
    }
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id)
      return true;
  }
  return false;
}

ash::ShelfID AppServiceAppWindowLauncherController::GetShelfId(
    aura::Window* window) const {
  std::string shelf_app_id = crostini_tracker_->GetShelfAppId(window);
  if (!shelf_app_id.empty())
    return ash::ShelfID(shelf_app_id);

  ash::ShelfID shelf_id;

  // If the window exists in InstanceRegistry, get the shelf id from
  // InstanceRegistry.
  bool exist_in_instance = proxy_->InstanceRegistry().ForOneInstance(
      window, [&shelf_id](const apps::InstanceUpdate& update) {
        shelf_id = ash::ShelfID(update.AppId(), update.LaunchId());
      });
  if (!exist_in_instance) {
    shelf_id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  }

  if (!shelf_id.IsNull()) {
    if (proxy_->AppRegistryCache().GetAppType(shelf_id.app_id) ==
        apps::mojom::AppType::kUnknown) {
      return ash::ShelfID();
    }
    return shelf_id;
  }

  // For null shelf id, it could be VM window or ARC apps window.
  if (plugin_vm::IsPluginVmWindow(window))
    return ash::ShelfID(plugin_vm::kPluginVmAppId);

  return ash::ShelfID();
}
