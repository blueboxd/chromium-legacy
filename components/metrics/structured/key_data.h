// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_

#include <string>

#include "base/optional.h"

class JsonPrefStore;

namespace metrics {
namespace structured {
namespace internal {

// KeyData is the central class for managing keys and generating hashes for
// structured metrics.
//
// The class maintains one key and its rotation data for every event defined
// in /tools/metrics/structured.xml. This can be used to generate:
//  - a user ID for the event with KeyData::UserEventId.
//  - a hash of a given value for the event with KeyData::Hash.
//
// KeyData performs key rotation. Every event is associated with a rotation
// period, which is 90 days unless specified in structured.xml. Keys are rotated
// with a resolution of one day. They are guaranteed not to be used for Hash or
// UserEventId for longer than their rotation period, except in cases of local
// clock changes.
//
// When first created, every event's key rotation date is selected uniformly so
// that there is an even distribution of rotations across users. This means
// that, for most users, the first rotation period will be shorter than the
// standard full rotation period for that event.
//
// Key storage is backed by a JsonPrefStore which is passed to the ctor and must
// outlive the KeyData instance. Within the pref store, each event has three
// pieces of associated data:
//  - the rotation period for this event in days.
//  - the day of the last key rotation, as a day since the unix epoch.
//  - the key itself.
//
// This is stored in the structure:
//   keys.{event_name_hash}.rotation_period
//                         .last_rotation
//                         .key
//
// TODO(crbug.com/1016655): add ability to override default rotation period
class KeyData {
 public:
  explicit KeyData(JsonPrefStore* key_store);
  ~KeyData();

  KeyData(const KeyData&) = delete;
  KeyData& operator=(const KeyData&) = delete;

  // Returns a digest of |value| for |metric| in the context of
  // |project_name_hash|. Terminology: a metric is a (name, value) pair, and an
  // event is a bundle of metrics.
  //
  //  - |project_name_hash| is the uint64 name hash of an event or project.
  //  - |metric| is the uint64 name hash of a metric within |project_name_hash|.
  //  - |value| is the string value to hash.
  //
  // The result is the HMAC digest of the |value| salted with |metric|, using
  // the key for |project_name_hash|. That is:
  //
  //   HMAC_SHA256(key(project_name_hash), concat(value, hex(metric)))
  //
  // Returns 0u in case of an error.
  uint64_t HashForEventMetric(uint64_t project_name_hash,
                              uint64_t metric,
                              const std::string& value);

  // Returns an ID for this (user, |project_name_hash|) pair.
  // |project_name_hash| is the name of an event or project, represented by the
  // first 8 bytes of the MD5 hash of its name defined  in structured.xml.
  //
  // The derived ID is the first 8 bytes of SHA256(key(project_name_hash)).
  // Returns 0u in case of an error.
  //
  // This ID is intended as the only ID for a particular structured metrics
  // event. However, events are uploaded from the device alongside the UMA
  // client ID, which is only removed after the event reaches the server. This
  // means events are associated with the client ID when uploaded from the
  // device. See the class comment of StructuredMetricsProvider for more
  // details.
  uint64_t UserEventId(uint64_t project_name_hash);

 private:
  int GetRotationPeriod(uint64_t event);
  void SetRotationPeriod(uint64_t event, int rotation_period);

  int GetLastRotation(uint64_t event);
  void SetLastRotation(uint64_t event, int last_rotation);

  // Ensure that a valid key exists for |event|, and return it. Either returns a
  // string of size |kKeySize| or base::nullopt, which indicates an error.
  base::Optional<std::string> ValidateAndGetKey(uint64_t project_name_hash);
  void SetKey(uint64_t event, const std::string& key);

  // Ensure that valid keys exist for all events.
  void ValidateKeys();

  // Storage for keys and rotation data. Must outlive the KeyData instance.
  JsonPrefStore* key_store_;
};

}  // namespace internal
}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_H_
