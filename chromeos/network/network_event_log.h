// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
#define CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"

namespace chromeos {

class NetworkState;

// Returns a consistent network identifier for logs. If |network| is null
// returns "<none>".
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkId(const NetworkState* network);

// Returns a consistent network identifier for logs. Looks up the network by
// |service_path|. If no network is found, returns service_x for /service/x.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkPathId(const std::string& service_path);

// Returns a consistent network identifier for logs. Looks up the network by
// |guid|. If no network is found, returns |guid|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string NetworkGuidId(const std::string& guid);

// Namespace for functions for logging network events.
namespace network_event_log {

namespace internal {

// Adds an entry to the event log at level specified by |log_level|.
// A maximum number of events are recorded after which new events replace
// old ones. Error events are prioritized such that error events will only be
// deleted if more than least half of the entries are errors (at which point
// the oldest error entry will be replaced). Does nothing unless Initialize()
// has been called. NOTE: Generally use NET_LOG instead.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
void AddEntry(const char* file,
              int file_line,
              device_event_log::LogLevel log_level,
              const std::string& event,
              const std::string& description);

}  // namespace internal

// Errors
#define NET_LOG_ERROR(event, desc) \
  NET_LOG_LEVEL(::device_event_log::LOG_LEVEL_ERROR, event, desc)

// Chrome initiated events, e.g. connection requests, scan requests,
// configuration removal (either from the UI or from ONC policy application).
#define NET_LOG_USER(event, desc) \
  NET_LOG_LEVEL(::device_event_log::LOG_LEVEL_USER, event, desc)

// Important events, e.g. state updates
#define NET_LOG_EVENT(event, desc) \
  NET_LOG_LEVEL(::device_event_log::LOG_LEVEL_EVENT, event, desc)

// Non-essential debugging events
#define NET_LOG_DEBUG(event, desc) \
  NET_LOG_LEVEL(::device_event_log::LOG_LEVEL_DEBUG, event, desc)

// Macro to include file and line number info in the event log.
#define NET_LOG_LEVEL(log_level, event, description)                       \
  ::device_event_log::AddEntryWithDescription(                             \
      __FILE__, __LINE__, ::device_event_log::LOG_TYPE_NETWORK, log_level, \
      event, description)

}  // namespace network_event_log

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
