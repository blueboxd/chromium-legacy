// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_access_delegate_impl.h"

#include "net/cookies/cookie_util.h"

namespace network {

CookieAccessDelegateImpl::CookieAccessDelegateImpl(
    const CookieSettings* cookie_settings)
    : cookie_settings_(cookie_settings) {}

CookieAccessDelegateImpl::~CookieAccessDelegateImpl() = default;

net::CookieAccessSemantics CookieAccessDelegateImpl::GetAccessSemantics(
    const net::CanonicalCookie& cookie) const {
  return cookie_settings_->GetCookieAccessSemanticsForDomain(cookie.Domain());
}

}  // namespace network
