// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_CONTENT_ORIGIN_ALLOWLIST_H_
#define CONTENT_BROWSER_PLUGIN_CONTENT_ORIGIN_ALLOWLIST_H_

#include <set>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {

class WebContents;

class CONTENT_EXPORT PluginContentOriginAllowlist : public WebContentsObserver {
  // This class manages the lists of document-tied temporarily allowlisted
  // plugin content origins that are exempt from power saving.
  //
  // RenderFrames report content origins that should be allowlisted via IPC.
  // This class aggregates those origins and broadcasts the total list to all
  // RenderFrames inside the same RenderView. This class also sends these
  // origins to any newly created RenderFrames.
 public:
  explicit PluginContentOriginAllowlist(WebContents* web_contents);
  ~PluginContentOriginAllowlist() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PluginContentOriginAllowlistTest,
                           ClearAllowlistOnNavigate);
  FRIEND_TEST_ALL_PREFIXES(PluginContentOriginAllowlistTest,
                           SubframeInheritsAllowlist);

  class DocumentPluginContentOriginAllowlist
      : public RenderDocumentHostUserData<
            DocumentPluginContentOriginAllowlist> {
    // This class manages the list of temporarily allowlisted plugin content
    // origins that are exempt from power saving. The list is managed
    // per-document and cleared upon document destruction, as this is part of
    // the RenderDocumentHostUserData.
   public:
    ~DocumentPluginContentOriginAllowlist() override;
    void InsertOrigin(const url::Origin& content_origin);
    std::set<url::Origin> origins() { return origins_; }

   private:
    explicit DocumentPluginContentOriginAllowlist(
        RenderFrameHost* render_frame_host);
    friend class RenderDocumentHostUserData<
        DocumentPluginContentOriginAllowlist>;

    std::set<url::Origin> origins_;

    RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();
  };

  static DocumentPluginContentOriginAllowlist* GetOrCreateAllowlistForFrame(
      RenderFrameHost* render_frame_host);

  static bool IsOriginAllowlistedForFrameForTesting(
      RenderFrameHost* render_frame_host,
      const url::Origin& content_origin);

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  void OnPluginContentOriginAllowed(RenderFrameHost* render_frame_host,
                                    const url::Origin& content_origin);

  DISALLOW_COPY_AND_ASSIGN(PluginContentOriginAllowlist);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PLUGIN_CONTENT_ORIGIN_ALLOWLIST_H_
