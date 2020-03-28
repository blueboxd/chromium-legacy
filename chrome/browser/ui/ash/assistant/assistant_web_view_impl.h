// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_IMPL_H_

#include "ash/public/cpp/assistant/assistant_web_view.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Implements AssistantWebView used by Ash to work around dependency
// restrictions.
class AssistantWebViewImpl : public ash::AssistantWebView,
                             public content::WebContentsDelegate,
                             public content::WebContentsObserver {
 public:
  explicit AssistantWebViewImpl(Profile* profile, const InitParams& params);
  ~AssistantWebViewImpl() override;

  AssistantWebViewImpl(AssistantWebViewImpl&) = delete;
  AssistantWebViewImpl& operator=(AssistantWebViewImpl&) = delete;

  // ash::AssistantWebView:
  const char* GetClassName() const override;
  gfx::NativeView GetNativeView() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void Layout() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool GoBack() override;
  void Navigate(const GURL& url) override;
  void AddedToWidget() override;

  // content::WebContentsDelegate:
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  bool TakeFocus(content::WebContents* web_contents, bool reverse) override;
  void NavigationStateChanged(content::WebContents* web_contents,
                              content::InvalidateTypes changed_flags) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void NavigationEntriesDeleted() override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;

 private:
  void InitWebContents(Profile* profile);
  void InitLayout(Profile* profile);

  void NotifyDidSuppressNavigation(const GURL& url,
                                   WindowOpenDisposition disposition,
                                   bool from_user_gesture);

  void UpdateCanGoBack();

  // Update the window property that stores whether we can minimize on a back
  // event.
  void UpdateMinimizeOnBackProperty();

  const InitParams params_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::WebView> web_view_;

  // Whether or not the embedded |web_contents_| can go back.
  bool can_go_back_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AssistantWebViewImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_WEB_VIEW_IMPL_H_
