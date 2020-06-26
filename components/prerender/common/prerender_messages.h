// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_COMMON_PRERENDER_MESSAGES_H_
#define COMPONENTS_PRERENDER_COMMON_PRERENDER_MESSAGES_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#define IPC_MESSAGE_START PrerenderMsgStart

// PrerenderLinkManager Messages

// Sent by the renderer process to notify that the resource prefetcher has
// discovered all possible subresources and issued requests for them.
IPC_MESSAGE_CONTROL0(PrerenderHostMsg_PrefetchFinished)

#endif  // COMPONENTS_PRERENDER_COMMON_PRERENDER_MESSAGES_H_
