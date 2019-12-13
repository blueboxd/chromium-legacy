// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BINDER_OVERRIDES_H_
#define SERVICES_DEVICE_BINDER_OVERRIDES_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"

namespace device {
namespace internal {

using GeolocationContextBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::GeolocationContext>)>;
COMPONENT_EXPORT(DEVICE_SERVICE_BINDER_OVERRIDES)
GeolocationContextBinder& GetGeolocationContextBinderOverride();

}  // namespace internal
}  // namespace device

#endif  // SERVICES_DEVICE_BINDER_OVERRIDES_H_
