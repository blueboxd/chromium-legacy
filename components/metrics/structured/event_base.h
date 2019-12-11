// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_

#include <string>
#include <utility>
#include <vector>

namespace metrics {
namespace structured {

// A base class for generated structured metrics event objects. This class
// should not be used directly.
class EventBase {
 public:
  EventBase(const EventBase& other);
  virtual ~EventBase();

  // Finalizes the event and sends it for recording. After this call, the event
  // is left in an invalid state and should not be used further.
  void Record();

 protected:
  EventBase();

  // Specifies which value type a Metric object holds.
  enum class MetricType {
    STRING = 0,
    INT = 1,
  };

  // Stores all information about a single metric: name hash, value, and a
  // specifier of the value type.
  struct Metric {
    Metric(uint64_t name_hash, MetricType type);
    ~Metric();

    // First 8 bytes of the MD5 hash of the metric name, as defined in
    // structured.xml. This is calculated by
    // tools/metrics/structured/codegen.py.
    uint64_t name_hash;
    MetricType type;

    // TODO(crbug.com/10116655): Replace this with a base::Value.
    // All possible value types a metric can take. Exactly one of these should
    // be set. If |string_value| is set (with |type| as MetricType::STRING),
    // only the HMAC digest will be reported, so it is safe to put any value
    // here.
    std::string string_value;
    int int_value;
  };

  void AddStringMetric(uint64_t name_hash, const std::string& value) {
    Metric metric(name_hash, MetricType::STRING);
    metric.string_value = value;
    metrics_.push_back(metric);
  }

  void AddIntMetric(uint64_t name_hash, int value) {
    Metric metric(name_hash, MetricType::INT);
    metric.int_value = value;
    metrics_.push_back(metric);
  }

  // First 8 bytes of the MD5 hash of the event name, as defined in
  // structured.xml. This is calculated by tools/metrics/structured/codegen.py.
  uint64_t event_name_hash_;
  std::vector<Metric> metrics_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_BASE_H_
