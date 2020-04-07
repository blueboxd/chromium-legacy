// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#include "base/pickle.h"

namespace blink {

namespace {
constexpr uint32_t kVersion = 1u;
}  // namespace

// static
base::Optional<std::string> UserAgentMetadata::Marshal(
    const base::Optional<UserAgentMetadata>& in) {
  if (!in)
    return base::nullopt;
  base::Pickle out;
  out.WriteUInt32(kVersion);
  out.WriteString(in->brand);
  out.WriteString(in->full_version);
  out.WriteString(in->major_version);
  out.WriteString(in->platform);
  out.WriteString(in->platform_version);
  out.WriteString(in->architecture);
  out.WriteString(in->model);
  out.WriteBool(in->mobile);
  return std::string(reinterpret_cast<const char*>(out.data()), out.size());
}

// static
base::Optional<UserAgentMetadata> UserAgentMetadata::Demarshal(
    const base::Optional<std::string>& encoded) {
  if (!encoded)
    return base::nullopt;

  base::Pickle pickle(encoded->data(), encoded->size());
  base::PickleIterator in(pickle);

  uint32_t version;
  UserAgentMetadata out;
  if (!in.ReadUInt32(&version) || version != kVersion)
    return base::nullopt;
  if (!in.ReadString(&out.brand))
    return base::nullopt;
  if (!in.ReadString(&out.full_version))
    return base::nullopt;
  if (!in.ReadString(&out.major_version))
    return base::nullopt;
  if (!in.ReadString(&out.platform))
    return base::nullopt;
  if (!in.ReadString(&out.platform_version))
    return base::nullopt;
  if (!in.ReadString(&out.architecture))
    return base::nullopt;
  if (!in.ReadString(&out.model))
    return base::nullopt;
  if (!in.ReadBool(&out.mobile))
    return base::nullopt;
  return base::make_optional(std::move(out));
}

bool operator==(const UserAgentMetadata& a, const UserAgentMetadata& b) {
  return a.brand == b.brand && a.full_version == b.full_version &&
         a.major_version == b.major_version && a.platform == b.platform &&
         a.platform_version == b.platform_version &&
         a.architecture == b.architecture && a.model == b.model &&
         a.mobile == b.mobile;
}

bool operator==(const UserAgentOverride& a, const UserAgentOverride& b) {
  return a.ua_string_override == b.ua_string_override &&
         a.ua_metadata_override == b.ua_metadata_override;
}

}  // namespace blink
