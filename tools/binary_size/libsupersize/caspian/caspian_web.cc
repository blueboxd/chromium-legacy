// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command-line interface for checking the integrity of .size files.
// Intended to be called from WebAssembly code.

#include <stdlib.h>

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "third_party/jsoncpp/source/include/json/json.h"
#include "third_party/re2/src/re2/re2.h"
#include "tools/binary_size/libsupersize/caspian/file_format.h"
#include "tools/binary_size/libsupersize/caspian/model.h"
#include "tools/binary_size/libsupersize/caspian/tree_builder.h"

namespace caspian {
namespace {
SizeInfo info;
std::unique_ptr<TreeBuilder> builder;

std::unique_ptr<Json::StreamWriter> writer;

std::string JsonSerialize(const Json::Value& value) {
  if (!writer) {
    writer.reset(Json::StreamWriterBuilder().newStreamWriter());
  }
  std::stringstream s;
  writer->write(value, &s);
  return s.str();
}
}  // namespace

extern "C" {
void LoadSizeFile(const char* compressed, size_t size) {
  ParseSizeInfo(compressed, size, &info);
}

void BuildTree(bool group_by_component,
               const char* include_regex_str,
               const char* exclude_regex_str,
               int minimum_size_bytes) {
  std::vector<std::function<bool(const Symbol&)>> filters;

  if (minimum_size_bytes > 0) {
    filters.push_back([minimum_size_bytes](const Symbol& sym) -> bool {
      return sym.pss() >= minimum_size_bytes;
    });
  }

  std::unique_ptr<RE2> include_regex;
  if (include_regex_str && *include_regex_str) {
    include_regex.reset(new RE2(include_regex_str));
    if (include_regex->error_code() == RE2::NoError) {
      filters.push_back([&include_regex](const Symbol& sym) -> bool {
        return RE2::PartialMatch(sym.full_name, *include_regex);
      });
    }
  }

  std::unique_ptr<RE2> exclude_regex;
  if (exclude_regex_str && *exclude_regex_str) {
    exclude_regex.reset(new RE2(exclude_regex_str));
    if (exclude_regex->error_code() == RE2::NoError) {
      filters.push_back([&exclude_regex](const Symbol& sym) -> bool {
        return !RE2::PartialMatch(sym.full_name, *exclude_regex);
      });
    }
  }

  builder.reset(new TreeBuilder(&info, group_by_component, filters));
  builder->Build();
}

const char* Open(const char* path) {
  // Returns a string that can be parsed to a JS object.
  static std::string result;
  Json::Value v = builder->Open(path);
  result = JsonSerialize(v);
  return result.c_str();
}
}  // extern "C"
}  // namespace caspian
