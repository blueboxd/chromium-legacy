// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/value_util.h"
#include <algorithm>
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill_assistant {

// Compares two 'repeated' fields and returns true if every element matches.
template <typename T>
bool RepeatedFieldEquals(const T& values_a, const T& values_b) {
  if (values_a.size() != values_b.size()) {
    return false;
  }
  for (int i = 0; i < values_a.size(); i++) {
    if (!(values_a[i] == values_b[i])) {
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
    case ValueProto::kBooleans:
      return value_a.booleans().values() == value_b.booleans().values();
    case ValueProto::kInts:
      return value_a.ints().values() == value_b.ints().values();
    case ValueProto::kUserActions:
      return value_a.user_actions().values() == value_b.user_actions().values();
    case ValueProto::kDates:
      return value_a.dates().values() == value_b.dates().values();
    case ValueProto::KIND_NOT_SET:
      return true;
  }
  return true;
}

bool operator!=(const ValueProto& value_a, const ValueProto& value_b) {
  return !(value_a == value_b);
}

bool operator<(const ValueProto& value_a, const ValueProto& value_b) {
  if (value_a.kind_case() != value_b.kind_case()) {
    return false;
  }
  if (!AreAllValuesOfSize({value_a, value_b}, 1)) {
    return false;
  }
  switch (value_a.kind_case()) {
    case ValueProto::kStrings:
      return base::i18n::FoldCase(
                 base::UTF8ToUTF16(value_a.strings().values(0))) <
             base::i18n::FoldCase(
                 base::UTF8ToUTF16(value_b.strings().values(0)));
    case ValueProto::kInts:
      return value_a.ints().values(0) < value_b.ints().values(0);
    case ValueProto::kDates:
      return value_a.dates().values(0) < value_b.dates().values(0);
    case ValueProto::kUserActions:
    case ValueProto::kBooleans:
    case ValueProto::KIND_NOT_SET:
      NOTREACHED();
      return false;
  }
  return true;
}

bool operator>(const ValueProto& value_a, const ValueProto& value_b) {
  return value_b < value_a && !(value_b == value_a);
}

// Compares two |ModelValue| instances and returns true if they exactly match.
bool operator==(const ModelProto::ModelValue& value_a,
                const ModelProto::ModelValue& value_b) {
  return value_a.identifier() == value_b.identifier() &&
         value_a.value() == value_b.value();
}

// Compares two |ChipProto| instances and returns true if they exactly match.
bool operator==(const ChipProto& value_a, const ChipProto& value_b) {
  return value_a.type() == value_b.type() && value_a.icon() == value_b.icon() &&
         value_a.text() == value_b.text() &&
         value_a.sticky() == value_b.sticky();
}

// Compares two |DirectActionProto| instances and returns true if they exactly
// match.
bool operator==(const DirectActionProto& value_a,
                const DirectActionProto& value_b) {
  return RepeatedFieldEquals(value_a.names(), value_b.names()) &&
         RepeatedFieldEquals(value_a.required_arguments(),
                             value_b.required_arguments()) &&
         RepeatedFieldEquals(value_a.optional_arguments(),
                             value_b.optional_arguments());
}

// Compares two |UserActionProto| instances and returns true if they exactly
// match.
bool operator==(const UserActionProto& value_a,
                const UserActionProto& value_b) {
  return value_a.chip() == value_b.chip() &&
         value_a.direct_action() == value_b.direct_action() &&
         value_a.identifier() == value_b.identifier() &&
         value_a.enabled() == value_b.enabled();
}

// Compares two |DateProto| instances and returns true if they exactly match.
bool operator==(const DateProto& value_a, const DateProto& value_b) {
  return value_a.year() == value_b.year() &&
         value_a.month() == value_b.month() && value_a.day() == value_b.day();
}

bool operator<(const DateProto& value_a, const DateProto& value_b) {
  auto tuple_a =
      std::make_tuple(value_a.year(), value_a.month(), value_a.day());
  auto tuple_b =
      std::make_tuple(value_b.year(), value_b.month(), value_b.day());
  return tuple_a < tuple_b;
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

// Intended for debugging. '<<' operator specialization for UserActionProto.
std::ostream& operator<<(std::ostream& out, const UserActionProto& value) {
  out << value.identifier();
  return out;
}

// Intended for debugging. '<<' operator specialization for DateProto.
std::ostream& operator<<(std::ostream& out, const DateProto& value) {
  out << value.year() << "-" << value.month() << "-" << value.day();
  return out;
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
    case ValueProto::kUserActions:
      out << value.user_actions().values();
      break;
    case ValueProto::kDates:
      out << value.dates().values();
      break;
    case ValueProto::KIND_NOT_SET:
      break;
  }
  if (value.is_client_side_only()) {
    out << " (client-side-only)";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const ValueReferenceProto& reference) {
  switch (reference.kind_case()) {
    case ValueReferenceProto::kValue:
      return out << reference.value();
    case ValueReferenceProto::kModelIdentifier:
      return out << reference.model_identifier();
    case ValueReferenceProto::KIND_NOT_SET:
      return out;
  }
}

// Intended for debugging.  Writes a string representation of |value| to |out|.
std::ostream& operator<<(std::ostream& out,
                         const ModelProto::ModelValue& value) {
  out << value.identifier() << ": " << value.value();
  return out;
}

// Convenience constructors.
ValueProto SimpleValue(bool b, bool is_client_side_only) {
  ValueProto value;
  value.mutable_booleans()->add_values(b);
  if (is_client_side_only)
    value.set_is_client_side_only(is_client_side_only);
  return value;
}

ValueProto SimpleValue(const std::string& s, bool is_client_side_only) {
  ValueProto value;
  value.mutable_strings()->add_values(s);
  if (is_client_side_only)
    value.set_is_client_side_only(is_client_side_only);
  return value;
}

ValueProto SimpleValue(int i, bool is_client_side_only) {
  ValueProto value;
  value.mutable_ints()->add_values(i);
  if (is_client_side_only)
    value.set_is_client_side_only(is_client_side_only);
  return value;
}

ValueProto SimpleValue(const DateProto& proto, bool is_client_side_only) {
  ValueProto value;
  auto* date = value.mutable_dates()->add_values();
  date->set_year(proto.year());
  date->set_month(proto.month());
  date->set_day(proto.day());
  if (is_client_side_only)
    value.set_is_client_side_only(is_client_side_only);
  return value;
}

ModelProto::ModelValue SimpleModelValue(const std::string& identifier,
                                        const ValueProto& value) {
  ModelProto::ModelValue model_value;
  model_value.set_identifier(identifier);
  *model_value.mutable_value() = value;
  return model_value;
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
    if (GetValueSize(value) != target_size) {
      return false;
    }
  }
  return true;
}

bool ContainsClientOnlyValue(const std::vector<ValueProto>& values) {
  for (const auto& value : values) {
    if (value.is_client_side_only()) {
      return true;
    }
  }
  return false;
}

int GetValueSize(const ValueProto& value) {
  switch (value.kind_case()) {
    case ValueProto::kStrings:
      return value.strings().values_size();
    case ValueProto::kBooleans:
      return value.booleans().values_size();
    case ValueProto::kInts:
      return value.ints().values_size();
    case ValueProto::kUserActions:
      return value.user_actions().values_size();
    case ValueProto::kDates:
      return value.dates().values_size();
    case ValueProto::KIND_NOT_SET:
      return 0;
  }
}

base::Optional<ValueProto> GetNthValue(const ValueProto& value, int index) {
  if (value == ValueProto()) {
    return base::nullopt;
  }
  if (index < 0 || index >= GetValueSize(value)) {
    return base::nullopt;
  }
  ValueProto nth_value;
  if (value.is_client_side_only())
    nth_value.set_is_client_side_only(value.is_client_side_only());
  switch (value.kind_case()) {
    case ValueProto::kStrings:
      nth_value.mutable_strings()->add_values(
          value.strings().values().at(index));
      return nth_value;
    case ValueProto::kBooleans:
      nth_value.mutable_booleans()->add_values(
          value.booleans().values().at(index));
      return nth_value;
    case ValueProto::kInts:
      nth_value.mutable_ints()->add_values(value.ints().values().at(index));
      return nth_value;
    case ValueProto::kUserActions:
      *nth_value.mutable_user_actions()->add_values() =
          value.user_actions().values().at(index);
      return nth_value;
    case ValueProto::kDates:
      *nth_value.mutable_dates()->add_values() =
          value.dates().values().at(index);
      return nth_value;
    case ValueProto::KIND_NOT_SET:
      return base::nullopt;
  }
}

}  // namespace autofill_assistant
