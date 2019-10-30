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

void BuildTree() {
  builder.reset(new TreeBuilder(&info));
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
