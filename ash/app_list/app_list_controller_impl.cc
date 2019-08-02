// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_observer.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_delegate_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/voice_interaction_controller.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

bool IsHomeScreenAvailable() {
  return Shell::Get()->home_screen_controller()->IsHomeScreenAvailable();
}

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Close current Assistant UI.
void CloseAssistantUi(AssistantExitPoint exit_point) {
  if (app_list_features::IsEmbeddedAssistantUIEnabled())
    Shell::Get()->assistant_controller()->ui_controller()->CloseUi(exit_point);
}

app_list::TabletModeAnimationTransition CalculateAnimationTransitionForMetrics(
    HomeScreenDelegate::AnimationTrigger trigger,
    bool launcher_should_show) {
  switch (trigger) {
    case HomeScreenDelegate::AnimationTrigger::kHideForWindow:
      return app_list::TabletModeAnimationTransition::
          kHideHomeLauncherForWindow;
    case HomeScreenDelegate::AnimationTrigger::kLauncherButton:
      return app_list::TabletModeAnimationTransition::kHomeButtonShow;
    case HomeScreenDelegate::AnimationTrigger::kDragRelease:
      return launcher_should_show
                 ? app_list::TabletModeAnimationTransition::kDragReleaseShow
                 : app_list::TabletModeAnimationTransition::kDragReleaseHide;
    case HomeScreenDelegate::AnimationTrigger::kOverviewMode:
      return launcher_should_show
                 ? app_list::TabletModeAnimationTransition::kExitOverviewMode
                 : app_list::TabletModeAnimationTransition::kEnterOverviewMode;
  }
}

int GetAssistantPrivacyInfoShownCount() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetInteger(prefs::kAssistantPrivacyInfoShownInLauncher);
}

void SetAssistantPrivacyInfoShownCount(int count) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetInteger(prefs::kAssistantPrivacyInfoShownInLauncher, count);
}

bool IsAssistantPrivacyInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetBoolean(prefs::kAssistantPrivacyInfoDismissedInLauncher);
}

void SetAssistantPrivacyInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kAssistantPrivacyInfoDismissedInLauncher, true);
}
}  // namespace

AppListControllerImpl::AppListControllerImpl()
    : model_(std::make_unique<app_list::AppListModel>()),
      presenter_(std::make_unique<AppListPresenterDelegateImpl>(this)) {
  model_->AddObserver(this);

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_controller->AddObserver(this);

  // In case of crash-and-restart case where session state starts with ACTIVE
  // and does not change to trigger OnSessionStateChanged(), notify the current
  // session state here to ensure that the app list is shown.
  OnSessionStateChanged(session_controller->GetSessionState());

  Shell* shell = Shell::Get();
  shell->tablet_mode_controller()->AddObserver(this);
  shell->wallpaper_controller()->AddObserver(this);
  shell->AddShellObserver(this);
  shell->overview_controller()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  VoiceInteractionController::Get()->AddLocalObserver(this);
  shell->window_tree_host_manager()->AddObserver(this);
  shell->mru_window_tracker()->AddObserver(this);
  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    shell->assistant_controller()->AddObserver(this);
    shell->assistant_controller()->ui_controller()->AddModelObserver(this);
  }
  shell->home_screen_controller()->home_launcher_gesture_handler()->AddObserver(
      this);
}

AppListControllerImpl::~AppListControllerImpl() {
  // If this is being destroyed before the Shell starts shutting down, first
  // remove this from objects it's observing.
  if (!is_shutdown_)
    Shutdown();

  if (client_)
    client_->OnAppListControllerDestroyed();
}

