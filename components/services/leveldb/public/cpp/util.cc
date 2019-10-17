// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/leveldb/public/cpp/util.h"

#include "base/containers/span.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {

leveldb::Slice GetSliceFor(const std::vector<uint8_t>& key) {
  if (key.size() == 0)
    return leveldb::Slice();
  return leveldb::Slice(reinterpret_cast<const char*>(&key.front()),
                        key.size());
}

std::vector<uint8_t> GetVectorFor(const leveldb::Slice& s) {
  if (s.size() == 0)
    return std::vector<uint8_t>();
  return std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(s.data()),
      reinterpret_cast<const uint8_t*>(s.data() + s.size()));
}

std::string Uint8VectorToStdString(const std::vector<uint8_t>& input) {
  return std::string(reinterpret_cast<const char*>(input.data()), input.size());
}

std::vector<uint8_t> StdStringToUint8Vector(const std::string& input) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(input.data());
  return std::vector<uint8_t>(data, data + input.size());
}

base::StringPiece Uint8VectorToStringPiece(const std::vector<uint8_t>& input) {
  return base::StringPiece(reinterpret_cast<const char*>(input.data()),
                           input.size());
}

std::vector<uint8_t> StringPieceToUint8Vector(base::StringPiece input) {
  base::span<const uint8_t> data = base::as_bytes(base::make_span(input));
  return std::vector<uint8_t>(data.begin(), data.end());
}

base::string16 Uint8VectorToString16(const std::vector<uint8_t>& input) {
  return base::string16(reinterpret_cast<const base::char16*>(input.data()),
                        input.size() * sizeof(base::char16) / sizeof(uint8_t));
}

std::vector<uint8_t> String16ToUint8Vector(const base::string16& input) {
  base::span<const uint8_t> data = base::as_bytes(base::make_span(input));
  return std::vector<uint8_t>(data.begin(), data.end());
}

}  // namespace leveldb
