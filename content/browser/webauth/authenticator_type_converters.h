// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_

#include "device/fido/fido_constants.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

// TODO(hongjunchoi): Remove type converters and instead expose mojo interface
// directly from device/fido service.
// See: https://crbug.com/831209
namespace mojo {

template <>
struct TypeConverter<::device::ProtocolVersion,
                     ::blink::test::mojom::ClientToAuthenticatorProtocol> {
  static ::device::ProtocolVersion Convert(
      const ::blink::test::mojom::ClientToAuthenticatorProtocol& input);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
