// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_
#include <deque>
#include <vector>

#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
struct EVENTS_OZONE_EVDEV_EXPORT DistilledDevInfo {
 private:
  explicit DistilledDevInfo(const EventDeviceInfo& devinfo);

 public:
  static const DistilledDevInfo Create(const EventDeviceInfo& devinfo);
  DistilledDevInfo() = delete;
  float max_x = 0.f;
  float max_y = 0.f;
  float x_res = 1.f;
  float y_res = 1.f;
  float major_radius_res = 1.f;
  float minor_radius_res = 1.f;
  bool minor_radius_supported = false;
};

// Data for a single touch event.
class EVENTS_OZONE_EVDEV_EXPORT Sample {
 public:
  Sample(const InProgressTouchEvdev& touch,
         const base::TimeTicks& time,
         const DistilledDevInfo& dev_info);
  Sample(const Sample& other);
  Sample() = delete;
  Sample& operator=(const Sample& other);

  float major_radius = 0;
  float minor_radius = 0;
  float pressure = 0;
  float edge = 0;
  int tracking_id = 0;
  gfx::PointF point;
  base::TimeTicks time = base::TimeTicks::UnixEpoch();
};

class EVENTS_OZONE_EVDEV_EXPORT Stroke {
 public:
  explicit Stroke(int max_length);
  Stroke(const Stroke& other);
  Stroke(Stroke&& other);
  virtual ~Stroke();

  void AddSample(const Sample& sample);
  gfx::PointF GetCentroid() const;
  float BiggestSize() const;
  const std::deque<Sample>& samples() const;
  int tracking_id() const;

 private:
  std::deque<Sample> samples_;
  int tracking_id_ = 0;
  unsigned long max_length_;
};

}  // namespace ui
#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_NEURAL_STYLUS_PALM_DETECTION_FILTER_UTIL_H_