// static
void AppListControllerImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kAssistantPrivacyInfoShownInLauncher, 0);
  registry->RegisterBooleanPref(
      prefs::kAssistantPrivacyInfoDismissedInLauncher, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void AppListControllerImpl::SetClient(app_list::AppListClient* client) {
  client_ = client;
}

app_list::AppListClient* AppListControllerImpl::GetClient() {
  DCHECK(client_);
  return client_;
}

app_list::AppListModel* AppListControllerImpl::GetModel() {
  return model_.get();
}

app_list::SearchModel* AppListControllerImpl::GetSearchModel() {
  return &search_model_;
}

void AppListControllerImpl::AddItem(
    std::unique_ptr<ash::AppListItemMetadata> item_data) {
  const std::string folder_id = item_data->folder_id;
  if (folder_id.empty())
    model_->AddItem(CreateAppListItem(std::move(item_data)));
  else
    AddItemToFolder(std::move(item_data), folder_id);
}

void AppListControllerImpl::AddItemToFolder(
    std::unique_ptr<ash::AppListItemMetadata> item_data,
    const std::string& folder_id) {
  // When we're setting a whole model of a profile, each item may have its
  // folder id set properly. However, |AppListModel::AddItemToFolder| requires
  // the item to add is not in the target folder yet, and sets its folder id
  // later. So we should clear the folder id here to avoid breaking checks.
  item_data->folder_id.clear();
  model_->AddItemToFolder(CreateAppListItem(std::move(item_data)), folder_id);
}

void AppListControllerImpl::RemoveItem(const std::string& id) {
  model_->DeleteItem(id);
}

void AppListControllerImpl::RemoveUninstalledItem(const std::string& id) {
  model_->DeleteUninstalledItem(id);
}

void AppListControllerImpl::MoveItemToFolder(const std::string& id,
                                             const std::string& folder_id) {
  app_list::AppListItem* item = model_->FindItem(id);
  model_->MoveItemToFolder(item, folder_id);
}

void AppListControllerImpl::SetStatus(ash::AppListModelStatus status) {
  model_->SetStatus(status);
}

void AppListControllerImpl::SetState(ash::AppListState state) {
  model_->SetState(state);
}

void AppListControllerImpl::HighlightItemInstalledFromUI(
    const std::string& id) {
  model_->top_level_item_list()->HighlightItemInstalledFromUI(id);
}

void AppListControllerImpl::SetSearchEngineIsGoogle(bool is_google) {
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void AppListControllerImpl::SetSearchTabletAndClamshellAccessibleName(
    const base::string16& tablet_accessible_name,
    const base::string16& clamshell_accessible_name) {
  search_model_.search_box()->SetTabletAndClamshellAccessibleName(
      tablet_accessible_name, clamshell_accessible_name);
}

void AppListControllerImpl::SetSearchHintText(const base::string16& hint_text) {
  search_model_.search_box()->SetHintText(hint_text);
}

void AppListControllerImpl::UpdateSearchBox(const base::string16& text,
                                            bool initiated_by_user) {
  search_model_.search_box()->Update(text, initiated_by_user);
}

void AppListControllerImpl::PublishSearchResults(
    std::vector<std::unique_ptr<ash::SearchResultMetadata>> results) {
  std::vector<std::unique_ptr<app_list::SearchResult>> new_results;
  for (auto& result_metadata : results) {
    std::unique_ptr<app_list::SearchResult> result =
        std::make_unique<app_list::SearchResult>();
    result->SetMetadata(std::move(result_metadata));
    new_results.push_back(std::move(result));
  }
  search_model_.PublishResults(std::move(new_results));
}

void AppListControllerImpl::SetItemMetadata(
    const std::string& id,
    std::unique_ptr<ash::AppListItemMetadata> data) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (!item)
    return;

  // data may not contain valid position or icon. Preserve it in this case.
  if (!data->position.IsValid())
    data->position = item->position();

  // Update the item's position and name based on the metadata.
  if (!data->position.Equals(item->position()))
    model_->SetItemPosition(item, data->position);

  if (data->short_name.empty()) {
    if (data->name != item->name()) {
      model_->SetItemName(item, data->name);
    }
  } else {
    if (data->name != item->name() || data->short_name != item->short_name()) {
      model_->SetItemNameAndShortName(item, data->name, data->short_name);
    }
  }

  // Folder icon is generated on ash side and chrome side passes a null
  // icon here. Skip it.
  if (data->icon.isNull())
    data->icon = item->icon();
  item->SetMetadata(std::move(data));
}

void AppListControllerImpl::SetItemIcon(const std::string& id,
                                        const gfx::ImageSkia& icon) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIcon(icon);
}

void AppListControllerImpl::SetItemIsInstalling(const std::string& id,
                                                bool is_installing) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIsInstalling(is_installing);
}

void AppListControllerImpl::SetItemPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetPercentDownloaded(percent_downloaded);
}

void AppListControllerImpl::SetModelData(
    int profile_id,
    std::vector<std::unique_ptr<ash::AppListItemMetadata>> apps,
    bool is_search_engine_google) {
  // Clear old model data.
  model_->DeleteAllItems();
  search_model_.DeleteAllResults();

  profile_id_ = profile_id;

  // Populate new models. First populate folders and then other items to avoid
  // automatically creating folder items in |AddItemToFolder|.
  for (auto& app : apps) {
    if (!app->is_folder)
      continue;
    DCHECK(app->folder_id.empty());
    AddItem(std::move(app));
  }
  for (auto& app : apps) {
    if (!app)
      continue;
    AddItem(std::move(app));
  }
  search_model_.SetSearchEngineIsGoogle(is_search_engine_google);
}

void AppListControllerImpl::SetSearchResultMetadata(
    std::unique_ptr<ash::SearchResultMetadata> metadata) {
  app_list::SearchResult* result = search_model_.FindSearchResult(metadata->id);
  if (result)
    result->SetMetadata(std::move(metadata));
}

void AppListControllerImpl::SetSearchResultIsInstalling(const std::string& id,
                                                        bool is_installing) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->SetIsInstalling(is_installing);
}

void AppListControllerImpl::SetSearchResultPercentDownloaded(
    const std::string& id,
    int32_t percent_downloaded) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->SetPercentDownloaded(percent_downloaded);
}

void AppListControllerImpl::NotifySearchResultItemInstalled(
    const std::string& id) {
  app_list::SearchResult* result = search_model_.FindSearchResult(id);
  if (result)
    result->NotifyItemInstalled();
}

void AppListControllerImpl::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  base::flat_map<std::string, uint16_t> id_to_app_list_index;
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_->top_level_item_list()->item_at(i)->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

void AppListControllerImpl::FindOrCreateOemFolder(
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position,
    FindOrCreateOemFolderCallback callback) {
  app_list::AppListFolderItem* oem_folder =
      model_->FindFolderItem(kOemFolderId);
  if (!oem_folder) {
    std::unique_ptr<app_list::AppListFolderItem> new_folder =
        std::make_unique<app_list::AppListFolderItem>(kOemFolderId);
    syncer::StringOrdinal oem_position = preferred_oem_position.IsValid()
                                             ? preferred_oem_position
                                             : GetOemFolderPos();
    // Do not create a sync item for the OEM folder here, do it in
    // ResolveFolderPositions() when the item position is finalized.
    oem_folder = static_cast<app_list::AppListFolderItem*>(
        model_->AddItem(std::move(new_folder)));
    model_->SetItemPosition(oem_folder, oem_position);
  }
  model_->SetItemName(oem_folder, oem_folder_name);
  std::move(callback).Run();
}

