// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"

namespace ui {

PalmFilterDeviceInfo CreatePalmFilterDeviceInfo(
    const EventDeviceInfo& devinfo) {
  PalmFilterDeviceInfo info;

  info.max_x = devinfo.GetAbsMaximum(ABS_MT_POSITION_X);
  info.x_res = devinfo.GetAbsResolution(ABS_MT_POSITION_X);
  info.max_y = devinfo.GetAbsMaximum(ABS_MT_POSITION_Y);
  info.y_res = devinfo.GetAbsResolution(ABS_MT_POSITION_Y);
  if (info.x_res == 0) {
    info.x_res = 1;
  }
  if (info.y_res == 0) {
    info.y_res = 1;
  }

  info.major_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MAJOR);
  if (info.major_radius_res == 0) {
    // Device does not report major res: set to 1.
    info.major_radius_res = 1;
  }
  if (devinfo.HasAbsEvent(ABS_MT_TOUCH_MINOR)) {
    info.minor_radius_supported = true;
    info.minor_radius_res = devinfo.GetAbsResolution(ABS_MT_TOUCH_MINOR);
  } else {
    info.minor_radius_supported = false;
    info.minor_radius_res = info.major_radius_res;
  }
  if (info.minor_radius_res == 0) {
    // Device does not report minor res: set to 1.
    info.minor_radius_res = 1;
  }

  return info;
}

PalmFilterSample CreatePalmFilterSample(const InProgressTouchEvdev& touch,
                                        const base::TimeTicks& time,
                                        const PalmFilterDeviceInfo& dev_info) {
  // radius_x and radius_y have been
  // scaled by resolution already.

  PalmFilterSample sample;
  sample.time = time;

  // Original model here is not normalized appropriately, so we divide by 40.
  sample.major_radius = std::max(touch.major, touch.minor) * dev_info.x_res /
                        40.0 * dev_info.major_radius_res;
  if (dev_info.minor_radius_supported) {
    sample.minor_radius = std::min(touch.major, touch.minor) * dev_info.x_res /
                          40.0 * dev_info.minor_radius_res;
  } else {
    sample.minor_radius = sample.major_radius;
  }

  // Nearest edge distance, in cm.
  float nearest_x_edge = std::min(touch.x, dev_info.max_x - touch.x);
  float nearest_y_edge = std::min(touch.y, dev_info.max_y - touch.y);
  float normalized_x_edge = nearest_x_edge / dev_info.x_res;
  float normalized_y_edge = nearest_y_edge / dev_info.y_res;
  sample.edge = std::min(normalized_x_edge, normalized_y_edge);
  sample.point =
      gfx::PointF(touch.x / dev_info.x_res, touch.y / dev_info.y_res);
  sample.tracking_id = touch.tracking_id;
  sample.pressure = touch.pressure;

  return sample;
}

PalmFilterStroke::PalmFilterStroke(size_t max_length)
    : max_length_(max_length) {}
PalmFilterStroke::PalmFilterStroke(const PalmFilterStroke& other) = default;
PalmFilterStroke::PalmFilterStroke(PalmFilterStroke&& other) = default;
PalmFilterStroke& PalmFilterStroke::operator=(const PalmFilterStroke& other) =
    default;
PalmFilterStroke& PalmFilterStroke::operator=(PalmFilterStroke&& other) =
    default;
PalmFilterStroke::~PalmFilterStroke() {}

void PalmFilterStroke::AddSample(const PalmFilterSample& sample) {
  samples_seen_++;
  if (samples_.empty()) {
    tracking_id_ = sample.tracking_id;
  }
  DCHECK_EQ(tracking_id_, sample.tracking_id);
  samples_.push_back(sample);
  while (samples_.size() > max_length_) {
    samples_.pop_front();
  }
}

gfx::PointF PalmFilterStroke::GetCentroid() const {
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

const std::deque<PalmFilterSample>& PalmFilterStroke::samples() const {
  return samples_;
}

int PalmFilterStroke::tracking_id() const {
  return tracking_id_;
}

uint64_t PalmFilterStroke::samples_seen() const {
  return samples_seen_;
}

void PalmFilterStroke::SetTrackingId(int tracking_id) {
  tracking_id_ = tracking_id;
}

float PalmFilterStroke::MaxMajorRadius() const {
  float maximum = 0.0;
  for (const auto& sample : samples_) {
    maximum = std::max(maximum, sample.major_radius);
  }
  return maximum;
}

float PalmFilterStroke::BiggestSize() const {
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
