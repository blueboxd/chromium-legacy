// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_

#include <ostream>
#include <string>
#include <vector>
#include "base/optional.h"
#include "components/autofill_assistant/browser/model.pb.h"

namespace autofill_assistant {

// Custom comparison operator for |ValueProto|, because we can't use
// |MessageDifferencer| for protobuf lite and can't rely on serialization.
bool operator==(const ValueProto& value_a, const ValueProto& value_b);

// Custom comparison operator for |ModelValue|.
bool operator==(const ModelProto::ModelValue& value_a,
                const ModelProto::ModelValue& value_b);

// Intended for debugging.
std::ostream& operator<<(std::ostream& out, const ValueProto& value);
std::ostream& operator<<(std::ostream& out,
                         const ModelProto::ModelValue& value);

// Convenience constructors.
ValueProto SimpleValue(bool value);
ValueProto SimpleValue(const std::string& value);
ValueProto SimpleValue(int value);

// Returns true if all |values| share the specified |target_type|.
bool AreAllValuesOfType(const std::vector<ValueProto>& values,
                        ValueProto::KindCase target_type);

// Returns true if all |values| share the specified |target_size|.
bool AreAllValuesOfSize(const std::vector<ValueProto>& values, int target_size);

// Combines all specified |values| in a single ValueProto where the individual
// value lists are appended after each other. Returns nullopt if |values| do not
// share the same type.
base::Optional<ValueProto> CombineValues(const std::vector<ValueProto>& values);

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_
