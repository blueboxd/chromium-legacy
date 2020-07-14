// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#define CHROME_COMMON_RENDER_MESSAGES_H_

#include <string>

#include "chrome/common/web_application_info_provider_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

// Singly-included section for enums and custom IPC traits.
#ifndef INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_
#define INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_


#endif  // INTERNAL_CHROME_COMMON_RENDER_MESSAGES_H_

#define IPC_MESSAGE_START ChromeMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the render frame to load all blocked plugins with the given identifier.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_LoadBlockedPlugins,
                    std::string /* identifier */)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

// Tells the browser to open a PDF file in a new tab. Used when no PDF Viewer is
// available, and user clicks to view PDF.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_OpenPDF, GURL /* url */)

#endif  // CHROME_COMMON_RENDER_MESSAGES_H_
