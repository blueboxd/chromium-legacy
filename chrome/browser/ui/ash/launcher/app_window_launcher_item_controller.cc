// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/base_window.h"
#include "ui/wm/core/window_util.h"

AppWindowLauncherItemController::AppWindowLauncherItemController(
    const ash::ShelfID& shelf_id)
    : ash::ShelfItemDelegate(shelf_id) {}

AppWindowLauncherItemController::~AppWindowLauncherItemController() {}

void AppWindowLauncherItemController::AddWindow(ui::BaseWindow* app_window) {
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Add(window);
  if (window && window->GetProperty(ash::kHideInShelfKey))
    hidden_windows_.push_front(app_window);
  else
    windows_.push_front(app_window);
  UpdateShelfItemIcon();
}

AppWindowLauncherItemController::WindowList::iterator
AppWindowLauncherItemController::GetFromNativeWindow(aura::Window* window,
                                                     WindowList& list) {
  return std::find_if(list.begin(), list.end(),
                      [window](ui::BaseWindow* base_window) {
                        return base_window->GetNativeWindow() == window;
                      });
}

void AppWindowLauncherItemController::RemoveWindow(ui::BaseWindow* app_window) {
  DCHECK(app_window);
  aura::Window* window = app_window->GetNativeWindow();
  if (window)
    observed_windows_.Remove(window);
  if (app_window == last_active_window_)
    last_active_window_ = nullptr;
  auto iter = std::find(windows_.begin(), windows_.end(), app_window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
  } else {
    iter =
        std::find(hidden_windows_.begin(), hidden_windows_.end(), app_window);
    if (iter != hidden_windows_.end()) {
      hidden_windows_.erase(iter);
    } else {
      NOTREACHED();
      return;
    }
  }
  UpdateShelfItemIcon();
}

ui::BaseWindow* AppWindowLauncherItemController::GetAppWindow(
    aura::Window* window,
    bool include_hidden) {
  auto iter = GetFromNativeWindow(window, windows_);
  if (iter != windows_.end())
    return *iter;
  if (include_hidden) {
    iter = GetFromNativeWindow(window, hidden_windows_);
    if (iter != hidden_windows_.end())
      return *iter;
  }
  return nullptr;
}

void AppWindowLauncherItemController::SetActiveWindow(aura::Window* window) {
  // If the window is hidden, do not set it as last_active_window
  ui::BaseWindow* app_window = GetAppWindow(window, false);
  if (app_window)
    last_active_window_ = app_window;
  UpdateShelfItemIcon();
}

AppWindowLauncherItemController*
AppWindowLauncherItemController::AsAppWindowLauncherItemController() {
  return this;
}

void AppWindowLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (windows_.empty()) {
    std::move(callback).Run(ash::SHELF_ACTION_NONE, {});
    return;
  }

  ui::BaseWindow* window_to_show =
      last_active_window_ ? last_active_window_ : windows_.front();
  // If the event was triggered by a keystroke, we try to advance to the next
  // item if the window we are trying to activate is already active.
  ash::ShelfAction action = ash::SHELF_ACTION_NONE;
  if (windows_.size() >= 1 && window_to_show->IsActive() && event &&
      event->type() == ui::ET_KEY_RELEASED) {
    action = ActivateOrAdvanceToNextAppWindow(window_to_show);
  } else if (windows_.size() <= 1 || source != ash::LAUNCH_FROM_SHELF) {
    action = ShowAndActivateOrMinimize(window_to_show);
  } else {
    // Do nothing if multiple windows are available when launching from shelf -
    // the shelf will show a context menu with available windows.
    action = ash::SHELF_ACTION_NONE;
  }

  std::move(callback).Run(
      action, GetAppMenuItems(event ? event->flags() : ui::EF_NONE));
}

ash::ShelfItemDelegate::AppMenuItems
AppWindowLauncherItemController::GetAppMenuItems(int event_flags) {
  AppMenuItems items;
  base::string16 app_title = LauncherControllerHelper::GetAppTitle(
      ChromeLauncherController::instance()->profile(), app_id());
  for (const auto* it : windows()) {
    aura::Window* window = it->GetNativeWindow();
    auto title = (window && !window->GetTitle().empty()) ? window->GetTitle()
                                                         : app_title;
    gfx::ImageSkia image;
    const gfx::ImageSkia* app_icon = nullptr;
    if (window)
      app_icon = window->GetProperty(aura::client::kAppIconKey);
    if (app_icon && !app_icon->isNull())
      image = *app_icon;
    items.push_back({title, image});
  }
  return items;
}

