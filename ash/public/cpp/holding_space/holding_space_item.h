// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"

namespace ash {

// Contains data needed to display a single item in the temporary holding space
// UI.
class ASH_PUBLIC_EXPORT HoldingSpaceItem {
 public:
  explicit HoldingSpaceItem(const std::string& id);
  HoldingSpaceItem(const HoldingSpaceItem& other) = delete;
  HoldingSpaceItem operator=(const HoldingSpaceItem& other) = delete;
  ~HoldingSpaceItem();

  const std::string& id() const { return id_; }

  void set_text(const base::string16& text) { text_ = text; }
  const base::Optional<base::string16>& text() const { return text_; }

 private:
  // The holding space item ID assigned to the item.
  std::string id_;

  // If set, the text data associated with the item.
  base::Optional<base::string16> text_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_H_
