// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_inspector_agent.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/sensor/sensor_inspector_agent.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

using protocol::Response;

DeviceOrientationInspectorAgent::~DeviceOrientationInspectorAgent() = default;

DeviceOrientationInspectorAgent::DeviceOrientationInspectorAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      sensor_agent_(MakeGarbageCollected<SensorInspectorAgent>(
          inspected_frames->Root()->GetDocument())),
      enabled_(&agent_state_, /*default_value=*/false),
      alpha_(&agent_state_, /*default_value=*/0.0),
      beta_(&agent_state_, /*default_value=*/0.0),
      gamma_(&agent_state_, /*default_value=*/0.0) {}

void DeviceOrientationInspectorAgent::Trace(Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(sensor_agent_);
  InspectorBaseAgent::Trace(visitor);
}

DeviceOrientationController* DeviceOrientationInspectorAgent::Controller() {
  Document* document = inspected_frames_->Root()->GetDocument();
  return document ? &DeviceOrientationController::From(*document) : nullptr;
}

Response DeviceOrientationInspectorAgent::setDeviceOrientationOverride(
    double alpha,
    double beta,
    double gamma) {
  enabled_.Set(true);
  alpha_.Set(alpha);
  beta_.Set(beta);
  gamma_.Set(gamma);
  if (Controller()) {
    Controller()->SetOverride(
        DeviceOrientationData::Create(alpha, beta, gamma, false));
  }
  sensor_agent_->SetOrientationSensorOverride(alpha, beta, gamma);
  return Response::Success();
}

Response DeviceOrientationInspectorAgent::clearDeviceOrientationOverride() {
  return disable();
}

Response DeviceOrientationInspectorAgent::disable() {
  agent_state_.ClearAllFields();
  if (Controller())
    Controller()->ClearOverride();
  sensor_agent_->Disable();
  return Response::Success();
}

void DeviceOrientationInspectorAgent::Restore() {
  if (!Controller() || !enabled_.Get())
    return;
  Controller()->SetOverride(DeviceOrientationData::Create(
      alpha_.Get(), beta_.Get(), gamma_.Get(), false));
  sensor_agent_->SetOrientationSensorOverride(alpha_.Get(), beta_.Get(),
                                              gamma_.Get());
}

void DeviceOrientationInspectorAgent::DidCommitLoadForLocalFrame(
    LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    // New document in main frame - apply override there.
    // No need to cleanup previous one, as it's already gone.
    sensor_agent_->DidCommitLoadForLocalFrame(frame);
    Restore();
  }
}

}  // namespace blink