void AppListControllerImpl::ResolveOemFolderPosition(
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  // In ash:
  app_list::AppListFolderItem* ash_oem_folder = FindFolderItem(kOemFolderId);
  std::unique_ptr<ash::AppListItemMetadata> metadata;
  if (ash_oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preferred_oem_position.IsValid() ? preferred_oem_position
                                         : GetOemFolderPos();
    model_->SetItemPosition(ash_oem_folder, oem_folder_pos);
    metadata = ash_oem_folder->CloneMetadata();
  }
  std::move(callback).Run(std::move(metadata));
}

void AppListControllerImpl::DismissAppList() {
  presenter_.Dismiss(base::TimeTicks());
}

void AppListControllerImpl::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  app_list::AppListView* app_list_view = presenter_.GetView();
  gfx::Rect bounds = gfx::Rect();
  if (app_list_view)
    bounds = app_list_view->GetAppInfoDialogBounds();
  std::move(callback).Run(bounds);
}

void AppListControllerImpl::ShowAppListAndSwitchToState(
    ash::AppListState state) {
  bool app_list_was_open = true;
  if (!presenter_.IsVisible()) {
    // TODO(calamity): This may cause the app list to show briefly before the
    // state change. If this becomes an issue, add the ability to ash::Shell to
    // load the app list without showing it.
    ShowAppList();
    app_list_was_open = false;
  }

  if (state == ash::AppListState::kInvalidState)
    return;

  app_list::ContentsView* contents_view =
      presenter_.GetView()->app_list_main_view()->contents_view();
  contents_view->SetActiveState(state, app_list_was_open /* animate */);
}

void AppListControllerImpl::ShowAppList() {
  presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
}

aura::Window* AppListControllerImpl::GetWindow() {
  return presenter_.GetWindow();
}

////////////////////////////////////////////////////////////////////////////////
// app_list::AppListModelObserver:

void AppListControllerImpl::OnAppListItemAdded(app_list::AppListItem* item) {
  if (item->is_folder())
    client_->OnFolderCreated(profile_id_, item->CloneMetadata());
  else if (item->is_page_break())
    client_->OnPageBreakItemAdded(profile_id_, item->id(), item->position());
}

void AppListControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* /* pref_service */) {
  if (!IsHomeScreenAvailable()) {
    DismissAppList();
    return;
  }

  // Show the app list after signing in in tablet mode.
  Show(GetDisplayIdToShowAppListOn(), app_list::AppListShowSource::kTabletMode,
       base::TimeTicks());

  // The app list is not dismissed before switching user, suggestion chips will
  // not be shown. So reset app list state and trigger an initial search here to
  // update the suggestion results.
  presenter_.GetView()->CloseOpenedPage();
  presenter_.GetView()->search_box_view()->ClearSearch();
}

void AppListControllerImpl::OnAppListItemWillBeDeleted(
    app_list::AppListItem* item) {
  if (!client_)
    return;

  if (item->is_folder())
    client_->OnFolderDeleted(profile_id_, item->CloneMetadata());

  if (item->is_page_break())
    client_->OnPageBreakItemDeleted(profile_id_, item->id());
}

void AppListControllerImpl::OnAppListItemUpdated(app_list::AppListItem* item) {
  if (client_)
    client_->OnItemUpdated(profile_id_, item->CloneMetadata());
}

