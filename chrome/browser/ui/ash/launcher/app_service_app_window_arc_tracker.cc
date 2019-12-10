// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_arc_tracker.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/app_service_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr size_t kMaxIconPngSize = 64 * 1024;  // 64 kb

}  // namespace

// The information about the ARC application window which has to be kept
// even when its AppWindow is not present.
class AppServiceAppWindowArcTracker::ArcAppWindowInfo {
 public:
  ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                   const std::string& launch_intent,
                   const std::string& package_name)
      : app_shelf_id_(app_shelf_id),
        launch_intent_(launch_intent),
        package_name_(package_name) {}
  ~ArcAppWindowInfo() = default;

  ArcAppWindowInfo(const ArcAppWindowInfo&) = delete;
  ArcAppWindowInfo& operator=(const ArcAppWindowInfo&) = delete;

  void SetDescription(const std::string& title,
                      const std::vector<uint8_t>& icon_data_png) {
    DCHECK(base::IsStringUTF8(title));
    title_ = title;

    // Chrome has custom Play Store icon. Don't overwrite it.
    if (app_shelf_id_.app_id() == arc::kPlayStoreAppId)
      return;
    if (icon_data_png.size() < kMaxIconPngSize)
      icon_data_png_ = icon_data_png;
    else
      VLOG(1) << "Task icon size is too big " << icon_data_png.size() << ".";
  }

  void set_window(aura::Window* window) { window_ = window; }

  aura::Window* window() { return window_; }

  const arc::ArcAppShelfId& app_shelf_id() const { return app_shelf_id_; }

  const ash::ShelfID shelf_id() const {
    return ash::ShelfID(app_shelf_id_.app_id());
  }

  const std::string& launch_intent() const { return launch_intent_; }

  const std::string& package_name() const { return package_name_; }

  const std::string& title() const { return title_; }

  const std::vector<uint8_t>& icon_data_png() const { return icon_data_png_; }

 private:
  const arc::ArcAppShelfId app_shelf_id_;
  const std::string launch_intent_;
  const std::string package_name_;
  // Keeps overridden window title.
  std::string title_;
  // Keeps overridden window icon.
  std::vector<uint8_t> icon_data_png_;

  aura::Window* window_ = nullptr;
};

AppServiceAppWindowArcTracker::AppServiceAppWindowArcTracker(
    AppServiceAppWindowLauncherController* app_service_controller)
    : observed_profile_(app_service_controller->owner()->profile()),
      app_service_controller_(app_service_controller) {
  DCHECK(observed_profile_);
  DCHECK(app_service_controller_);

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->AddObserver(this);
}

AppServiceAppWindowArcTracker::~AppServiceAppWindowArcTracker() {
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(observed_profile_);
  DCHECK(prefs);
  prefs->RemoveObserver(this);
}

void AppServiceAppWindowArcTracker::OnWindowVisibilityChanging(
    aura::Window* window) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId || task_id == arc::kSystemWindowTaskId)
    return;

  // Attach window to multi-user manager now to let it manage visibility state
  // of the ARC window correctly.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId());
}

void AppServiceAppWindowArcTracker::OnTaskCreated(
    int task_id,
    const std::string& package_name,
    const std::string& activity_name,
    const std::string& intent) {
  DCHECK(task_id_to_arc_app_window_info_.find(task_id) ==
         task_id_to_arc_app_window_info_.end());

  const std::string arc_app_id =
      ArcAppListPrefs::GetAppId(package_name, activity_name);
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromIntentAndAppId(intent, arc_app_id);
  task_id_to_arc_app_window_info_[task_id] = std::make_unique<ArcAppWindowInfo>(
      arc_app_shelf_id, intent, package_name);

  CheckAndAttachControllers();

  // TODO(crbug.com/1011235): Add AttachControllerToTask to handle tasks started
  // in background.

  aura::Window* const window =
      task_id_to_arc_app_window_info_[task_id]->window();
  if (!window)
    return;

  // If we found the window, update AppService InstanceRegistry to add the
  // window information.
  // Update |state|. The app must be started, and running state. If visible,
  // set it as |kVisible|, otherwise, clear the visible bit.
  apps::InstanceState state = apps::InstanceState::kUnknown;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(observed_profile_);
  proxy->InstanceRegistry().ForOneInstance(
      window,
      [&state](const apps::InstanceUpdate& update) { state = update.State(); });
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  app_service_controller_->app_service_instance_helper()->OnInstances(
      arc_app_id, window, std::string(), state);
}

void AppServiceAppWindowArcTracker::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);
  info->SetDescription(label, icon_png_data);

  // TODO(crbug.com/1011235): Set title and image
}

void AppServiceAppWindowArcTracker::OnTaskDestroyed(int task_id) {
  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  aura::Window* const window = it->second.get()->window();
  if (window)
    app_service_controller_->UnregisterWindow(window);
  task_id_to_arc_app_window_info_.erase(it);
}

