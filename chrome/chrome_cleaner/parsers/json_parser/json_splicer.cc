// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/json_parser/json_splicer.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/values.h"

namespace chrome_cleaner {

bool RemoveKeyFromDictionary(base::Value* dictionary, const std::string& key) {
  bool result = false;
  base::DictionaryValue* entries = nullptr;
  if (dictionary == nullptr || !dictionary->is_dict() ||
      !dictionary->GetAsDictionary(&entries)) {
    LOG(ERROR) << "Got a "
               << (dictionary ? dictionary->GetTypeName(dictionary->type())
                              : "NULL")
               << " but expected a dictionary.";
    return result;
  }
  if (!(result = entries->RemoveKey(key))) {
    LOG(ERROR) << key << " was not found in the dictionary";
  }
  return result;
}

bool RemoveValueFromList(base::Value* list, const std::string& key) {
  if (list == nullptr || !list->is_list()) {
    LOG(ERROR) << "Got a " << (list ? list->GetTypeName(list->type()) : "NULL")
               << " but expected a list.";
    return false;
  }

  if (!list->EraseListValue(base::Value(key))) {
    LOG(ERROR) << key << " was not found in the list";
    return false;
  }

  return true;
}

}  // namespace chrome_cleaner
