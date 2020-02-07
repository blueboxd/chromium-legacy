// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace IPC {
class Message;
}

namespace prerender {

class PrerenderMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit PrerenderMessageFilter(int render_process_id);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<PrerenderMessageFilter>;

  ~PrerenderMessageFilter() override;

  // Overridden from content::BrowserMessageFilter.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  void OnPrefetchFinished();

  const int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderMessageFilter);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_MESSAGE_FILTER_H_

