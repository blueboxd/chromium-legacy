// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"

#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_throbber.h"

namespace {

constexpr int kUpdateIconIntervalMs = 40;  // 40ms for 25 frames per second.
constexpr int kSpinningGapPercent = 25;

class SpinningEffectSource : public gfx::CanvasImageSource {
 public:
  SpinningEffectSource(const base::WeakPtr<ShelfSpinnerController>& host,
                       const std::string& app_id,
                       const gfx::ImageSkia& image)
      : gfx::CanvasImageSource(image.size()),
        host_(host),
        app_id_(app_id),
        image_(image) {}

  ~SpinningEffectSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    if (!host_)
      return;

    canvas->DrawImageInt(image_, 0, 0);

    const int gap = kSpinningGapPercent * image_.width() / 100;
    gfx::PaintThrobberSpinning(canvas,
                               gfx::Rect(gap, gap, image_.width() - 2 * gap,
                                         image_.height() - 2 * gap),
                               SK_ColorWHITE, host_->GetActiveTime(app_id_));
  }

 private:
  base::WeakPtr<ShelfSpinnerController> host_;
  const std::string app_id_;
  const gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(SpinningEffectSource);
};
}  // namespace

ShelfSpinnerController::ShelfSpinnerController(ChromeLauncherController* owner)
    : owner_(owner), weak_ptr_factory_(this) {
  owner->shelf_model()->AddObserver(this);
  if (user_manager::UserManager::IsInitialized()) {
    if (auto* active_user = user_manager::UserManager::Get()->GetActiveUser())
      current_account_id_ = active_user->GetAccountId();
    else
      LOG(ERROR) << "Failed to get active user, UserManager returned null";
  } else {
    LOG(ERROR) << "Failed to get active user, UserManager is not initialized";
  }
}

ShelfSpinnerController::~ShelfSpinnerController() {
  owner_->shelf_model()->RemoveObserver(this);
}

void ShelfSpinnerController::MaybeApplySpinningEffect(const std::string& app_id,
                                                      gfx::ImageSkia* image) {
  DCHECK(image);
  if (app_controller_map_.find(app_id) == app_controller_map_.end())
    return;

  const color_utils::HSL shift = {-1, 0, 0.25};
  *image = gfx::ImageSkia(
      std::make_unique<SpinningEffectSource>(
          weak_ptr_factory_.GetWeakPtr(), app_id,
          gfx::ImageSkiaOperations::CreateTransparentImage(
              gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image, shift),
              0.5)),
      image->size());
}

void ShelfSpinnerController::HideSpinner(const std::string& app_id) {
  if (!RemoveSpinnerFromControllerMap(app_id))
    return;

  const ash::ShelfID shelf_id(app_id);

  // If the app whose spinner is being hidden is pinned, we don't want to un-pin
  // it when we remove it from the shelf, so disable pin syncing while we update
  // things.
  auto pin_disabler = owner_->GetScopedPinSyncDisabler();
  // The static_cast here is safe, because if the delegate were not a
  // ShelfSpinnerItemController then ShelfItemDelegateChanged would have been
  // called and we would not have reached this place.
  auto delegate =
      owner_->shelf_model()->RemoveItemAndTakeShelfItemDelegate(shelf_id);
  std::unique_ptr<ShelfSpinnerItemController> cast_delegate(
      static_cast<ShelfSpinnerItemController*>(delegate.release()));

  hidden_app_controller_map_.emplace(
      current_account_id_, std::make_pair(app_id, std::move(cast_delegate)));
}

void ShelfSpinnerController::CloseSpinner(const std::string& app_id) {
  if (!RemoveSpinnerFromControllerMap(app_id))
    return;

  owner_->CloseLauncherItem(ash::ShelfID(app_id));
  UpdateShelfItemIcon(app_id);
}

bool ShelfSpinnerController::RemoveSpinnerFromControllerMap(
    const std::string& app_id) {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return false;

  const ash::ShelfID shelf_id(app_id);
  DCHECK_EQ(it->second, owner_->shelf_model()->GetShelfItemDelegate(shelf_id));
  app_controller_map_.erase(it);

  return true;
}

