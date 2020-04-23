// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_
#define WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller.h"
#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace error_page {
class Error;
}

namespace weblayer {

// A class that allows error pages to handle user interaction by handling their
// javascript commands. Currently only SSL and safebrowsing related
// interstitials are supported.
// This is a stripped down version of Chrome's NetErrorHelper.
// TODO(crbug.com/1073624): Share this logic with NetErrorHelper.
class ErrorPageHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ErrorPageHelper>,
      public content::RenderThreadObserver,
      public security_interstitials::SecurityInterstitialPageController::
          Delegate {
 public:
  // Creates an ErrorPageHelper which will observe and tie its lifetime to
  // |render_frame|, if it's a main frame. ErrorPageHelpers will not be created
  // for sub frames.
  static void Create(content::RenderFrame* render_frame);

  // Returns the ErrorPageHelper for the frame, if it exists.
  static ErrorPageHelper* GetForFrame(content::RenderFrame* render_frame);

  // Called when the current navigation results in an error.
  void PrepareErrorPage(const error_page::Error& error, bool was_failed_post);

  // Returns whether a load for the frame the ErrorPageHelper is attached to
  // should have its error page suppressed.
  bool ShouldSuppressErrorPage();

  // content::RenderFrameObserver:
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void OnStop() override;
  void WasShown() override;
  void WasHidden() override;
  void OnDestruct() override;

  // content::RenderThreadObserver:
  void NetworkStateChanged(bool online) override;

  // security_interstitials::SecurityInterstitialPageController::Delegate:
  void SendCommand(
      security_interstitials::SecurityInterstitialCommand command) override;

  mojo::AssociatedRemote<security_interstitials::mojom::InterstitialCommands>
  GetInterface() override;

 private:
  struct ErrorPageInfo;

  explicit ErrorPageHelper(content::RenderFrame* render_frame);
  ~ErrorPageHelper() override;

  void Reload();
  void MaybeStartAutoReloadTimer();
  void StartAutoReloadTimer();
  void AutoReloadTimerFired();
  void PauseAutoReloadTimer();
  void CancelPendingReload();

  // Information for the provisional / "pre-provisional" error page. Null when
  // there's no page pending, or the pending page is not an error page.
  std::unique_ptr<ErrorPageInfo> pending_error_page_info_;

  // Information for the committed error page. Null when the committed page is
  // not an error page.
  std::unique_ptr<ErrorPageInfo> committed_error_page_info_;

  // Timer used to wait for auto-reload attempts.
  base::OneShotTimer auto_reload_timer_;

  // True if the auto-reload timer would be running but is waiting for an
  // offline->online network transition.
  bool auto_reload_paused_ = false;

  // Whether an auto-reload-initiated Reload() attempt is in flight.
  bool auto_reload_in_flight_ = false;

  // True if there is an uncommitted-but-started load, error page or not. This
  // is used to inhibit starting auto-reload when an error page finishes, in
  // case this happens:
  //   Error page starts
  //   Error page commits
  //   Non-error page starts
  //   Error page finishes
  bool uncommitted_load_started_ = false;

  // Is the browser online?
  bool online_ = false;

  int auto_reload_count_ = 0;

  base::WeakPtrFactory<ErrorPageHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ErrorPageHelper);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_ERROR_PAGE_HELPER_H_
