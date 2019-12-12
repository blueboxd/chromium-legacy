// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_controller.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/app_service_app_window_arc_tracker.h"
#include "chrome/browser/ui/ash/launcher/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_base.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/services/app_service/public/cpp/instance.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "components/arc/arc_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

AppServiceAppWindowLauncherController::AppServiceAppWindowLauncherController(
    ChromeLauncherController* owner)
    : AppWindowLauncherController(owner),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(owner->profile())),
      app_service_instance_helper_(
          std::make_unique<AppServiceInstanceRegistryHelper>(
              owner->profile())) {
  aura::Env::GetInstance()->AddObserver(this);
  DCHECK(proxy_);
  Observe(&proxy_->InstanceRegistry());

  if (arc::IsArcAllowedForProfile(owner->profile()))
    arc_tracker_ = std::make_unique<AppServiceAppWindowArcTracker>(this);

  if (crostini::CrostiniFeatures::Get()->IsUIAllowed(owner->profile()))
    crostini_tracker_ = std::make_unique<AppServiceAppWindowCrostiniTracker>();
}

AppServiceAppWindowLauncherController::
    ~AppServiceAppWindowLauncherController() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

AppWindowLauncherItemController*
AppServiceAppWindowLauncherController::ControllerForWindow(
    aura::Window* window) {
  if (!window)
    return nullptr;

  auto it = aura_window_to_app_window_.find(window);
  if (it == aura_window_to_app_window_.end())
    return nullptr;

  AppWindowBase* const app_window = it->second.get();
  DCHECK(app_window);
  return app_window->controller();
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
    if (app_id == ash::kInternalAppIdKeyboardShortcutViewer)
      continue;

    if (!new_proxy->InstanceRegistry()
             .GetWindows(app_window->shelf_id().app_id)
             .empty()) {
      AddAppWindowToShelf(app_window);
    } else {
      RemoveAppWindowFromShelf(app_window);
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
  if (arc_tracker_)
    arc_tracker_->AddCandidateWindow(window);
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

  RegisterWindow(window, shelf_id);
}

void AppServiceAppWindowLauncherController::OnWindowVisibilityChanging(
    aura::Window* window,
    bool visible) {
  // Skip OnWindowVisibilityChanged for ancestors/descendants.
  if (!observed_windows_.IsObserving(window))
    return;

  if (arc_tracker_)
    arc_tracker_->OnWindowVisibilityChanging(window);

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

  RegisterWindow(window, shelf_id);

  if (crostini_tracker_)
    crostini_tracker_->OnWindowVisibilityChanging(window, shelf_id.app_id);
}

void AppServiceAppWindowLauncherController::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(observed_windows_.IsObserving(window));
  observed_windows_.Remove(window);
  if (arc_tracker_)
    arc_tracker_->RemoveCandidateWindow(window);

  const ash::ShelfID shelf_id = GetShelfId(window);
  if (shelf_id.IsNull())
    return;

  // Delete the instance from InstanceRegistry.
  app_service_instance_helper_->OnInstances(
      shelf_id.app_id, window, std::string(), apps::InstanceState::kDestroyed);

  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;

  RemoveAppWindowFromShelf(app_window_it->second.get());

  if (!shelf_id.IsNull() && crostini_tracker_)
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

void AppServiceAppWindowLauncherController::UnregisterWindow(
    aura::Window* window) {
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it == aura_window_to_app_window_.end())
    return;
  UnregisterAppWindow(app_window_it->second.get());
}

void AppServiceAppWindowLauncherController::AddWindowToShelf(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  if (base::Contains(aura_window_to_app_window_, window))
    return;

  auto app_window_ptr = std::make_unique<AppWindowBase>(
      shelf_id, views::Widget::GetWidgetForNativeWindow(window));
  AppWindowBase* app_window = app_window_ptr.get();
  aura_window_to_app_window_[window] = std::move(app_window_ptr);

  AddAppWindowToShelf(app_window);
}

