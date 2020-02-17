// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class WebTestMessageFilter : public BrowserMessageFilter {
 public:
  explicit WebTestMessageFilter(int render_process_id);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebTestMessageFilter>;

  ~WebTestMessageFilter() override;

  // BrowserMessageFilter implementation.
  void OnDestruct() const override;
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnInitiateCaptureDump(bool capture_navigation_history,
                             bool capture_pixels);
  void OnSetFilePathForMockFileDialog(const base::FilePath& path);

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(WebTestMessageFilter);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_MESSAGE_FILTER_H_