void AppWindowLauncherItemController::GetContextMenu(
    int64_t display_id,
    GetContextMenuCallback callback) {
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item = controller->GetItem(shelf_id());
  context_menu_ = ShelfContextMenu::Create(controller, item, display_id);
  context_menu_->GetMenuModel(std::move(callback));
}

void AppWindowLauncherItemController::Close() {
  for (auto* window : windows_)
    window->Close();
  for (auto* window : hidden_windows_)
    window->Close();
}

void AppWindowLauncherItemController::ActivateIndexedApp(size_t index) {
  if (index >= windows_.size())
    return;
  auto it = windows_.begin();
  std::advance(it, index);
  ShowAndActivateOrMinimize(*it);
}

void AppWindowLauncherItemController::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == aura::client::kDrawAttentionKey) {
    ash::ShelfItemStatus status;
    // Active windows don't draw attention because the user is looking at them.
    if (window->GetProperty(aura::client::kDrawAttentionKey) &&
        !wm::IsActiveWindow(window)) {
      status = ash::STATUS_ATTENTION;
    } else {
      status = ash::STATUS_RUNNING;
    }
    ChromeLauncherController::instance()->SetItemStatus(shelf_id(), status);
  } else if (key == aura::client::kAppIconKey) {
    UpdateShelfItemIcon();
  } else if (key == ash::kHideInShelfKey) {
    UpdateWindowInLists(window);
  }
}

ui::BaseWindow* AppWindowLauncherItemController::GetLastActiveWindow() {
  if (last_active_window_)
    return last_active_window_;
  if (windows_.empty())
    return nullptr;
  return windows_.front();
}

ash::ShelfAction AppWindowLauncherItemController::ShowAndActivateOrMinimize(
    ui::BaseWindow* app_window) {
  // Either show or minimize windows when shown from the launcher.
  return ChromeLauncherController::instance()->ActivateWindowOrMinimizeIfActive(
      app_window, windows().size() == 1);
}

ash::ShelfAction
AppWindowLauncherItemController::ActivateOrAdvanceToNextAppWindow(
    ui::BaseWindow* window_to_show) {
  WindowList::iterator i(
      std::find(windows_.begin(), windows_.end(), window_to_show));
  if (i != windows_.end()) {
    if (++i != windows_.end())
      window_to_show = *i;
    else
      window_to_show = windows_.front();
  }
  if (window_to_show->IsActive()) {
    // Coming here, only a single window is active. For keyboard activations
    // the window gets animated.
    ash_util::BounceWindow(window_to_show->GetNativeWindow());
  } else {
    return ShowAndActivateOrMinimize(window_to_show);
  }
  return ash::SHELF_ACTION_NONE;
}

void AppWindowLauncherItemController::UpdateShelfItemIcon() {
  // Set the shelf item icon from the kAppIconKey property of the current
  // (or most recently) active window. If there is no valid icon, ask
  // ChromeLauncherController to update the icon.
  const gfx::ImageSkia* app_icon = nullptr;
  ui::BaseWindow* last_active_window = GetLastActiveWindow();
  if (last_active_window && last_active_window->GetNativeWindow()) {
    app_icon = last_active_window->GetNativeWindow()->GetProperty(
        aura::client::kAppIconKey);
  }
  // TODO(khmel): Remove using image_set_by_controller
  if (app_icon && !app_icon->isNull()) {
    set_image_set_by_controller(true);
    ChromeLauncherController::instance()->SetLauncherItemImage(shelf_id(),
                                                               *app_icon);
  } else if (image_set_by_controller()) {
    set_image_set_by_controller(false);
    ChromeLauncherController::instance()->UpdateLauncherItemImage(
        shelf_id().app_id);
  }
}

void AppWindowLauncherItemController::UpdateWindowInLists(
    aura::Window* window) {
  if (window->GetProperty(ash::kHideInShelfKey)) {
    // Hide Window:
    auto it = GetFromNativeWindow(window, windows_);
    if (it != windows_.end()) {
      hidden_windows_.push_front(*it);
      windows_.erase(it);
      UpdateShelfItemIcon();
    }
  } else {
    // Unhide window:
    auto it = GetFromNativeWindow(window, hidden_windows_);
    if (it != hidden_windows_.end()) {
      windows_.push_front(*it);
      hidden_windows_.erase(it);
      UpdateShelfItemIcon();
    }
  }
}

void AppWindowLauncherItemController::ExecuteCommand(bool from_context_menu,
                                                     int64_t command_id,
                                                     int32_t event_flags,
                                                     int64_t display_id) {
  if (from_context_menu && ExecuteContextMenuCommand(command_id, event_flags))
    return;

  ActivateIndexedApp(command_id);
}
