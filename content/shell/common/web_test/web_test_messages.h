// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_
#define CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_

#include <string>
#include <vector>

#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START WebTestMsgStart

IPC_MESSAGE_ROUTED2(WebTestHostMsg_InitiateCaptureDump,
                    bool /* should dump navigation history */,
                    bool /* should dump pixels */)

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_
