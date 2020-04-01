// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_model.h"

#include "base/strings/string_util.h"

namespace autofill_assistant {

UserModel::Observer::Observer() = default;
UserModel::Observer::~Observer() = default;

UserModel::UserModel() = default;
UserModel::~UserModel() = default;

base::WeakPtr<UserModel> UserModel::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UserModel::SetValue(const std::string& identifier,
                         const ValueProto& value,
                         bool force_notification) {
  auto result = values_.emplace(identifier, value);
  if (!force_notification && !result.second && result.first->second == value) {
    return;
  } else if (!result.second) {
    result.first->second = value;
  }

  for (auto& observer : observers_) {
    observer.OnValueChanged(identifier, value);
  }
}

base::Optional<ValueProto> UserModel::GetValue(
    const std::string& identifier) const {
  auto it = values_.find(identifier);
  if (it != values_.end()) {
    return it->second;
  }
  return base::nullopt;
}

base::Optional<ValueProto> UserModel::GetValue(
    const ValueReferenceProto& reference) const {
  switch (reference.kind_case()) {
    case ValueReferenceProto::kValue:
      return reference.value();
    case ValueReferenceProto::kModelIdentifier:
      return GetValue(reference.model_identifier());
    case ValueReferenceProto::KIND_NOT_SET:
      return base::nullopt;
  }
}

void UserModel::MergeWithProto(const ModelProto& another,
                               bool force_notifications) {
  for (const auto& another_value : another.values()) {
    if (another_value.value() == ValueProto()) {
      // std::map::emplace does not overwrite existing values.
      if (values_.emplace(another_value.identifier(), another_value.value())
              .second ||
          force_notifications) {
        for (auto& observer : observers_) {
          observer.OnValueChanged(another_value.identifier(),
                                  another_value.value());
        }
      }
      continue;
    }
    SetValue(another_value.identifier(), another_value.value(),
             force_notifications);
  }
}

void UserModel::UpdateProto(ModelProto* model_proto) const {
  for (auto& model_value : *model_proto->mutable_values()) {
    auto it = values_.find(model_value.identifier());
    if (it != values_.end()) {
      *model_value.mutable_value() = (it->second);
    }
  }
}

void UserModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace autofill_assistant
