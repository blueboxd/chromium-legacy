// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/crash_keys.h"

#include "base/stl_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace network {
namespace debug {

namespace {

base::debug::CrashKeyString* GetRequestUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetRequestInitiatorCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator", base::debug::CrashKeySize::Size64);
  return crash_key;
}

}  // namespace

ScopedRequestCrashKeys::ScopedRequestCrashKeys(
    const network::ResourceRequest& request)
    : url_(GetRequestUrlCrashKey(), request.url.possibly_invalid_spec()),
      request_initiator_(GetRequestInitiatorCrashKey(),
                         base::OptionalOrNullptr(request.request_initiator)) {}

ScopedRequestCrashKeys::~ScopedRequestCrashKeys() = default;

}  // namespace debug
}  // namespace network
