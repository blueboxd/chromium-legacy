// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"
#include "content/public/browser/system_connector.h"

namespace chromeos {

namespace secure_channel {

SecureChannelClientProvider::SecureChannelClientProvider() = default;

SecureChannelClientProvider::~SecureChannelClientProvider() = default;

// static
SecureChannelClientProvider* SecureChannelClientProvider::GetInstance() {
  static base::NoDestructor<SecureChannelClientProvider> provider;
  return provider.get();
}

SecureChannelClient* SecureChannelClientProvider::GetClient() {
  if (!secure_channel_client_) {
    secure_channel_client_ =
        SecureChannelClientImpl::Factory::Get()->BuildInstance(
            content::GetSystemConnector());
  }

  return secure_channel_client_.get();
}

}  // namespace secure_channel

}  // namespace chromeos
