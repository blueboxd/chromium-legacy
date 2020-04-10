// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_METRICS_WEBXR_SESSION_TRACKER_H_
#define CHROME_BROWSER_VR_METRICS_WEBXR_SESSION_TRACKER_H_

#include <memory>
#include <set>

#include "chrome/browser/vr/metrics/session_tracker.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace vr {

class VR_BASE_EXPORT WebXRSessionTracker
    : public SessionTracker<ukm::builders::XR_WebXR_Session>,
      device::mojom::XRSessionMetricsRecorder {
 public:
  explicit WebXRSessionTracker(
      std::unique_ptr<ukm::builders::XR_WebXR_Session> entry);
  ~WebXRSessionTracker() override;

  // Records which features for the session have been requested as required or
  // optional, which were accepted/rejeceted, and which weren't requested at
  // all. This assumes that the session as a whole was accepted.
  void ReportRequestedFeatures(
      const device::mojom::XRSessionOptions& session_options,
      const std::set<device::mojom::XRSessionFeature>& enabled_features);

  // |XRSessionMetricsRecorder| implementation
  void ReportFeatureUsed(device::mojom::XRSessionFeature feature) override;

  // Binds this tracker's |XRSessionMetricsRecorder| receiver to a new pipe, and
  // returns the |PendingRemote|.
  mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
  BindMetricsRecorderPipe();

 private:
  void SetFeatureRequest(device::mojom::XRSessionFeature feature,
                         device::mojom::XRSessionFeatureRequestStatus status);

  mojo::Receiver<device::mojom::XRSessionMetricsRecorder> receiver_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_METRICS_WEBXR_SESSION_TRACKER_H_