void AppListControllerImpl::OnAppListStateChanged(ash::AppListState new_state,
                                                  ash::AppListState old_state) {
  if (!app_list_features::IsEmbeddedAssistantUIEnabled())
    return;

  UpdateLauncherContainer();

  if (new_state == ash::AppListState::kStateEmbeddedAssistant) {
    // ShowUi will be no-op if the AssistantUiModel is already visible.
    Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        ash::AssistantEntryPoint::kUnspecified);
    return;
  }

  if (old_state == ash::AppListState::kStateEmbeddedAssistant) {
    // CloseUi will be no-op if the AssistantUiModel is already closed.
    Shell::Get()->assistant_controller()->ui_controller()->CloseUi(
        ash::AssistantExitPoint::kBackInLauncher);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Methods used in Ash

bool AppListControllerImpl::GetTargetVisibility() const {
  return presenter_.GetTargetVisibility();
}

bool AppListControllerImpl::IsVisible() const {
  return presenter_.IsVisible();
}

void AppListControllerImpl::Show(int64_t display_id,
                                 app_list::AppListShowSource show_source,
                                 base::TimeTicks event_time_stamp) {
  UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                            show_source);

  presenter_.Show(display_id, event_time_stamp);

  // AppListControllerImpl::Show is called in ash at the first time of showing
  // app list view. So check whether the expand arrow view should be visible.
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::UpdateYPositionAndOpacity(
    int y_position_in_screen,
    float background_opacity) {
  // Avoid changing app list opacity and position when homecher is enabled.
  if (IsHomeScreenAvailable())
    return;
  presenter_.UpdateYPositionAndOpacity(y_position_in_screen,
                                       background_opacity);
}

void AppListControllerImpl::EndDragFromShelf(
    ash::AppListViewState app_list_state) {
  // Avoid dragging app list when homecher is enabled.
  if (IsHomeScreenAvailable())
    return;
  presenter_.EndDragFromShelf(app_list_state);
}

void AppListControllerImpl::ProcessMouseWheelEvent(
    const ui::MouseWheelEvent& event) {
  presenter_.ProcessMouseWheelOffset(event.offset());
}

ash::ShelfAction AppListControllerImpl::ToggleAppList(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  ash::ShelfAction action =
      presenter_.ToggleAppList(display_id, show_source, event_time_stamp);
  UpdateExpandArrowVisibility();
  if (action == SHELF_ACTION_APP_LIST_SHOWN) {
    UMA_HISTOGRAM_ENUMERATION(app_list::kAppListToggleMethodHistogram,
                              show_source);
  }
  return action;
}

ash::AppListViewState AppListControllerImpl::GetAppListViewState() {
  return model_->state_fullscreen();
}

void AppListControllerImpl::OnShelfAlignmentChanged(aura::Window* root_window) {
  if (!IsHomeScreenAvailable())
    DismissAppList();
}

void AppListControllerImpl::OnShellDestroying() {
  // Stop observing at the beginning of ~Shell to avoid unnecessary work during
  // Shell shutdown.
  Shutdown();
}

void AppListControllerImpl::OnOverviewModeStarting() {
  if (!IsHomeScreenAvailable())
    DismissAppList();
}

void AppListControllerImpl::OnTabletModeStarted() {
  presenter_.OnTabletModeChanged(true);

  // Show the app list if the tablet mode starts.
  Shell::Get()->home_screen_controller()->Show();
  UpdateLauncherContainer();
}

void AppListControllerImpl::OnTabletModeEnded() {
  aura::Window* window = presenter_.GetWindow();
  base::AutoReset<bool> auto_reset(
      &should_dismiss_immediately_,
      window && RootWindowController::ForWindow(window)
                    ->GetShelfLayoutManager()
                    ->HasVisibleWindow());
  presenter_.OnTabletModeChanged(false);

  // Dismiss the app list if the tablet mode ends.
  DismissAppList();
  UpdateLauncherContainer();
}

void AppListControllerImpl::OnWallpaperColorsChanged() {
  if (IsVisible())
    presenter_.GetView()->OnWallpaperColorsChanged();
}

void AppListControllerImpl::OnKeyboardVisibilityChanged(const bool is_visible) {
  onscreen_keyboard_shown_ = is_visible;
  app_list::AppListView* app_list_view = presenter_.GetView();
  if (app_list_view)
    app_list_view->OnScreenKeyboardShown(is_visible);
}

void AppListControllerImpl::OnVoiceInteractionStatusChanged(
    mojom::VoiceInteractionState state) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnVoiceInteractionSettingsEnabled(bool enabled) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnAssistantFeatureAllowedChanged(
    mojom::AssistantAllowedState state) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnDisplayConfigurationChanged() {
  // Entering tablet mode triggers a display configuration change when we
  // automatically switch to mirror mode. Switching to mirror mode happens
  // asynchronously (see DisplayConfigurationObserver::OnTabletModeStarted()).
  // This may result in the removal of a window tree host, as in the example of
  // switching to tablet mode while Unified Desktop mode is on; the Unified host
  // will be destroyed and the Home Launcher (which was created earlier when we
  // entered tablet mode) will be dismissed.
  // To avoid crashes, we must ensure that the Home Launcher shown status is as
  // expected if it's enabled and we're still in tablet mode.
  // https://crbug.com/900956.
  const bool should_be_shown = IsTabletMode();
  DCHECK_EQ(should_be_shown, IsHomeScreenAvailable());
  if (should_be_shown == GetTargetVisibility())
    return;

  if (should_be_shown)
    Shell::Get()->home_screen_controller()->Show();
}

void AppListControllerImpl::OnWindowUntracked(aura::Window* untracked_window) {
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::OnAssistantReady() {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  switch (new_visibility) {
    case AssistantVisibility::kVisible:
      if (!IsVisible()) {
        Show(GetDisplayIdToShowAppListOn(), app_list::kAssistantEntryPoint,
             base::TimeTicks());
      }

      if (!IsShowingEmbeddedAssistantUI())
        presenter_.ShowEmbeddedAssistantUI(true);
      break;
    case AssistantVisibility::kHidden:
      NOTREACHED();
      break;
    case AssistantVisibility::kClosed:
      if (!IsShowingEmbeddedAssistantUI())
        break;

      // When Launcher is closing, we do not want to call
      // |ShowEmbeddedAssistantUI(false)|, which will show previous state page
      // in Launcher and make the UI flash.
      if (IsHomeScreenAvailable()) {
        base::Optional<
            app_list::ContentsView::ScopedSetActiveStateAnimationDisabler>
            set_active_state_animation_disabler;
        // When taking a screenshot by Assistant, we do not want to animate to
        // the final state. Otherwise the screenshot may have tansient state
        // during the animation. In tablet mode, we want to go back to
        // kStateApps immediately, i.e. skipping the animation in
        // |SetActiveStateInternal|, which are called from
        // |ShowEmbeddedAssistantUI(false)| and
        // |ClearSearchAndDeactivateSearchBox()|.
        if (exit_point == AssistantExitPoint::kScreenshot) {
          set_active_state_animation_disabler.emplace(
              presenter_.GetView()->app_list_main_view()->contents_view());
        }

        presenter_.ShowEmbeddedAssistantUI(false);

        if (exit_point != AssistantExitPoint::kBackInLauncher) {
          presenter_.GetView()
              ->search_box_view()
              ->ClearSearchAndDeactivateSearchBox();
        }
      } else if (exit_point != AssistantExitPoint::kBackInLauncher) {
        // Similarly, when taking a screenshot by Assistant in clamshell mode,
        // we do not want to dismiss launcher with animation. Otherwise the
        // screenshot may have tansient state during the animation.
        base::AutoReset<bool> auto_reset(
            &should_dismiss_immediately_,
            exit_point == AssistantExitPoint::kScreenshot);
        DismissAppList();
      }

      break;
  }
}

void AppListControllerImpl::OnHomeLauncherAnimationComplete(
    bool shown,
    int64_t display_id) {
  CloseAssistantUi(shown ? AssistantExitPoint::kLauncherOpen
                         : AssistantExitPoint::kLauncherClose);
}

void AppListControllerImpl::ShowHomeScreenView() {
  DCHECK(IsTabletMode());

  Show(GetDisplayIdToShowAppListOn(), app_list::kTabletMode, base::TimeTicks());
}

aura::Window* AppListControllerImpl::GetHomeScreenWindow() {
  return presenter_.GetWindow();
}

void AppListControllerImpl::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  presenter_.UpdateYPositionAndOpacityForHomeLauncher(
      y_position_in_screen, opacity, std::move(callback));
}

void AppListControllerImpl::UpdateAfterHomeLauncherShown() {
  // Show or hide the expand arrow view.
  UpdateExpandArrowVisibility();
}

base::Optional<base::TimeDelta>
AppListControllerImpl::GetOptionalAnimationDuration() {
  if (model_->state() == ash::AppListState::kStateEmbeddedAssistant) {
    // If Assistant is shown, we don't want any delay in animation transitions
    // since the launcher is already shown.
    return base::TimeDelta::Min();
  }
  return base::nullopt;
}

void AppListControllerImpl::Back() {
  presenter_.GetView()->Back();
}

void AppListControllerImpl::SetKeyboardTraversalMode(bool engaged) {
  if (keyboard_traversal_engaged_ == engaged)
    return;

  keyboard_traversal_engaged_ = engaged;

  views::View* focused_view =
      presenter_.GetView()->GetFocusManager()->GetFocusedView();

  if (!focused_view)
    return;

  // When the search box has focus, it is actually the textfield that has focus.
  // As such, the |SearchBoxView| must be told to repaint directly.
  if (focused_view == presenter_.GetView()->search_box_view()->search_box())
    presenter_.GetView()->search_box_view()->SchedulePaint();
  else
    focused_view->SchedulePaint();
}

ash::ShelfAction AppListControllerImpl::OnHomeButtonPressed(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  if (!IsHomeScreenAvailable())
    return ToggleAppList(display_id, show_source, event_time_stamp);

  bool handled = Shell::Get()->home_screen_controller()->GoHome(display_id);

  // Perform the "back" action for the app list.
  if (!handled)
    Back();

  return ash::SHELF_ACTION_APP_LIST_SHOWN;
}

bool AppListControllerImpl::IsShowingEmbeddedAssistantUI() const {
  return presenter_.IsShowingEmbeddedAssistantUI();
}

void AppListControllerImpl::UpdateExpandArrowVisibility() {
  bool should_show = false;

  // Hide the expand arrow view when the home screen is available and there is
  // no activatable window on the current active desk.
  if (IsHomeScreenAvailable()) {
    should_show = !ash::Shell::Get()
                       ->mru_window_tracker()
                       ->BuildWindowForCycleList(kActiveDesk)
                       .empty();
  } else {
    should_show = true;
  }

  presenter_.SetExpandArrowViewVisibility(should_show);
}

ash::AppListViewState AppListControllerImpl::CalculateStateAfterShelfDrag(
    const ui::LocatedEvent& event_in_screen,
    float launcher_above_shelf_bottom_amount) const {
  if (presenter_.GetView())
    return presenter_.GetView()->CalculateStateAfterShelfDrag(
        event_in_screen, launcher_above_shelf_bottom_amount);
  return ash::AppListViewState::kClosed;
}

void AppListControllerImpl::SetAppListModelForTest(
    std::unique_ptr<app_list::AppListModel> model) {
  model_->RemoveObserver(this);
  model_ = std::move(model);
  model_->AddObserver(this);
}

void AppListControllerImpl::SetStateTransitionAnimationCallback(
    StateTransitionAnimationCallback callback) {
  state_transition_animation_callback_ = std::move(callback);
}

void AppListControllerImpl::RecordShelfAppLaunched(
    base::Optional<AppListViewState> recorded_app_list_view_state,
    base::Optional<bool> recorded_home_launcher_shown) {
  app_list::RecordAppListAppLaunched(
      AppListLaunchedFrom::kLaunchedFromShelf,
      recorded_app_list_view_state.value_or(GetAppListViewState()),
      IsTabletMode(),
      recorded_home_launcher_shown.value_or(presenter_.home_launcher_shown()));
}

////////////////////////////////////////////////////////////////////////////////
// Methods of |client_|:

void AppListControllerImpl::StartAssistant() {
  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    ash::Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        ash::AssistantEntryPoint::kLauncherSearchBoxMic);
    return;
  }

  if (!IsHomeScreenAvailable())
    DismissAppList();

  ash::Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
      ash::AssistantEntryPoint::kLauncherSearchBox);
}

