// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"

namespace ash {

namespace {

std::string TypeToString(HoldingSpaceItem::Type type) {
  switch (type) {
    case HoldingSpaceItem::Type::kPinnedFile:
      return "pinned_file";
    case HoldingSpaceItem::Type::kDownload:
      return "download";
    case HoldingSpaceItem::Type::kScreenshot:
      return "screenshot";
  }
}

}  // namespace

HoldingSpaceItem::~HoldingSpaceItem() = default;

// static
std::string HoldingSpaceItem::GetFileBackedItemId(
    Type type,
    const base::FilePath& file_path) {
  return base::StrCat({TypeToString(type), ":", file_path.value()});
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const base::FilePath& file_path,
    const GURL& file_system_url,
    const gfx::ImageSkia& image) {
  // Note: std::make_unique does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, GetFileBackedItemId(type, file_path), file_path, file_system_url,
      file_path.BaseName().LossyDisplayName(), image));
}

HoldingSpaceItem::HoldingSpaceItem(Type type,
                                   const std::string& id,
                                   const base::FilePath& file_path,
                                   const GURL& file_system_url,
                                   const base::string16& text,
                                   const gfx::ImageSkia& image)
    : type_(type),
      id_(id),
      file_path_(file_path),
      file_system_url_(file_system_url),
      text_(text),
      image_(image) {}

}  // namespace ash
