// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"

namespace ash {

HoldingSpaceKeyedService::HoldingSpaceKeyedService(
    content::BrowserContext* context,
    const AccountId& account_id)
    : browser_context_(context) {
  HoldingSpaceController::Get()->RegisterModelForUser(account_id,
                                                      &holding_space_model_);
}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() = default;

void HoldingSpaceKeyedService::AddScreenshot(
    const base::FilePath& screenshot_file,
    const gfx::ImageSkia& image) {
  GURL file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          Profile::FromBrowserContext(browser_context_), screenshot_file,
          file_manager::kFileManagerAppId, &file_system_url)) {
    VLOG(2) << "Unable to convert screenshot file path to File System URL";
    return;
  }

  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kScreenshot, screenshot_file, file_system_url,
      image);
  holding_space_model_.AddItem(std::move(item));
}

}  // namespace ash