void AppListControllerImpl::StartSearch(const base::string16& raw_query) {
  if (client_) {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->StartSearch(query);
  }
}

void AppListControllerImpl::OpenSearchResult(const std::string& result_id,
                                             int event_flags,
                                             AppListLaunchedFrom launched_from,
                                             AppListLaunchType launch_type,
                                             int suggestion_index) {
  app_list::SearchResult* result = search_model_.FindSearchResult(result_id);
  if (!result)
    return;

  if (launch_type == AppListLaunchType::kAppSearchResult) {
    switch (launched_from) {
      case AppListLaunchedFrom::kLaunchedFromSearchBox:
      case AppListLaunchedFrom::kLaunchedFromSuggestionChip:
        RecordAppLaunched(launched_from);
        break;
      case AppListLaunchedFrom::kLaunchedFromGrid:
      case AppListLaunchedFrom::kLaunchedFromShelf:
        break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(app_list::kSearchResultOpenDisplayTypeHistogram,
                            result->display_type(),
                            ash::SearchResultDisplayType::kLast);

  // Suggestion chips are not represented to the user as search results, so do
  // not record search result metrics for them.
  if (launched_from != AppListLaunchedFrom::kLaunchedFromSuggestionChip) {
    base::RecordAction(base::UserMetricsAction("AppList_OpenSearchResult"));

    UMA_HISTOGRAM_COUNTS_100(app_list::kSearchQueryLength,
                             GetLastQueryLength());
    if (IsTabletMode()) {
      UMA_HISTOGRAM_COUNTS_100(app_list::kSearchQueryLengthInTablet,
                               GetLastQueryLength());
    } else {
      UMA_HISTOGRAM_COUNTS_100(app_list::kSearchQueryLengthInClamshell,
                               GetLastQueryLength());
    }

    if (result->distance_from_origin() >= 0) {
      UMA_HISTOGRAM_COUNTS_100(app_list::kSearchResultDistanceFromOrigin,
                               result->distance_from_origin());
    }
  }

  if (presenter_.IsVisible() && result->is_omnibox_search() &&
      IsAssistantAllowedAndEnabled() &&
      app_list_features::IsEmbeddedAssistantUIEnabled()) {
    // Record the assistant result. Other types of results are recorded in
    // |client_| where there is richer data on SearchResultType.
    DCHECK_EQ(AppListLaunchedFrom::kLaunchedFromSearchBox, launched_from)
        << "Only log search results which are represented to the user as "
           "search results (ie. search results in the search result page) not "
           "chips.";
    app_list::RecordSearchResultOpenTypeHistogram(
        launched_from, app_list::ASSISTANT_OMNIBOX_RESULT, IsTabletMode());
    if (!GetLastQueryLength()) {
      app_list::RecordZeroStateSuggestionOpenTypeHistogram(
          app_list::ASSISTANT_OMNIBOX_RESULT);
    }
    Shell::Get()->assistant_controller()->ui_controller()->ShowUi(
        AssistantEntryPoint::kLauncherSearchResult);
    Shell::Get()->assistant_controller()->OpenUrl(
        ash::assistant::util::CreateAssistantQueryDeepLink(
            base::UTF16ToUTF8(result->title())));
  } else {
    if (client_)
      client_->OpenSearchResult(result_id, event_flags, launched_from,
                                launch_type, suggestion_index);
  }

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::LogResultLaunchHistogram(
    app_list::SearchResultLaunchLocation launch_location,
    int suggestion_index) {
  app_list::RecordSearchLaunchIndexAndQueryLength(
      launch_location, GetLastQueryLength(), suggestion_index);
}

void AppListControllerImpl::LogSearchAbandonHistogram() {
  app_list::RecordSearchAbandonWithQueryLengthHistogram(GetLastQueryLength());
}

void AppListControllerImpl::InvokeSearchResultAction(
    const std::string& result_id,
    int action_index,
    int event_flags) {
  if (client_)
    client_->InvokeSearchResultAction(result_id, action_index, event_flags);
}

void AppListControllerImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetSearchResultContextMenuModel(result_id, std::move(callback));
}

void AppListControllerImpl::ViewShown(int64_t display_id) {
  if (app_list_features::IsEmbeddedAssistantUIEnabled() &&
      GetAssistantViewDelegate()->GetUiModel()->ui_mode() !=
          ash::AssistantUiMode::kLauncherEmbeddedUi) {
    CloseAssistantUi(AssistantExitPoint::kLauncherOpen);
  }
  UpdateAssistantVisibility();
  if (client_)
    client_->ViewShown(display_id);

  // Ensure search box starts fresh with no ring each time it opens.
  keyboard_traversal_engaged_ = false;
}

void AppListControllerImpl::ViewClosing() {
  if (presenter_.GetView()->search_box_view()->is_search_box_active()) {
    // Close the virtual keyboard before the app list view is dismissed.
    // Otherwise if the browser is behind the app list view, after the latter is
    // closed, IME is updated because of the changed focus. Consequently,
    // the virtual keyboard is hidden for the wrong IME instance, which may
    // bring troubles when restoring the virtual keyboard (see
    // https://crbug.com/944233).
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
  }

  CloseAssistantUi(AssistantExitPoint::kLauncherClose);

  if (client_)
    client_->ViewClosing();
}

void AppListControllerImpl::ViewClosed() {
  // Clear results to prevent initializing the next app list view with outdated
  // results.
  if (client_)
    client_->StartSearch(base::string16());
}

const std::vector<SkColor>&
AppListControllerImpl::GetWallpaperProminentColors() {
  return Shell::Get()->wallpaper_controller()->GetWallpaperColors();
}

void AppListControllerImpl::ActivateItem(const std::string& id,
                                         int event_flags,
                                         AppListLaunchedFrom launched_from) {
  RecordAppLaunched(launched_from);

  if (client_)
    client_->ActivateItem(profile_id_, id, event_flags);

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetContextMenuModel(profile_id_, id, std::move(callback));
}

ui::ImplicitAnimationObserver* AppListControllerImpl::GetAnimationObserver(
    ash::AppListViewState target_state) {
  // |presenter_| observes the close animation only.
  if (target_state == ash::AppListViewState::kClosed)
    return &presenter_;
  return nullptr;
}

void AppListControllerImpl::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(onscreen_location, source_type);
}

bool AppListControllerImpl::ProcessHomeLauncherGesture(
    ui::GestureEvent* event,
    const gfx::Point& screen_location) {
  HomeLauncherGestureHandler* home_launcher_gesture_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return home_launcher_gesture_handler->OnPressEvent(
          HomeLauncherGestureHandler::Mode::kSlideDownToHide, screen_location);
    case ui::ET_GESTURE_SCROLL_UPDATE:
      return home_launcher_gesture_handler->OnScrollEvent(
          screen_location, event->details().scroll_y());
    case ui::ET_GESTURE_END:
      return home_launcher_gesture_handler->OnReleaseEvent(screen_location);
    default:
      break;
  }

  NOTREACHED();
  return false;
}

