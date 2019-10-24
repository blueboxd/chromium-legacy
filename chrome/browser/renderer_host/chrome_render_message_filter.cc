// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/common/render_messages.h"
#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/common/network_hints_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/network_isolation_key.h"
#include "ppapi/buildflags/buildflags.h"

using content::BrowserThread;

namespace {

const uint32_t kRenderFilteredMessageClasses[] = {
    ChromeMsgStart, NetworkHintsMsgStart,
};

void StartPreconnect(
    base::WeakPtr<predictors::PreconnectManager> preconnect_manager,
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allow_credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!preconnect_manager)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  net::NetworkIsolationKey network_isolation_key(
      web_contents->GetMainFrame()->GetLastCommittedOrigin(),
      render_frame_host->GetLastCommittedOrigin());
  preconnect_manager->StartPreconnectUrl(url, allow_credentials,
                                         network_isolation_key);
}

}  // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(int render_process_id,
                                                     Profile* profile)
    : BrowserMessageFilter(kRenderFilteredMessageClasses,
                           base::size(kRenderFilteredMessageClasses)),
      render_process_id_(render_process_id),
      preconnect_manager_initialized_(false) {
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor && loading_predictor->preconnect_manager()) {
    preconnect_manager_ = loading_predictor->preconnect_manager()->GetWeakPtr();
    preconnect_manager_initialized_ = true;
  }
}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_DNSPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_Preconnect, OnPreconnect)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
#if BUILDFLAG(ENABLE_PLUGINS)
  switch (message.type()) {
    case ChromeViewHostMsg_IsCrashReportingEnabled::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
#endif
}

void ChromeRenderMessageFilter::OnDnsPrefetch(
    const network_hints::LookupRequest& request) {
  if (preconnect_manager_initialized_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&predictors::PreconnectManager::StartPreresolveHosts,
                       preconnect_manager_, request.hostname_list));
  }
}

void ChromeRenderMessageFilter::OnPreconnect(int render_frame_id,
                                             const GURL& url,
                                             bool allow_credentials,
                                             int count) {
  if (count < 1) {
    LOG(WARNING) << "NetworkHintsMsg_Preconnect IPC with invalid count: "
                 << count;
    return;
  }

  if (!url.is_valid() || !url.has_host() || !url.has_scheme() ||
      !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!preconnect_manager_initialized_)
    return;

  // TODO(mmenke):  Think about enabling cross-site preconnects, though that
  // will result in at least some cross-site information leakage.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&StartPreconnect, preconnect_manager_, render_process_id_,
                     render_frame_id, url, allow_credentials));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  *enabled = ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}
#endif
