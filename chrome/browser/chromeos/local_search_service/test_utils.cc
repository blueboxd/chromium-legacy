// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/test_utils.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace local_search_service {

namespace {

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

}  // namespace

std::vector<Data> CreateTestData(
    const std::map<std::string, std::vector<ContentWithId>>& input) {
  std::vector<Data> output;
  for (const auto& item : input) {
    Data data;
    data.id = item.first;
    std::vector<Content>& contents = data.contents;
    for (const auto& content_with_id : item.second) {
      const Content content(content_with_id.first,
                            base::UTF8ToUTF16(content_with_id.second));
      contents.push_back(content);
    }
    output.push_back(data);
  }
  return output;
}

}  // namespace local_search_service