bool AppListControllerImpl::KeyboardTraversalEngaged() {
  return keyboard_traversal_engaged_;
}

bool AppListControllerImpl::CanProcessEventsOnApplistViews() {
  // Do not allow processing events during overview or while overview is
  // finished but still animating out. Note in clamshell mode, if overview and
  // splitview is both active, we still allow the user to open app list and
  // select an app. The app will be opened in snapped window state and overview
  // will be ended after the app is opened.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  auto* split_view_controller = Shell::Get()->split_view_controller();
  if ((overview_controller->InOverviewSession() &&
       !split_view_controller->InClamshellSplitViewMode()) ||
      overview_controller->IsCompletingShutdownAnimations()) {
    return false;
  }

  HomeScreenController* home_screen_controller =
      Shell::Get()->home_screen_controller();
  return home_screen_controller &&
         home_screen_controller->home_launcher_gesture_handler()->mode() !=
             HomeLauncherGestureHandler::Mode::kSlideUpToShow;
}

bool AppListControllerImpl::ShouldDismissImmediately() {
  if (should_dismiss_immediately_)
    return true;

  DCHECK(Shell::HasInstance());
  const int ideal_shelf_y =
      Shelf::ForWindow(presenter_.GetView()->GetWidget()->GetNativeView())
          ->GetIdealBounds()
          .y();
  const int current_y = presenter_.GetView()->GetBoundsInScreen().y();
  return current_y > ideal_shelf_y;
}

