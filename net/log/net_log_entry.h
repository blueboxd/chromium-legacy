// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_NET_LOG_ENTRY_H_
#define NET_LOG_NET_LOG_ENTRY_H_

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"

namespace base {
class Value;
}

namespace net {

struct NET_EXPORT NetLogEntry {
 public:
  NetLogEntry(NetLogEventType type,
              NetLogSource source,
              NetLogEventPhase phase,
              base::TimeTicks time,
              base::Value params);

  ~NetLogEntry();

  // Serializes the specified event to a Value.
  base::Value ToValue() const;

  const NetLogEventType type;
  const NetLogSource source;
  const NetLogEventPhase phase;
  const base::TimeTicks time;

  // TODO(eroman): Make this base::Optional instead?
  const base::Value params;
};

}  // namespace net

#endif  // NET_LOG_NET_LOG_ENTRY_H_
