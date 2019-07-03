// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_folder_item.h"

#include "ash/app_list/model/app_list_item_list.h"
#include "base/guid.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

AppListFolderItem::AppListFolderItem(const std::string& id)
    : AppListItem(id),
      folder_type_(id == ash::kOemFolderId ? FOLDER_TYPE_OEM
                                           : FOLDER_TYPE_NORMAL),
      item_list_(new AppListItemList),
      folder_image_(item_list_.get()) {
  folder_image_.AddObserver(this);
  set_is_folder(true);
}

AppListFolderItem::~AppListFolderItem() {
  folder_image_.RemoveObserver(this);
}

gfx::Rect AppListFolderItem::GetTargetIconRectInFolderForItem(
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) {
  return folder_image_.GetTargetIconRectInFolderForItem(item,
                                                        folder_icon_bounds);
}

// static
const char AppListFolderItem::kItemType[] = "FolderItem";

const char* AppListFolderItem::GetItemType() const {
  return AppListFolderItem::kItemType;
}

AppListItem* AppListFolderItem::FindChildItem(const std::string& id) {
  return item_list_->FindItem(id);
}

size_t AppListFolderItem::ChildItemCount() const {
  return item_list_->item_count();
}

bool AppListFolderItem::IsPersistent() const {
  return GetMetadata()->is_persistent;
}

void AppListFolderItem::SetIsPersistent(bool is_persistent) {
  metadata()->is_persistent = is_persistent;
}

bool AppListFolderItem::ShouldAutoRemove() const {
  return ChildItemCount() <= (IsPersistent() ? 0u : 1u);
}

std::string AppListFolderItem::GenerateId() {
  return base::GenerateGUID();
}

void AppListFolderItem::OnFolderImageUpdated() {
  SetIcon(folder_image_.icon());
}

void AppListFolderItem::NotifyOfDraggedItem(AppListItem* dragged_item) {
  folder_image_.UpdateDraggedItem(dragged_item);
}

}  // namespace app_list