void AppListControllerImpl::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  if (client_)
    client_->GetNavigableContentsFactory(std::move(receiver));
}

int AppListControllerImpl::GetTargetYForAppListHide(aura::Window* root_window) {
  DCHECK(Shell::HasInstance());
  return Shelf::ForWindow(root_window)->GetIdealBounds().y();
}

ash::AssistantViewDelegate* AppListControllerImpl::GetAssistantViewDelegate() {
  return Shell::Get()->assistant_controller()->view_delegate();
}

void AppListControllerImpl::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {
  if (client_)
    client_->OnSearchResultVisibilityChanged(id, visibility);
}

void AppListControllerImpl::NotifySearchResultsForLogging(
    const base::string16& raw_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int position_index) {
  if (client_) {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->NotifySearchResultsForLogging(query, results, position_index);
  }
}

bool AppListControllerImpl::IsAssistantAllowedAndEnabled() const {
  if (!chromeos::switches::IsAssistantEnabled())
    return false;

  if (!Shell::Get()->assistant_controller()->IsAssistantReady())
    return false;

  auto* controller = VoiceInteractionController::Get();
  return controller->settings_enabled().value_or(false) &&
         controller->allowed_state() == mojom::AssistantAllowedState::ALLOWED &&
         controller->voice_interaction_state().value_or(
             mojom::VoiceInteractionState::NOT_READY) !=
             mojom::VoiceInteractionState::NOT_READY;
}

bool AppListControllerImpl::ShouldShowAssistantPrivacyInfo() const {
  if (!IsAssistantAllowedAndEnabled())
    return false;

  if (!app_list_features::IsEmbeddedAssistantUIEnabled())
    return false;

  const bool dismissed = IsAssistantPrivacyInfoDismissed();
  if (dismissed)
    return false;

  const int count = GetAssistantPrivacyInfoShownCount();
  constexpr int kThresholdToShow = 6;
  return count >= 0 && count <= kThresholdToShow;
}

void AppListControllerImpl::MaybeIncreaseAssistantPrivacyInfoShownCount() {
  const bool should_show = ShouldShowAssistantPrivacyInfo();
  if (should_show) {
    const int count = GetAssistantPrivacyInfoShownCount();
    SetAssistantPrivacyInfoShownCount(count + 1);
  }
}

void AppListControllerImpl::MarkAssistantPrivacyInfoDismissed() {
  // User dismissed the privacy info view. Will not show the view again.
  SetAssistantPrivacyInfoDismissed();
}

void AppListControllerImpl::OnStateTransitionAnimationCompleted(
    ash::AppListViewState state) {
  if (!state_transition_animation_callback_.is_null())
    state_transition_animation_callback_.Run(state);
}

void AppListControllerImpl::GetAppLaunchedMetricParams(
    app_list::AppLaunchedMetricParams* metric_params) {
  metric_params->app_list_view_state = GetAppListViewState();
  metric_params->is_tablet_mode = IsTabletMode();
  metric_params->home_launcher_shown = presenter_.home_launcher_shown();
}

void AppListControllerImpl::RecordAppLaunched(
    AppListLaunchedFrom launched_from) {
  app_list::RecordAppListAppLaunched(launched_from, GetAppListViewState(),
                                     IsTabletMode(),
                                     presenter_.home_launcher_shown());
}

