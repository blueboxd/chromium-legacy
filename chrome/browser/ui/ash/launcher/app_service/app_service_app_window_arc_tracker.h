// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_util.h"

namespace arc {
class ArcAppShelfId;
}

namespace ash {
class ShelfItemDelegate;
}

namespace aura {
class window;
}

namespace base {
class Time;
}

class AppServiceAppWindowLauncherController;
class AppServiceAppWindowLauncherItemController;
class ArcAppWindowInfo;
class Profile;

// AppServiceAppWindowArcTracker observes the ArcAppListPrefs to handle ARC app
// window special cases, e.g. task id, closing ARC app windows, etc.
class AppServiceAppWindowArcTracker : public ArcAppListPrefs::Observer,
                                      public arc::ArcSessionManager::Observer {
 public:
  explicit AppServiceAppWindowArcTracker(
      AppServiceAppWindowLauncherController* app_service_controller);
  ~AppServiceAppWindowArcTracker() override;

  AppServiceAppWindowArcTracker(const AppServiceAppWindowArcTracker&) = delete;
  AppServiceAppWindowArcTracker& operator=(
      const AppServiceAppWindowArcTracker&) = delete;

  void ActiveUserChanged(const std::string& user_email);

  // Invoked by controller to notify |window| visibility is changed.
  void OnWindowVisibilityChanged(aura::Window* window);

  // Invoked by controller to notify |window| is destroying.
  void OnWindowDestroying(aura::Window* window);

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;
  void OnTaskCreated(int task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDescriptionUpdated(
      int32_t task_id,
      const std::string& label,
      const std::vector<uint8_t>& icon_png_data) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskSetActive(int32_t task_id) override;

  // Attaches controller and sets window's property when |window| is an ARC
  // window and has the related task id.
  void AttachControllerToWindow(aura::Window* window);

  // Adds the app window to |arc_window_candidates_|.
  void AddCandidateWindow(aura::Window* window);
  // Removes the app window from |arc_window_candidates_|.
  void RemoveCandidateWindow(aura::Window* window);

  // Removes controller from |app_shelf_group_to_controller_map_|.
  void OnItemDelegateDiscarded(const ash::ShelfID& shelf_id,
                               ash::ShelfItemDelegate* delegate);

  ash::ShelfID GetShelfId(int task_id) const;

  int active_task_id() const { return active_task_id_; }

 private:
  using TaskIdToArcAppWindowInfo =
      std::map<int, std::unique_ptr<ArcAppWindowInfo>>;

  // Maps shelf group id to controller. Shelf group id is optional parameter for
  // the Android task. If it is not set, app id is used instead.
  using ShelfGroupToAppControllerMap =
      std::map<arc::ArcAppShelfId, AppServiceAppWindowLauncherItemController*>;

  // Checks |arc_window_candidates_| and attaches controller when they
  // are ARC app windows and have task id.
  void CheckAndAttachControllers();
  void AttachControllerToTask(int taskId);

  // arc::ArcSessionManager::Observer:
  void OnArcOptInManagementCheckStarted() override;
  void OnArcSessionStopped(arc::ArcStopReason stop_reason) override;

  void HandlePlayStoreLaunch(ArcAppWindowInfo* app_window_info);

  std::vector<int> GetTaskIdsForApp(const std::string& arc_app_id) const;

  Profile* const observed_profile_;
  AppServiceAppWindowLauncherController* const app_service_controller_;

  TaskIdToArcAppWindowInfo task_id_to_arc_app_window_info_;
  ShelfGroupToAppControllerMap app_shelf_group_to_controller_map_;

  // ARC app task id could be created after the window initialized.
  // |arc_window_candidates_| is used to record those initialized ARC app
  // windows, which haven't been assigned a task id. When a task id is created,
  // the windows in |arc_window_candidates_| will be checked and attach the task
  // id. Once the window is assigned a task id, the window is removed from
  // |arc_window_candidates_|.
  std::set<aura::Window*> arc_window_candidates_;

  int active_task_id_ = arc::kNoTaskId;

  // The time when the ARC OptIn management check was started. This happens
  // right after user agrees the ToS or in some cases for managed user when ARC
  // starts for the first time. OptIn management check is preceding step before
  // ARC container is actually started.
  base::Time opt_in_management_check_start_time_;
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_SERVICE_APP_SERVICE_APP_WINDOW_ARC_TRACKER_H_
