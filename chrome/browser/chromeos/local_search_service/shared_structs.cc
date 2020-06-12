// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/shared_structs.h"

#include <utility>

#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/local_search_service/linear_map_search.h"
#include "chrome/common/string_matching/fuzzy_tokenized_string_match.h"
#include "chrome/common/string_matching/tokenized_string.h"

namespace local_search_service {

local_search_service::Content::Content(const std::string& id,
                                       const base::string16& content)
    : id(id), content(content) {}
local_search_service::Content::Content() = default;
local_search_service::Content::Content(const Content& content) = default;
local_search_service::Content::~Content() = default;

local_search_service::Data::Data(
    const std::string& id,
    const std::vector<local_search_service::Content>& contents)
    : id(id), contents(contents) {}
local_search_service::Data::Data() = default;
local_search_service::Data::Data(const Data& data) = default;
local_search_service::Data::~Data() = default;
local_search_service::Result::Result() = default;
local_search_service::Result::Result(const Result& result) = default;
local_search_service::Result::~Result() = default;

}  // namespace local_search_service
