// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_

#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"

#include <memory>

namespace blink {
class WebMouseEvent;
}

namespace safe_browsing {

// Used for UMA. There may be more than one event per navigation (e.g.
// kAll and kWarningShownOnKeypress).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DelayedWarningEvent {
  // User loaded a page with a delayed warning.
  kPageLoaded = 0,
  // User left the page and the warning was never shown.
  kWarningNotShown = 1,
  // User pressed a key and the warning was shown.
  kWarningShownOnKeypress = 2,
  // User clicked on the page at least once but the feature isn't configured to
  // show warnings on mouse clicks.
  kWarningNotTriggeredOnMouseClick = 3,
  // User clicked on the page and the warning was shown.
  kWarningShownOnMouseClick = 4,
  // The page tried to enter fullscreen mode.
  kWarningShownOnFullscreenAttempt = 5,

  kMaxValue = kWarningShownOnFullscreenAttempt,
};

// Name of the histogram.
extern const char kDelayedWarningsHistogram[];

// Observes user interactions and shows an interstitial if necessary.
// Only created when an interstitial was about to be displayed but was delayed
// due to the Delayed Warnings experiment. Deleted once the interstitial is
// shown, or the tab is closed or navigated away.
class SafeBrowsingUserInteractionObserver
    : public base::SupportsUserData::Data,
      public content::WebContentsObserver {
 public:
  // Creates an observer for given |web_contents|. |resource| is the unsafe
  // resource for which a delayed interstitial will be displayed.
  // |is_main_frame| is true if the interstitial is for the top frame. If false,
  // it's for a subresource / subframe.
  // |ui_manager| is the UIManager that shows the actual warning.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      bool is_main_frame,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);

  // See CreateForWebContents() for parameters. These need to be public.
  SafeBrowsingUserInteractionObserver(
      content::WebContents* web_contents,
      const security_interstitials::UnsafeResource& resource,
      bool is_main_frame,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);
  ~SafeBrowsingUserInteractionObserver() override;

  // content::WebContentsObserver methods:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void WebContentsDestroyed() override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

 private:
  static SafeBrowsingUserInteractionObserver* FromWebContents(
      content::WebContents* web_contents);

  bool HandleKeyPress(const content::NativeWebKeyboardEvent& event);
  bool HandleMouseEvent(const blink::WebMouseEvent& event);

  void ShowInterstitial(DelayedWarningEvent event);
  void CleanUp();

  content::RenderWidgetHost::KeyPressEventCallback key_press_callback_;
  content::RenderWidgetHost::MouseEventCallback mouse_event_callback_;

  content::WebContents* web_contents_;
  security_interstitials::UnsafeResource resource_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  bool interstitial_shown_ = false;
  bool mouse_click_with_no_warning_recorded_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_USER_INTERACTION_OBSERVER_H_
