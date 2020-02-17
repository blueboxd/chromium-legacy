// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_message_filter.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/common/web_test/web_test_messages.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/test/mock_platform_notification_service.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/origin.h"

namespace content {

WebTestMessageFilter::WebTestMessageFilter(int render_process_id)
    : BrowserMessageFilter(WebTestMsgStart),
      render_process_id_(render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebTestMessageFilter::~WebTestMessageFilter() {}

void WebTestMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

scoped_refptr<base::SequencedTaskRunner>
WebTestMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case WebTestHostMsg_InitiateCaptureDump::ID:
      return base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  }
  return nullptr;
}

bool WebTestMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebTestMessageFilter, message)
    IPC_MESSAGE_HANDLER(WebTestHostMsg_InitiateCaptureDump,
                        OnInitiateCaptureDump)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebTestMessageFilter::OnInitiateCaptureDump(
    bool capture_navigation_history,
    bool capture_pixels) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnInitiateCaptureDump(
        capture_navigation_history, capture_pixels);
  }
}

void WebTestMessageFilter::OnSetFilePathForMockFileDialog(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->SetFilePathForMockFileDialog(path);
}

}  // namespace content
