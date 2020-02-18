// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/value_util.h"
#include <algorithm>

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

// Intended for debugging.  Writes a string representation of |value| to |out|.
std::ostream& operator<<(std::ostream& out,
                         const ModelProto::ModelValue& value) {
  out << value.identifier() << ": " << value.value();
  return out;
}

// Convenience constructors.
ValueProto SimpleValue(bool b) {
  ValueProto value;
  value.mutable_booleans()->add_values(b);
  return value;
}

ValueProto SimpleValue(const std::string& s) {
  ValueProto value;
  value.mutable_strings()->add_values(s);
  return value;
}

ValueProto SimpleValue(int i) {
  ValueProto value;
  value.mutable_ints()->add_values(i);
  return value;
}

bool AreAllValuesOfType(const std::vector<ValueProto>& values,
                        ValueProto::KindCase target_type) {
  if (values.empty()) {
    return false;
  }
  for (const auto& value : values) {
    if (value.kind_case() != target_type) {
      return false;
    }
  }
  return true;
}

bool AreAllValuesOfSize(const std::vector<ValueProto>& values,
                        int target_size) {
  if (values.empty()) {
    return false;
  }
  for (const auto& value : values) {
    switch (value.kind_case()) {
      case ValueProto::kStrings:
        if (value.strings().values_size() != target_size)
          return false;
        break;
      case ValueProto::kBooleans:
        if (value.booleans().values_size() != target_size)
          return false;
        break;
      case ValueProto::kInts:
        if (value.ints().values_size() != target_size)
          return false;
        break;
      case ValueProto::KIND_NOT_SET:
        if (target_size != 0) {
          return false;
        }
        break;
    }
  }
  return true;
}

base::Optional<ValueProto> CombineValues(
    const std::vector<ValueProto>& values) {
  if (values.empty()) {
    return base::nullopt;
  }
  auto shared_type = values[0].kind_case();
  if (!AreAllValuesOfType(values, shared_type)) {
    return base::nullopt;
  }
  if (shared_type == ValueProto::KIND_NOT_SET) {
    return ValueProto();
  }

  ValueProto result;
  for (const auto& value : values) {
    switch (shared_type) {
      case ValueProto::kStrings:
        std::for_each(
            value.strings().values().begin(), value.strings().values().end(),
            [&](auto& s) { result.mutable_strings()->add_values(s); });
        break;
      case ValueProto::kBooleans:
        std::for_each(
            value.booleans().values().begin(), value.booleans().values().end(),
            [&](auto& b) { result.mutable_booleans()->add_values(b); });
        break;
      case ValueProto::kInts:
        std::for_each(
            value.ints().values().begin(), value.ints().values().end(),
            [&](const auto& i) { result.mutable_ints()->add_values(i); });
        break;
      case ValueProto::KIND_NOT_SET:
        NOTREACHED();
        return base::nullopt;
    }
  }
  return result;
}

}  // namespace autofill_assistant
