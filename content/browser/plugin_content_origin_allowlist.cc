// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_content_origin_allowlist.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist::
    ~DocumentPluginContentOriginAllowlist() {}

PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist::
    DocumentPluginContentOriginAllowlist(RenderFrameHost* render_frame_host) {}

void PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist::
    InsertOrigin(const url::Origin& content_origin) {
  origins_.insert(content_origin);
}

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(
    PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist)

PluginContentOriginAllowlist::PluginContentOriginAllowlist(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

PluginContentOriginAllowlist::~PluginContentOriginAllowlist() {}

void PluginContentOriginAllowlist::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // When RenderFrame is created inside the main frame,
  DocumentPluginContentOriginAllowlist* allowlist =
      DocumentPluginContentOriginAllowlist::GetForCurrentDocument(
          render_frame_host->GetMainFrame());
  if (!allowlist || allowlist->origins().empty())
    return;
  render_frame_host->Send(new FrameMsg_UpdatePluginContentOriginAllowlist(
      render_frame_host->GetRoutingID(), allowlist->origins()));
}

bool PluginContentOriginAllowlist::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PluginContentOriginAllowlist, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PluginContentOriginAllowed,
                        OnPluginContentOriginAllowed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PluginContentOriginAllowlist::OnPluginContentOriginAllowed(
    RenderFrameHost* render_frame_host,
    const url::Origin& content_origin) {
  DocumentPluginContentOriginAllowlist* allowlist =
      GetOrCreateAllowlistForFrame(render_frame_host);
  allowlist->InsertOrigin(content_origin);

  // TODO(yuzus, crbug.com/1061899): This message should be sent to all the
  // frames in the same frame tree as |render_frame_host|. When mojofying this
  // IPC, this should use PageBroadcast interface and look up the correct set of
  // RenderViewHosts from |render_frame_host| instead of getting them from
  // |web_contents()|.
  web_contents()->SendToAllFrames(
      new FrameMsg_UpdatePluginContentOriginAllowlist(MSG_ROUTING_NONE,
                                                      allowlist->origins()));
}

PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist*
PluginContentOriginAllowlist::GetOrCreateAllowlistForFrame(
    RenderFrameHost* render_frame_host) {
  RenderFrameHost* main_frame = render_frame_host->GetMainFrame();
  if (!DocumentPluginContentOriginAllowlist::GetForCurrentDocument(
          main_frame)) {
    DocumentPluginContentOriginAllowlist::CreateForCurrentDocument(main_frame);
  }
  return DocumentPluginContentOriginAllowlist::GetForCurrentDocument(
      main_frame);
}

bool PluginContentOriginAllowlist::IsOriginAllowlistedForFrameForTesting(
    RenderFrameHost* render_frame_host,
    const url::Origin& content_origin) {
  DocumentPluginContentOriginAllowlist* allowlist =
      DocumentPluginContentOriginAllowlist::GetForCurrentDocument(
          render_frame_host->GetMainFrame());
  PluginContentOriginAllowlist::DocumentPluginContentOriginAllowlist::
      GetForCurrentDocument(render_frame_host->GetMainFrame());
  if (!allowlist)
    return false;
  return allowlist->origins().find(content_origin) !=
         allowlist->origins().end();
}

}  // namespace content
