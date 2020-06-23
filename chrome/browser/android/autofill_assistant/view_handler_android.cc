// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/view_handler_android.h"
#include "components/autofill_assistant/browser/field_formatter.h"

namespace autofill_assistant {

ViewHandlerAndroid::ViewHandlerAndroid(
    const std::map<std::string, std::string>& identifier_placeholders)
    : identifier_placeholders_(identifier_placeholders) {}
ViewHandlerAndroid::~ViewHandlerAndroid() = default;

base::WeakPtr<ViewHandlerAndroid> ViewHandlerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::Optional<base::android::ScopedJavaGlobalRef<jobject>>
ViewHandlerAndroid::GetView(const std::string& input) const {
  // Replace all placeholders in the input.
  auto formatted_identifier =
      field_formatter::FormatString(input, identifier_placeholders_);
  if (!formatted_identifier.has_value()) {
    return base::nullopt;
  }
  std::string view_identifier = *formatted_identifier;
  auto it = views_.find(view_identifier);
  if (it == views_.end()) {
    return base::nullopt;
  }
  return it->second;
}

// Adds a view to the set of managed views.
void ViewHandlerAndroid::AddView(
    const std::string& input,
    base::android::ScopedJavaGlobalRef<jobject> jview) {
  // Replace all placeholders in the input.
  auto formatted_identifier =
      field_formatter::FormatString(input, identifier_placeholders_);
  if (!formatted_identifier.has_value()) {
    return;
  }
  std::string view_identifier = *formatted_identifier;
  DCHECK(views_.find(view_identifier) == views_.end());
  views_.emplace(view_identifier, jview);
}

void ViewHandlerAndroid::AddIdentifierPlaceholders(
    const std::map<std::string, std::string> placeholders) {
  for (const auto& placeholder : placeholders) {
    identifier_placeholders_[placeholder.first] = placeholder.second;
  }
}

void ViewHandlerAndroid::RemoveIdentifierPlaceholders(
    const std::map<std::string, std::string> placeholders) {
  for (const auto& placeholder : placeholders) {
    identifier_placeholders_.erase(placeholder.first);
  }
}

}  // namespace autofill_assistant
