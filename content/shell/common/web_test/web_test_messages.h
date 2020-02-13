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

IPC_MESSAGE_ROUTED0(WebTestHostMsg_ClearAllDatabases)
IPC_MESSAGE_ROUTED1(WebTestHostMsg_SetDatabaseQuota, int /* quota */)
IPC_MESSAGE_ROUTED2(WebTestHostMsg_InitiateCaptureDump,
                    bool /* should dump navigation history */,
                    bool /* should dump pixels */)

// Used send flag changes to renderers - either when
// 1) broadcasting change happening in one renderer to all other renderers, or
// 2) sending accumulated changes to a single new renderer.
IPC_MESSAGE_CONTROL1(WebTestMsg_ReplicateWebTestRuntimeFlagsChanges,
                     base::DictionaryValue /* changed_web_test_runtime_flags */)

#endif  // CONTENT_SHELL_COMMON_WEB_TEST_WEB_TEST_MESSAGES_H_
