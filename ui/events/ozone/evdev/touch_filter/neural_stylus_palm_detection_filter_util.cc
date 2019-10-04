// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"

namespace ui {
DistilledDevInfo::DistilledDevInfo(const EventDeviceInfo& devinfo) {
  max_x = devinfo.GetAbsMaximum(ABS_MT_POSITION_X);
  x_res = devinfo.GetAbsResolution(ABS_MT_POSITION_X);
  max_y = devinfo.GetAbsMaximum(ABS_MT_POSITION_Y);
  y_res = devinfo.GetAbsResolution(ABS_MT_POSITION_Y);
  major_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MAJOR);
  if (major_radius_res == 0) {
    // Device does not report major res: set to 1.
    major_radius_res = 1;
  }
  if (devinfo.HasAbsEvent(ABS_MT_TOUCH_MINOR)) {
    minor_radius_supported = true;
    minor_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MINOR);
  } else {
    minor_radius_supported = false;
    minor_radius_res = major_radius_res;
  }
  if (minor_radius_res == 0) {
    // Device does not report minor res: set to 1.
    minor_radius_res = 1;
  }
}

const DistilledDevInfo DistilledDevInfo::Create(
    const EventDeviceInfo& devinfo) {
  return DistilledDevInfo(devinfo);
}

Sample::Sample(const InProgressTouchEvdev& touch,
               const base::TimeTicks& time,
               const DistilledDevInfo& dev_info)
    : time(time) {  // radius_x and radius_y have been
                    // scaled by resolution.already.

  // Original model here is not normalized appropriately, so we divide by 40.
  major_radius = std::max(touch.major, touch.minor) * dev_info.x_res / 40.0 *
                 dev_info.major_radius_res;
  if (dev_info.minor_radius_supported) {
    minor_radius = std::min(touch.major, touch.minor) * dev_info.x_res / 40.0 *
                   dev_info.minor_radius_res;
  } else {
    minor_radius = major_radius;
  }

  // Nearest edge distance, in cm.
  float nearest_x_edge = std::min(touch.x, dev_info.max_x - touch.x);
  float nearest_y_edge = std::min(touch.y, dev_info.max_y - touch.y);
  float normalized_x_edge = nearest_x_edge / dev_info.x_res;
  float normalized_y_edge = nearest_y_edge / dev_info.y_res;
  edge = std::min(normalized_x_edge, normalized_y_edge);
  point = gfx::PointF(touch.x / dev_info.x_res, touch.y / dev_info.y_res);
  tracking_id = touch.tracking_id;
  pressure = touch.pressure;
}

Sample::Sample(const Sample& other) = default;
Sample& Sample::operator=(const Sample& other) = default;

Stroke::Stroke(const Stroke& other) = default;
Stroke::Stroke(Stroke&& other) = default;
Stroke::Stroke(int max_length) : max_length_(max_length) {}
Stroke::~Stroke() {}

void Stroke::AddSample(const Sample& samp) {
  if (samples_.empty()) {
    tracking_id_ = samp.tracking_id;
  }
  DCHECK_EQ(tracking_id_, samp.tracking_id);
  samples_.push_back(samp);
  while (samples_.size() > max_length_) {
    samples_.pop_front();
  }
}

gfx::PointF Stroke::GetCentroid() const {
  // TODO(robsc): Implement a Kahan sum to accurately track running sum instead
  // of brute force.
  if (samples_.size() == 0) {
    return gfx::PointF(0., 0.);
  }
  gfx::PointF unscaled_centroid;
  for (const auto& sample : samples_) {
    unscaled_centroid += sample.point.OffsetFromOrigin();
  }
  return gfx::ScalePoint(unscaled_centroid, 1.f / samples_.size());
}

int Stroke::tracking_id() const {
  return tracking_id_;
}

float Stroke::BiggestSize() const {
  float biggest = 0;
  for (const auto& sample : samples_) {
    float size;
    if (sample.minor_radius <= 0) {
      size = sample.major_radius * sample.major_radius;
    } else {
      size = sample.major_radius * sample.minor_radius;
    }
    biggest = std::max(biggest, size);
  }
  return biggest;
}

}  // namespace ui
