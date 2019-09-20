// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
#define SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_

#include "base/component_export.h"
#include "net/cookies/cookie_access_delegate.h"
#include "services/network/cookie_settings.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) CookieAccessDelegateImpl
    : public net::CookieAccessDelegate {
 public:
  // |cookie_settings| contains the set of content settings that describes which
  // cookies should be subject to legacy access rules.
  // |cookie_settings| is expected to outlive this class.
  explicit CookieAccessDelegateImpl(const CookieSettings* cookie_settings);

  ~CookieAccessDelegateImpl() override;

  // net::CookieAccessDelegate implementation:
  net::CookieAccessSemantics GetAccessSemantics(
      const net::CanonicalCookie& cookie) const override;

 private:
  const CookieSettings* const cookie_settings_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_ACCESS_DELEGATE_IMPL_H_
