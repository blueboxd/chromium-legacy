// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

ChromePopupNavigationDelegate::ChromePopupNavigationDelegate(
    NavigateParams params)
    : params_(std::move(params)),
      original_user_gesture_(params_.user_gesture) {}

content::RenderFrameHost* ChromePopupNavigationDelegate::GetOpener() {
  return params_.opener;
}

bool ChromePopupNavigationDelegate::GetOriginalUserGesture() {
  return original_user_gesture_;
}

const GURL& ChromePopupNavigationDelegate::GetURL() {
  return params_.url;
}

blocked_content::PopupNavigationDelegate::NavigateResult
ChromePopupNavigationDelegate::NavigateWithGesture(
    const blink::mojom::WindowFeatures& window_features,
    base::Optional<WindowOpenDisposition> updated_disposition) {
  params_.user_gesture = true;
  if (updated_disposition)
    params_.disposition = updated_disposition.value();
#if defined(OS_ANDROID)
  TabModelList::HandlePopupNavigation(&params_);
#else
  ::Navigate(&params_);
#endif
  if (params_.navigated_or_inserted_contents &&
      params_.disposition == WindowOpenDisposition::NEW_POPUP) {
    content::RenderFrameHost* host =
        params_.navigated_or_inserted_contents->GetMainFrame();
    DCHECK(host);
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(window_features.Clone());
  }
  return NavigateResult{params_.navigated_or_inserted_contents,
                        params_.disposition};
}

void ChromePopupNavigationDelegate::OnPopupBlocked(
    content::WebContents* web_contents,
    int total_popups_blocked_on_page) {
#if defined(OS_ANDROID)
  // Should replace existing popup infobars, with an updated count of how many
  // popups have been blocked.
  PopupBlockedInfoBarDelegate::Create(web_contents,
                                      total_popups_blocked_on_page);
#endif
}
