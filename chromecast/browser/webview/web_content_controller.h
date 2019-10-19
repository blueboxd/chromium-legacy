// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_

#include <memory>
#include <string>

#include "chromecast/browser/webview/proto/webview.pb.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// Processes proto commands to control WebContents
class WebContentController : public exo::SurfaceObserver {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void EnqueueSend(
        std::unique_ptr<webview::WebviewResponse> response) = 0;
    virtual void OnError(const std::string& error_message) = 0;
  };
  WebContentController(Client* client);
  ~WebContentController() override;

  virtual void Destroy() = 0;

  virtual void ProcessRequest(const webview::WebviewRequest& request);

  // Attach this web contents to an aura window as a child.
  void AttachTo(aura::Window* window, int window_id);

 protected:
  virtual content::WebContents* GetWebContents() = 0;
  Client* client_;  // Not owned.
  bool has_navigation_delegate_ = false;

 private:
  void ProcessInputEvent(const webview::InputEvent& ev);
  void JavascriptCallback(int64_t id, base::Value result);
  void HandleEvaluateJavascript(
      int64_t id,
      const webview::EvaluateJavascriptRequest& request);
  void HandleAddJavascriptChannels(
      const webview::AddJavascriptChannelsRequest& request);
  void HandleRemoveJavascriptChannels(
      const webview::RemoveJavascriptChannelsRequest& request);
  void HandleGetCurrentUrl(int64_t id);
  void HandleCanGoBack(int64_t id);
  void HandleCanGoForward(int64_t id);
  void HandleClearCache();
  void HandleGetTitle(int64_t id);
  void HandleUpdateSettings(const webview::UpdateSettingsRequest& request);
  void HandleSetAutoMediaPlaybackPolicy(
      const webview::SetAutoMediaPlaybackPolicyRequest& request);
  viz::SurfaceId GetSurfaceId();

  // exo::SurfaceObserver
  void OnSurfaceDestroying(exo::Surface* surface) override;

  ui::GestureRecognizerImpl gesture_recognizer_;

  exo::Surface* surface_ = nullptr;

  base::WeakPtrFactory<WebContentController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebContentController);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEB_CONTENT_CONTROLLER_H_
