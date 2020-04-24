// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"

#include <utility>

#include "base/base64.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/v2/public/types.h"

// Note: This file contains implementation for both types.h and public/types.h.
//       because our build system will not allow multiple types.cc files in the
//       same target.

namespace feed {
namespace {

void PickleDebugStreamData(const DebugStreamData& data, base::Pickle* pickle) {
  pickle->WriteInt(DebugStreamData::kVersion);
  pickle->WriteBool(data.fetch_info.has_value());
  if (data.fetch_info) {
    pickle->WriteInt(data.fetch_info->status_code);
    pickle->WriteUInt64(data.fetch_info->fetch_duration.InMilliseconds());
    pickle->WriteUInt64((data.fetch_info->fetch_time - base::Time::UnixEpoch())
                            .InMilliseconds());
    pickle->WriteString(data.fetch_info->bless_nonce);
    pickle->WriteString(data.fetch_info->base_request_url.spec());
  }
  pickle->WriteString(data.load_stream_status);
}

base::Optional<DebugStreamData> UnpickleDebugStreamData(
    base::PickleIterator iterator) {
  DebugStreamData result;
  int version;
  if (!iterator.ReadInt(&version) || version != DebugStreamData::kVersion)
    return base::nullopt;
  bool has_fetch_info;
  if (!iterator.ReadBool(&has_fetch_info))
    return base::nullopt;
  if (has_fetch_info) {
    NetworkResponseInfo fetch_info;
    uint64_t fetch_duration_ms;
    uint64_t fetch_time_ms;
    std::string base_request_url;
    if (!(iterator.ReadInt(&fetch_info.status_code) &&
          iterator.ReadUInt64(&fetch_duration_ms) &&
          iterator.ReadUInt64(&fetch_time_ms) &&
          iterator.ReadString(&fetch_info.bless_nonce) &&
          iterator.ReadString(&base_request_url)))
      return base::nullopt;
    fetch_info.fetch_duration =
        base::TimeDelta::FromMilliseconds(fetch_duration_ms);
    fetch_info.fetch_time = base::TimeDelta::FromMilliseconds(fetch_time_ms) +
                            base::Time::UnixEpoch();
    fetch_info.base_request_url = GURL(base_request_url);
    result.fetch_info = std::move(fetch_info);
  }
  if (!iterator.ReadString(&result.load_stream_status))
    return base::nullopt;
  return result;
}

}  // namespace

std::string ToString(ContentRevision c) {
  return base::NumberToString(c.value());
}

ContentRevision ToContentRevision(const std::string& str) {
  uint32_t value;
  if (!base::StringToUint(str, &value))
    return {};
  return ContentRevision(value);
}

std::string SerializeDebugStreamData(const DebugStreamData& data) {
  base::Pickle pickle;
  PickleDebugStreamData(data, &pickle);
  const uint8_t* pickle_data_ptr = static_cast<const uint8_t*>(pickle.data());
  return base::Base64Encode(
      base::span<const uint8_t>(pickle_data_ptr, pickle.size()));
}

base::Optional<DebugStreamData> DeserializeDebugStreamData(
    base::StringPiece base64_encoded) {
  std::string binary_data;
  if (!base::Base64Decode(base64_encoded, &binary_data))
    return base::nullopt;
  base::Pickle pickle(binary_data.data(), binary_data.size());
  return UnpickleDebugStreamData(base::PickleIterator(pickle));
}

DebugStreamData::DebugStreamData() = default;
DebugStreamData::~DebugStreamData() = default;
DebugStreamData::DebugStreamData(const DebugStreamData&) = default;
DebugStreamData& DebugStreamData::operator=(const DebugStreamData&) = default;

}  // namespace feed
