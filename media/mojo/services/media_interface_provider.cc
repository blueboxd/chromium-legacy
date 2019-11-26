// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_interface_provider.h"

namespace media {

MediaInterfaceProvider::MediaInterfaceProvider(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
    : receiver_(this, std::move(receiver)) {}

MediaInterfaceProvider::~MediaInterfaceProvider() = default;

void MediaInterfaceProvider::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.BindInterface(interface_name, std::move(handle));
}

}  // namespace media
