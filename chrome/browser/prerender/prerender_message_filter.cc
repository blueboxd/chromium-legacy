// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_message_filter.h"

#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/browser/render_process_host.h"

namespace prerender {

PrerenderMessageFilter::PrerenderMessageFilter(int render_process_id)
    : BrowserMessageFilter(PrerenderMsgStart),
      render_process_id_(render_process_id) {}

PrerenderMessageFilter::~PrerenderMessageFilter() = default;

bool PrerenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrerenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(PrerenderHostMsg_PrefetchFinished, OnPrefetchFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PrerenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PrerenderHostMsg_PrefetchFinished::ID)
    *thread = content::BrowserThread::UI;
}

void PrerenderMessageFilter::OnPrefetchFinished() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return;

  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(
          render_process_host->GetBrowserContext());
  if (!prerender_manager)
    return;

  // Kill the process doing the prefetch. Only one prefetch per renderer is
  // possible, also prefetches are not shared with other renderer processes.
  PrerenderContents* prerender_contents =
      prerender_manager->GetPrerenderContentsForProcess(render_process_id_);
  if (prerender_contents)
    prerender_contents->Destroy(FINAL_STATUS_NOSTATE_PREFETCH_FINISHED);
}

}  // namespace prerender

