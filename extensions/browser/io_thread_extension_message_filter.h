// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_IO_THREAD_EXTENSION_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_IO_THREAD_EXTENSION_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace extensions {

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created on the UI thread, but handles messages on the
// IO thread and is destroyed there.
class IOThreadExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  IOThreadExtensionMessageFilter();

 private:
  friend class base::DeleteHelper<IOThreadExtensionMessageFilter>;
  friend class content::BrowserThread;

  ~IOThreadExtensionMessageFilter() override;

  // content::BrowserMessageFilter implementation.
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handlers on the IO thread.
  void OnExtensionGenerateUniqueID(int* unique_id);

  DISALLOW_COPY_AND_ASSIGN(IOThreadExtensionMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IO_THREAD_EXTENSION_MESSAGE_FILTER_H_