void AppServiceAppWindowLauncherController::SetWindowActivated(
    aura::Window* window,
    bool active) {
  if (!window)
    return;

  const ash::ShelfID shelf_id = GetShelfId(window);
  if (shelf_id.IsNull())
    return;

  apps::InstanceState state = apps::InstanceState::kUnknown;
  if (active) {
    // If the app is active, it should be started, running, and visible.
    state = static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
  } else {
    proxy_->InstanceRegistry().ForOneInstance(
        window, [&state](const apps::InstanceUpdate& update) {
          state = update.State();
        });

    // When sets the instance active state, the instance should be in started
    // and running state.
    state = static_cast<apps::InstanceState>(
        state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);

    state =
        static_cast<apps::InstanceState>(state & ~apps::InstanceState::kActive);
  }
  app_service_instance_helper_->OnInstances(shelf_id.app_id, window,
                                            std::string(), state);
}

void AppServiceAppWindowLauncherController::RegisterWindow(
    aura::Window* window,
    const ash::ShelfID& shelf_id) {
  // Skip when this window has been handled. This can happen when the window
  // becomes visible again.
  auto app_window_it = aura_window_to_app_window_.find(window);
  if (app_window_it != aura_window_to_app_window_.end())
    return;

  // For Web apps, we don't need to register an app window, because
  // BrowserShortcutLauncherItemController sets the window's property. If
  // register app window for the app opened in a browser tab, the window is
  // added to aura_window_to_app_window_, and when the window is destroyed, it
  // could cause crash in RemoveFromShelf, because
  // BrowserShortcutLauncherItemController manages the window, and sets
  // related window properties, so it could cause the conflict settings.
  if (app_service_instance_helper_->IsWebApp(shelf_id.app_id))
    return;

  if (arc_tracker_)
    arc_tracker_->AttachControllerToWindow(window);

  AddWindowToShelf(window, shelf_id);
}

void AppServiceAppWindowLauncherController::UnregisterAppWindow(
    AppWindowBase* app_window) {
  if (!app_window)
    return;

  AppWindowLauncherItemController* const controller = app_window->controller();
  if (controller)
    controller->RemoveWindow(app_window);

  app_window->SetController(nullptr);
}

void AppServiceAppWindowLauncherController::AddAppWindowToShelf(
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
        std::make_unique<AppServiceAppWindowLauncherItemController>(shelf_id);
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

void AppServiceAppWindowLauncherController::RemoveAppWindowFromShelf(
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

  if (item_controller && item_controller->window_count() == 0)
    owner()->CloseLauncherItem(item_controller->shelf_id());
}

void AppServiceAppWindowLauncherController::OnItemDelegateDiscarded(
    ash::ShelfItemDelegate* delegate) {
  for (auto& it : aura_window_to_app_window_) {
    AppWindowBase* app_window = it.second.get();
    if (!app_window || app_window->controller() != delegate)
      continue;

    VLOG(1) << "Item controller was released externally for the app "
            << delegate->shelf_id().app_id << ".";

    if (arc_tracker_)
      arc_tracker_->OnItemDelegateDiscarded(app_window->shelf_id());

    UnregisterAppWindow(it.second.get());
  }
}

ash::ShelfID AppServiceAppWindowLauncherController::GetShelfId(
    aura::Window* window) const {
  if (crostini_tracker_) {
    std::string shelf_app_id;
    shelf_app_id = crostini_tracker_->GetShelfAppId(window);
    if (!shelf_app_id.empty())
      return ash::ShelfID(shelf_app_id);
  }

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
            apps::mojom::AppType::kUnknown &&
        shelf_id.app_id != extension_misc::kChromeAppId) {
      return ash::ShelfID();
    }
    return shelf_id;
  }

  // For null shelf id, it could be VM window or ARC apps window.
  if (plugin_vm::IsPluginVmWindow(window))
    return ash::ShelfID(plugin_vm::kPluginVmAppId);

  if (arc_tracker_)
    return arc_tracker_->GetShelfId(arc::GetWindowTaskId(window));

  return shelf_id;
}
