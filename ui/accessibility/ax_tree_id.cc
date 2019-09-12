// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_id.h"

#include <iostream>

#include "base/no_destructor.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

AXTreeID::AXTreeID() {}

AXTreeID::AXTreeID(const AXTreeID& other) = default;

AXTreeID::AXTreeID(ax::mojom::AXTreeIDType type) : type_(type) {
  if (type_ == ax::mojom::AXTreeIDType::kToken)
    token_ = base::UnguessableToken::Create();
}

AXTreeID::AXTreeID(const std::string& string) {
  if (string.empty()) {
    type_ = ax::mojom::AXTreeIDType::kUnknown;
  } else {
    type_ = ax::mojom::AXTreeIDType::kToken;
    base::Value string_value(string);
    base::UnguessableToken token;
    CHECK(base::GetValueAsUnguessableToken(string_value, &token));
    token_ = token;
  }
}

// static
AXTreeID AXTreeID::FromString(const std::string& string) {
  return AXTreeID(string);
}

// static
AXTreeID AXTreeID::CreateNewAXTreeID() {
  return AXTreeID(ax::mojom::AXTreeIDType::kToken);
}

std::string AXTreeID::ToString() const {
  switch (type_) {
    case ax::mojom::AXTreeIDType::kUnknown:
      return "";
    case ax::mojom::AXTreeIDType::kToken:
      return base::CreateUnguessableTokenValue(*token_).GetString();
  }

  NOTREACHED();
  return std::string();
}

bool AXTreeID::operator==(const AXTreeID& rhs) const {
  return type_ == rhs.type_ && token_ == rhs.token_;
}

bool AXTreeID::operator!=(const AXTreeID& rhs) const {
  return !(*this == rhs);
}

bool AXTreeID::operator<(const AXTreeID& rhs) const {
  return std::tie(type_, token_) < std::tie(rhs.type_, rhs.token_);
}

bool AXTreeID::operator<=(const AXTreeID& rhs) const {
  return std::tie(type_, token_) <= std::tie(rhs.type_, rhs.token_);
}

bool AXTreeID::operator>(const AXTreeID& rhs) const {
  return !(*this <= rhs);
}

bool AXTreeID::operator>=(const AXTreeID& rhs) const {
  return !(*this < rhs);
}

std::ostream& operator<<(std::ostream& stream, const AXTreeID& value) {
  return stream << value.ToString();
}

const AXTreeID& AXTreeIDUnknown() {
  static const base::NoDestructor<AXTreeID> ax_tree_id_unknown(
      ax::mojom::AXTreeIDType::kUnknown);
  return *ax_tree_id_unknown;
}

}  // namespace ui
