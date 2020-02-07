// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_MESSAGES_H_
#define CHROME_COMMON_PRERENDER_MESSAGES_H_

#include "chrome/common/prerender_types.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#define IPC_MESSAGE_START PrerenderMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(prerender::PrerenderMode,
                          prerender::PRERENDER_MODE_COUNT - 1)

// PrerenderLinkManager Messages

// Sent by the renderer process to notify that the resource prefetcher has
// discovered all possible subresources and issued requests for them.
IPC_MESSAGE_CONTROL0(PrerenderHostMsg_PrefetchFinished)

// PrerenderDispatcher Messages
// These are messages sent from the browser to the renderer in relation to
// running prerenders.

// Tells a renderer if it's currently being prerendered.  Must only be set
// before any navigation occurs, and only set to NO_PRERENDER at most once after
// that.
IPC_MESSAGE_ROUTED2(PrerenderMsg_SetIsPrerendering,
                    prerender::PrerenderMode,
                    std::string /* histogram_prefix */)

#endif  // CHROME_COMMON_PRERENDER_MESSAGES_H_
