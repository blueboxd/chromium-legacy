// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_geolocation_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/installable/installed_webapp_geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

InstalledWebappGeolocationBridge::InstalledWebappGeolocationBridge(
    mojo::PendingReceiver<Geolocation> receiver,
    const GURL& origin,
    InstalledWebappGeolocationContext* context)
    : context_(context),
      origin_(origin),
      high_accuracy_(false),
      has_position_to_report_(false),
      receiver_(this, std::move(receiver)) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&InstalledWebappGeolocationBridge::OnConnectionError,
                     base::Unretained(this)));
}

InstalledWebappGeolocationBridge::~InstalledWebappGeolocationBridge() {
  StopUpdates();
}

void InstalledWebappGeolocationBridge::StartListeningForUpdates() {
  // TODO(crbug.com/1069506) Add implementation.
}

void InstalledWebappGeolocationBridge::StopUpdates() {
  // TODO(crbug.com/1069506) Add implementation.
}

void InstalledWebappGeolocationBridge::SetHighAccuracy(bool high_accuracy) {
  high_accuracy_ = high_accuracy;

  if (device::ValidateGeoposition(position_override_)) {
    OnLocationUpdate(position_override_);
    return;
  }

  StartListeningForUpdates();
}

void InstalledWebappGeolocationBridge::QueryNextPosition(
    QueryNextPositionCallback callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = std::move(callback);

  if (has_position_to_report_)
    ReportCurrentPosition();
}

void InstalledWebappGeolocationBridge::SetOverride(
    const device::mojom::Geoposition& position) {
  // TODO(crbug.com/1069506) Add implementation.
}

void InstalledWebappGeolocationBridge::ClearOverride() {
  // TODO(crbug.com/1069506) Add implementation.
}

void InstalledWebappGeolocationBridge::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void InstalledWebappGeolocationBridge::OnLocationUpdate(
    const device::mojom::Geoposition& position) {
  DCHECK(context_);

  current_position_ = position;
  current_position_.valid = device::ValidateGeoposition(position);
  has_position_to_report_ = true;

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void InstalledWebappGeolocationBridge::ReportCurrentPosition() {
  DCHECK(position_callback_);
  std::move(position_callback_).Run(current_position_.Clone());
  has_position_to_report_ = false;
}
