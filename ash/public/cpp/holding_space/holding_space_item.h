// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

// Contains data needed to display a single item in the temporary holding space
// UI.
class ASH_PUBLIC_EXPORT HoldingSpaceItem {
 public:
  // Items types supported by the holding space.
  enum class Type { kPinnedFile, kScreenshot, kDownload };

  HoldingSpaceItem(const HoldingSpaceItem&) = delete;
  HoldingSpaceItem operator=(const HoldingSpaceItem&) = delete;
  ~HoldingSpaceItem();

  // Generates an item ID for a holding space item backed by a file, based on
  // the file's file system URL.
  static std::string GetFileBackedItemId(Type type,
                                         const base::FilePath& file_path);

  // Creates a HoldingSpaceItem that's backed by a file system URL.
  static std::unique_ptr<HoldingSpaceItem> CreateFileBackedItem(
      Type type,
      const base::FilePath& file_path,
      const GURL& file_system_url,
      const gfx::ImageSkia& image);

  const std::string& id() const { return id_; }

  Type type() const { return type_; }

  const base::string16& text() const { return text_; }

  const gfx::ImageSkia& image() const { return image_; }

  const base::FilePath& file_path() const { return file_path_; }

  const GURL& file_system_url() const { return file_system_url_; }

 private:
  // Contructor for file backed items.
  HoldingSpaceItem(Type type,
                   const std::string& id,
                   const base::FilePath& file_path,
                   const GURL& file_system_url,
                   const base::string16& text,
                   const gfx::ImageSkia& image);

  const Type type_;

  // The holding space item ID assigned to the item.
  std::string id_;

  // The file path by which the item is backed.
  base::FilePath file_path_;

  // The file system URL of the file that backs the item.
  GURL file_system_url_;

  // If set, the text that should be shown for the item.
  base::string16 text_;

  // The image representation of the item.
  gfx::ImageSkia image_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
