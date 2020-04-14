// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/dns/record_rdata.h"

namespace {

base::StringPiece MakeStringPiece(const std::vector<uint8_t>& vec) {
  return base::StringPiece(reinterpret_cast<const char*>(vec.data()),
                           vec.size());
}

// For arbitrary data, check that parse(data).serialize() == data.
void ParseThenSerializeProperty(const std::vector<uint8_t>& data) {
  // Since |data| is chosen by a fuzzer, the digest is unlikely to match the
  // nonce. As a result, |maybe_parsed| will likely be nullptr. However, we can
  // still exercise the code.
  auto maybe_parsed = net::IntegrityRecordRdata::Create(MakeStringPiece(data));
  if (!maybe_parsed) {
    return;  // Property is vacuously true since |data| was not parseable.
  }
  // Any parseable record's serialization should match the original input.
  std::vector<uint8_t> serialized = maybe_parsed->Serialize();
  CHECK_EQ(serialized.size(), maybe_parsed->LengthForSerialization());
  CHECK(data == serialized);
}

// For an arbitrary net::IntegrityRecordRdata r, test parse(r.serialize()) == r.
void SerializeThenParseProperty(const std::vector<uint8_t>& data) {
  // Ensure that the nonce is not too long to be serialized.
  if (data.size() > std::numeric_limits<uint16_t>::max()) {
    // Property is vacuously true because the record is not serializable.
    return;
  }
  // Build an IntegrityRecordRdata by treating |data| as a nonce.
  net::IntegrityRecordRdata record_from_nonce(data);
  std::vector<uint8_t> serialized = record_from_nonce.Serialize();
  CHECK_EQ(serialized.size(), record_from_nonce.LengthForSerialization());
  // Parsing |serialized| always produces a record identical to the original.
  auto maybe_parsed =
      net::IntegrityRecordRdata::Create(MakeStringPiece(serialized));
  CHECK(maybe_parsed);
  CHECK(maybe_parsed->IsEqual(&record_from_nonce));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::vector<uint8_t> data_vec(data, data + size);
  ParseThenSerializeProperty(data_vec);
  SerializeThenParseProperty(data_vec);
  // Construct a random IntegrityRecordRdata to exercise that code path. No need
  // to exercise parse/serialize since we already did that with |data|.
  net::IntegrityRecordRdata rand_record(net::IntegrityRecordRdata::Random());
  return 0;
}
