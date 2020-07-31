// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/value_conversions.h"

#include <algorithm>

#include "base/values.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"

namespace chrome_pdf {

pp::Var VarFromValue(const base::Value& value) {
  switch (value.type()) {
    case base::Value::Type::NONE:
      return pp::Var::Null();
    case base::Value::Type::BOOLEAN:
      return pp::Var(value.GetBool());
    case base::Value::Type::INTEGER:
      return pp::Var(value.GetInt());
    case base::Value::Type::DOUBLE:
      return pp::Var(value.GetDouble());
    case base::Value::Type::STRING:
      return pp::Var(value.GetString());
    case base::Value::Type::BINARY: {
      const base::Value::BlobStorage& blob = value.GetBlob();
      pp::VarArrayBuffer buffer(blob.size());
      std::copy(blob.begin(), blob.end(), static_cast<uint8_t*>(buffer.Map()));
      return buffer;
    }
    case base::Value::Type::DICTIONARY: {
      pp::VarDictionary var_dict;
      for (const auto& value_pair : value.DictItems()) {
        var_dict.Set(value_pair.first, VarFromValue(value_pair.second));
      }
      return var_dict;
    }
    case base::Value::Type::LIST: {
      pp::VarArray var_array;
      uint32_t i = 0;
      for (const auto& val : value.GetList()) {
        var_array.Set(i, VarFromValue(val));
        i++;
      }
      return var_array;
    }
    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD:
      CHECK(false);
      return pp::Var();
  }
}

}  // namespace chrome_pdf