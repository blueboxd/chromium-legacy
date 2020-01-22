// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_
#define CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START BlinkTestMsgStart

IPC_MESSAGE_ROUTED0(BlinkTestHostMsg_ResetDone)

// WebTestDelegate related.
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_OverridePreferences,
                    content::WebPreferences /* preferences */)
IPC_MESSAGE_ROUTED1(BlinkTestHostMsg_PrintMessage, std::string /* message */)

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_BLINK_TEST_MESSAGES_H_
