// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_model.h"

#include "base/strings/string_util.h"

namespace autofill_assistant {

// Compares two 'repeated' fields and returns true if every element matches.
template <typename T>
bool RepeatedFieldEquals(const T& values_a, const T& values_b) {
  if (values_a.size() != values_b.size()) {
    return false;
  }
  for (int i = 0; i < values_a.size(); i++) {
    if (values_a[i] != values_b[i]) {
      return false;
    }
  }
  return true;
}

// '==' operator specialization for RepeatedPtrField.
template <typename T>
bool operator==(const google::protobuf::RepeatedPtrField<T>& values_a,
                const google::protobuf::RepeatedPtrField<T>& values_b) {
  return RepeatedFieldEquals(values_a, values_b);
}

// '==' operator specialization for RepeatedField.
template <typename T>
bool operator==(const google::protobuf::RepeatedField<T>& values_a,
                const google::protobuf::RepeatedField<T>& values_b) {
  return RepeatedFieldEquals(values_a, values_b);
}

// Compares two |ValueProto| instances and returns true if they exactly match.
bool operator==(const ValueProto& value_a, const ValueProto& value_b) {
  if (value_a.kind_case() != value_b.kind_case()) {
    return false;
  }
  switch (value_a.kind_case()) {
    case ValueProto::kStrings:
      return value_a.strings().values() == value_b.strings().values();
      break;
    case ValueProto::kBooleans:
      return value_a.booleans().values() == value_b.booleans().values();
      break;
    case ValueProto::kInts:
      return value_a.ints().values() == value_b.ints().values();
      break;
    case ValueProto::KIND_NOT_SET:
      return true;
  }
  return true;
}

// Comapres two |ModelValue| instances and returns true if they exactly match.
bool operator==(const ModelProto::ModelValue& value_a,
                const ModelProto::ModelValue& value_b) {
  return value_a.identifier() == value_b.identifier() &&
         value_a.value() == value_b.value();
}

// Intended for debugging. Writes a string representation of |values| to |out|.
template <typename T>
std::ostream& WriteRepeatedField(std::ostream& out, const T& values) {
  std::string separator = "";
  out << "[";
  for (const auto& value : values) {
    out << separator << value;
    separator = ", ";
  }
  out << "]";
  return out;
}

// Intended for debugging. '<<' operator specialization for RepeatedPtrField.
template <typename T>
std::ostream& operator<<(std::ostream& out,
                         const google::protobuf::RepeatedPtrField<T>& values) {
  return WriteRepeatedField(out, values);
}

// Intended for debugging. '<<' operator specialization for RepeatedField.
template <typename T>
std::ostream& operator<<(std::ostream& out,
                         const google::protobuf::RepeatedField<T>& values) {
  return WriteRepeatedField(out, values);
}

// Intended for debugging.  Writes a string representation of |value| to |out|.
std::ostream& operator<<(std::ostream& out, const ValueProto& value) {
  switch (value.kind_case()) {
    case ValueProto::kStrings:
      out << value.strings().values();
      break;
    case ValueProto::kBooleans:
      out << value.booleans().values();
      break;
    case ValueProto::kInts:
      out << value.ints().values();
      break;
    case ValueProto::KIND_NOT_SET:
      break;
  }
  return out;
}

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
