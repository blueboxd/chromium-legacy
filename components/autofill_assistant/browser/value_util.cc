// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/value_util.h"
#include <algorithm>
#include "base/i18n/case_conversion.h"
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

ValueProto SimpleValue(const DateProto& proto) {
  ValueProto value;
  auto* date = value.mutable_dates()->add_values();
  date->set_year(proto.year());
  date->set_month(proto.month());
  date->set_day(proto.day());
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
      case ValueProto::kUserActions:
        if (value.user_actions().values_size() != target_size)
          return false;
        break;
      case ValueProto::kDates:
        if (value.dates().values_size() != target_size)
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
      case ValueProto::kUserActions:
        std::for_each(value.user_actions().values().begin(),
                      value.user_actions().values().end(),
                      [&](const auto& action) {
                        *result.mutable_user_actions()->add_values() = action;
                      });
        break;
      case ValueProto::kDates:
        std::for_each(value.dates().values().begin(),
                      value.dates().values().end(), [&](const auto& date) {
                        *result.mutable_dates()->add_values() = date;
                      });
        break;
      case ValueProto::KIND_NOT_SET:
        NOTREACHED();
        return base::nullopt;
    }
  }
  return result;
}

}  // namespace autofill_assistant
