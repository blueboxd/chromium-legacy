// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_

#include "base/values.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START PeerConnectionTrackerMsgStart

// Messages sent from PeerConnectionTracker to PeerConnectionTrackerHost.
IPC_MESSAGE_CONTROL2(PeerConnectionTrackerHost_AddStandardStats,
                     int /* lid */,
                     base::ListValue /* value */)
IPC_MESSAGE_CONTROL2(PeerConnectionTrackerHost_AddLegacyStats,
                     int /* lid */,
                     base::ListValue /* value */)

// Messages sent to PeerConnectionTracker.
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_GetStandardStats)
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_GetLegacyStats)
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_OnSuspend)
IPC_MESSAGE_CONTROL2(PeerConnectionTracker_StartEventLog,
                     int /* peer_connection_local_id */,
                     int /* output_period_ms */)
IPC_MESSAGE_CONTROL1(PeerConnectionTracker_StopEventLog,
                     int /* peer_connection_local_id */)

#endif  // CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_