void ShelfSpinnerController::CloseCrostiniSpinners() {
  std::vector<std::string> app_ids_to_close;
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          chromeos::ProfileHelper::Get()->GetProfileByAccountId(
              current_account_id_));
  for (const auto& app_id_controller_pair : app_controller_map_) {
    if (registry_service->IsCrostiniShelfAppId(app_id_controller_pair.first))
      app_ids_to_close.push_back(app_id_controller_pair.first);
  }
  for (const auto& app_id : app_ids_to_close)
    CloseSpinner(app_id);
}

bool ShelfSpinnerController::HasApp(const std::string& app_id) const {
  return app_controller_map_.count(app_id);
}

base::TimeDelta ShelfSpinnerController::GetActiveTime(
    const std::string& app_id) const {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return base::TimeDelta();

  return it->second->GetActiveTime();
}

Profile* ShelfSpinnerController::OwnerProfile() {
  return owner_->profile();
}

void ShelfSpinnerController::ShelfItemDelegateChanged(
    const ash::ShelfID& id,
    ash::ShelfItemDelegate* old_delegate,
    ash::ShelfItemDelegate* delegate) {
  auto it = app_controller_map_.find(id.app_id);
  if (it != app_controller_map_.end()) {
    app_controller_map_.erase(it);
    UpdateShelfItemIcon(id.app_id);
  }
}

void ShelfSpinnerController::ActiveUserChanged(const AccountId& account_id) {
  if (account_id == current_account_id_) {
    LOG(WARNING) << "Tried switching to currently active user";
    return;
  }

  std::vector<std::string> to_hide;
  std::vector<
      std::pair<std::string, std::unique_ptr<ShelfSpinnerItemController>>>
      to_show;

  for (const auto& app_id : app_controller_map_)
    to_hide.push_back(app_id.first);
  for (auto it = hidden_app_controller_map_.lower_bound(account_id);
       it != hidden_app_controller_map_.upper_bound(account_id); it++) {
    to_show.push_back(std::move(it->second));
  }

  hidden_app_controller_map_.erase(
      hidden_app_controller_map_.lower_bound(account_id),
      hidden_app_controller_map_.upper_bound(account_id));

  for (const auto& app_id : to_hide)
    HideSpinner(app_id);

  for (auto& app_id_delegate_pair : to_show) {
    AddSpinnerToShelf(app_id_delegate_pair.first,
                      std::move(app_id_delegate_pair.second));
  }

  current_account_id_ = account_id;
}

void ShelfSpinnerController::UpdateShelfItemIcon(const std::string& app_id) {
  owner_->UpdateLauncherItemImage(app_id);
}

void ShelfSpinnerController::UpdateApps() {
  if (app_controller_map_.empty())
    return;

  RegisterNextUpdate();
  for (const auto pair : app_controller_map_)
    UpdateShelfItemIcon(pair.first);
}

void ShelfSpinnerController::RegisterNextUpdate() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShelfSpinnerController::UpdateApps,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateIconIntervalMs));
}

void ShelfSpinnerController::AddSpinnerToShelf(
    const std::string& app_id,
    std::unique_ptr<ShelfSpinnerItemController> controller) {
  const ash::ShelfID shelf_id(app_id);

  // We should only apply the spinner controller only over non-active items.
  const ash::ShelfItem* item = owner_->GetItem(shelf_id);
  if (item && item->status != ash::STATUS_CLOSED)
    return;

  controller->SetHost(weak_ptr_factory_.GetWeakPtr());
  ShelfSpinnerItemController* item_controller = controller.get();
  if (!item) {
    owner_->CreateAppLauncherItem(std::move(controller), ash::STATUS_RUNNING);
  } else {
    owner_->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                std::move(controller));
    owner_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }

  if (app_controller_map_.empty())
    RegisterNextUpdate();

  app_controller_map_[app_id] = item_controller;
}