void AppListControllerImpl::AddObserver(AppListControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListControllerImpl::RemoveObserver(
    AppListControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListControllerImpl::NotifyAppListVisibilityChanged(bool visible,
                                                           int64_t display_id) {
  // Notify chrome of visibility changes.
  if (client_)
    client_->OnAppListVisibilityChanged(visible);

  for (auto& observer : observers_)
    observer.OnAppListVisibilityChanged(visible, display_id);
}

void AppListControllerImpl::NotifyAppListTargetVisibilityChanged(bool visible) {
  // Notify chrome of target visibility changes.
  if (client_)
    client_->OnAppListTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// Private used only:

void AppListControllerImpl::OnHomeLauncherDragStart() {
  app_list::AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragStart();
}

void AppListControllerImpl::OnHomeLauncherDragInProgress() {
  app_list::AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragInProgress();
}

void AppListControllerImpl::OnHomeLauncherDragEnd() {
  app_list::AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragEnd();
}

syncer::StringOrdinal AppListControllerImpl::GetOemFolderPos() {
  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  app_list::AppListItemList* item_list = model_->top_level_item_list();
  if (!item_list->item_count()) {
    LOG(ERROR) << "No top level item was found. "
               << "Placing OEM folder at the beginning.";
    return syncer::StringOrdinal::CreateInitialOrdinal();
  }

  size_t web_store_app_index;
  if (!item_list->FindItemIndex(extensions::kWebStoreAppId,
                                &web_store_app_index)) {
    LOG(ERROR) << "Web store position is not found it top items. "
               << "Placing OEM folder at the end.";
    return item_list->item_at(item_list->item_count() - 1)
        ->position()
        .CreateAfter();
  }

  // Skip items with the same position.
  const app_list::AppListItem* web_store_app_item =
      item_list->item_at(web_store_app_index);
  for (size_t j = web_store_app_index + 1; j < item_list->item_count(); ++j) {
    const app_list::AppListItem* next_item = item_list->item_at(j);
    DCHECK(next_item->position().IsValid());
    if (!next_item->position().Equals(web_store_app_item->position())) {
      const syncer::StringOrdinal oem_ordinal =
          web_store_app_item->position().CreateBetween(next_item->position());
      VLOG(1) << "Placing OEM Folder at: " << j
              << " position: " << oem_ordinal.ToDebugString();
      return oem_ordinal;
    }
  }

  const syncer::StringOrdinal oem_ordinal =
      web_store_app_item->position().CreateAfter();
  VLOG(1) << "Placing OEM Folder at: " << item_list->item_count()
          << " position: " << oem_ordinal.ToDebugString();
  return oem_ordinal;
}

std::unique_ptr<app_list::AppListItem> AppListControllerImpl::CreateAppListItem(
    std::unique_ptr<ash::AppListItemMetadata> metadata) {
  std::unique_ptr<app_list::AppListItem> app_list_item =
      metadata->is_folder
          ? std::make_unique<app_list::AppListFolderItem>(metadata->id)
          : std::make_unique<app_list::AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

app_list::AppListFolderItem* AppListControllerImpl::FindFolderItem(
    const std::string& folder_id) {
  return model_->FindFolderItem(folder_id);
}

void AppListControllerImpl::UpdateAssistantVisibility() {
  GetSearchModel()->search_box()->SetShowAssistantButton(
      IsAssistantAllowedAndEnabled());
}

int64_t AppListControllerImpl::GetDisplayIdToShowAppListOn() {
  if (IsHomeScreenAvailable() &&
      !Shell::Get()->display_manager()->IsInUnifiedMode()) {
    return display::Display::HasInternalDisplay()
               ? display::Display::InternalDisplayId()
               : display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(ash::Shell::GetRootWindowForNewWindows())
      .id();
}

void AppListControllerImpl::ResetHomeLauncherIfShown() {
  if (!IsHomeScreenAvailable() || !presenter_.IsVisible())
    return;

  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible())
    keyboard_controller->HideKeyboardByUser();
  presenter_.GetView()->CloseOpenedPage();

  // Refresh the suggestion chips with empty query.
  StartSearch(base::string16());
}

void AppListControllerImpl::UpdateLauncherContainer(
    base::Optional<int64_t> display_id) {
  aura::Window* window = presenter_.GetWindow();
  if (!window)
    return;

  aura::Window* parent_window = GetContainerForDisplayId(display_id);
  if (parent_window && !parent_window->Contains(window)) {
    parent_window->AddChild(window);
    bool is_showing_app_window = false;
    for (auto* app_window :
         Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
             kActiveDesk)) {
      if (!parent_window->Contains(app_window) &&
          !WindowState::Get(app_window)->IsMinimized()) {
        is_showing_app_window = true;
        break;
      }
    }
    if (ShouldLauncherShowBehindApps() && is_showing_app_window) {
      // When move launcher back to behind apps, and there is app window
      // showing, we release focus.
      Shell::Get()->activation_client()->DeactivateWindow(window);
    }
  }
}

int AppListControllerImpl::GetContainerId() const {
  return ShouldLauncherShowBehindApps()
             ? ash::kShellWindowId_HomeScreenContainer
             : ash::kShellWindowId_AppListContainer;
}

aura::Window* AppListControllerImpl::GetContainerForDisplayId(
    base::Optional<int64_t> display_id) {
  aura::Window* root_window =
      display_id.has_value()
          ? Shell::GetRootWindowForDisplayId(display_id.value())
          : presenter_.GetWindow()->GetRootWindow();
  return root_window->GetChildById(GetContainerId());
}

bool AppListControllerImpl::ShouldLauncherShowBehindApps() const {
  return IsHomeScreenAvailable() &&
         model_->state() != ash::AppListState::kStateEmbeddedAssistant;
}

int AppListControllerImpl::GetLastQueryLength() {
  return search_model_.search_box()->text().length();
}

void AppListControllerImpl::Shutdown() {
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;

  Shell* shell = Shell::Get();
  shell->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->RemoveObserver(this);
  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    shell->assistant_controller()->RemoveObserver(this);
    shell->assistant_controller()->ui_controller()->RemoveModelObserver(this);
  }
  shell->mru_window_tracker()->RemoveObserver(this);
  shell->window_tree_host_manager()->RemoveObserver(this);
  VoiceInteractionController::Get()->RemoveLocalObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  shell->wallpaper_controller()->RemoveObserver(this);
  shell->tablet_mode_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  model_->RemoveObserver(this);
}

void AppListControllerImpl::NotifyHomeLauncherAnimationTransition(
    AnimationTrigger trigger,
    bool launcher_will_show) {
  // The AppListView may not exist if this is happening after tablet mode
  // has started, but before the view is created.
  if (!presenter_.GetView())
    return;

  presenter_.GetView()->OnTabletModeAnimationTransitionNotified(
      CalculateAnimationTransitionForMetrics(trigger, launcher_will_show));
}

}  // namespace ash