void AppServiceAppWindowArcTracker::OnTaskSetActive(int32_t task_id) {
  if (observed_profile_ != app_service_controller_->owner()->profile()) {
    active_task_id_ = task_id;
    return;
  }

  if (task_id == active_task_id_)
    return;

  auto it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it != task_id_to_arc_app_window_info_.end()) {
    ArcAppWindowInfo* const previous_arc_app_window_info = it->second.get();
    DCHECK(previous_arc_app_window_info);
    app_service_controller_->owner()->SetItemStatus(
        previous_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);
    // TODO(crbug.com/1011235): Set previous window full screen mode.
  }

  active_task_id_ = task_id;
  it = task_id_to_arc_app_window_info_.find(active_task_id_);
  if (it == task_id_to_arc_app_window_info_.end())
    return;
  ArcAppWindowInfo* const current_arc_app_window_info = it->second.get();
  if (!current_arc_app_window_info || !current_arc_app_window_info->window())
    return;
  aura::Window* const window = current_arc_app_window_info->window();
  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  if (widget && widget->IsActive()) {
    auto* controller = app_service_controller_->ControllerForWindow(window);
    if (controller)
      controller->SetActiveWindow(window);
  }
  app_service_controller_->owner()->SetItemStatus(
      current_arc_app_window_info->shelf_id(), ash::STATUS_RUNNING);
}

void AppServiceAppWindowArcTracker::AttachControllerToWindow(
    aura::Window* window) {
  const int task_id = arc::GetWindowTaskId(window);
  if (task_id == arc::kNoTaskId)
    return;

  // System windows are also arc apps.
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::ARC_APP));

  if (task_id == arc::kSystemWindowTaskId)
    return;

  auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return;

  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  ArcAppWindowInfo* const info = it->second.get();
  DCHECK(info);

  // Check if we have set the window for this task.
  if (info->window())
    return;

  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  info->set_window(window);
  const ash::ShelfID shelf_id = info->shelf_id();
  app_service_controller_->AddWindowToShelf(window, shelf_id);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kArcPackageNameKey,
                      new std::string(info->package_name()));
  window->SetProperty(ash::kAppIDKey, new std::string(shelf_id.app_id));

  if (info->app_shelf_id().app_id() == arc::kPlayStoreAppId)
    HandlePlayStoreLaunch(info);
}

void AppServiceAppWindowArcTracker::AddCandidateWindow(aura::Window* window) {
  arc_window_candidates_.insert(window);
}

void AppServiceAppWindowArcTracker::RemoveCandidateWindow(
    aura::Window* window) {
  arc_window_candidates_.erase(window);
}

ash::ShelfID AppServiceAppWindowArcTracker::GetShelfId(int task_id) const {
  const auto it = task_id_to_arc_app_window_info_.find(task_id);
  if (it == task_id_to_arc_app_window_info_.end())
    return ash::ShelfID();

  return it->second->shelf_id();
}

void AppServiceAppWindowArcTracker::CheckAndAttachControllers() {
  for (auto* window : arc_window_candidates_)
    AttachControllerToWindow(window);
}

void AppServiceAppWindowArcTracker::OnArcOptInManagementCheckStarted() {
  // In case of retry this time is updated and we measure only successful run.
  opt_in_management_check_start_time_ = base::Time::Now();
}

void AppServiceAppWindowArcTracker::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  opt_in_management_check_start_time_ = base::Time();
}

void AppServiceAppWindowArcTracker::HandlePlayStoreLaunch(
    ArcAppWindowInfo* app_window_info) {
  arc::Intent intent;
  if (!arc::ParseIntent(app_window_info->launch_intent(), &intent))
    return;

  if (!opt_in_management_check_start_time_.is_null()) {
    if (intent.HasExtraParam(arc::kInitialStartParam)) {
      DCHECK(!arc::IsRobotOrOfflineDemoAccountMode());
      arc::UpdatePlayStoreShownTimeDeprecated(
          base::Time::Now() - opt_in_management_check_start_time_,
          app_service_controller_->owner()->profile());
      VLOG(1) << "Play Store is initially shown.";
    }
    opt_in_management_check_start_time_ = base::Time();
    return;
  }

  for (const auto& param : intent.extra_params()) {
    int64_t start_request_ms;
    if (sscanf(param.c_str(), arc::kRequestStartTimeParamTemplate,
               &start_request_ms) != 1)
      continue;
    const base::TimeDelta launch_time =
        base::TimeTicks::Now() - base::TimeTicks() -
        base::TimeDelta::FromMilliseconds(start_request_ms);
    DCHECK_GE(launch_time, base::TimeDelta());
    arc::UpdatePlayStoreLaunchTime(launch_time);
  }
}
