// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/vr/consent/win_xr_consent_helper.h"

#include "chrome/browser/vr/consent/xr_session_request_consent_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {
content::WebContents* GetWebContents(int render_process_id,
                                     int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  return web_contents;
}
}  // anonymous namespace

namespace vr {
void WinXrConsentHelper::ShowConsentPrompt(
    int render_process_id,
    int render_frame_id,
    content::XrConsentPromptLevel consent_level,
    content::OnXrUserConsentCallback response_callback) {
  XRSessionRequestConsentManager::Instance()->ShowDialogAndGetConsent(
      GetWebContents(render_process_id, render_frame_id), consent_level,
      std::move(response_callback));
}
}  // namespace vr
