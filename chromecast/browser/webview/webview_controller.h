// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_

#include <memory>
#include <string>

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/webview/proto/webview.pb.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {
class CastWebContents;
}  // namespace chromecast

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace chromecast {

// This owns a WebContents and CastWebContents and processes proto commands
// to allow the web contents to be controlled and embedded.
class WebviewController : public CastWebContents::Delegate,
                          public CastWebContents::Observer {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void EnqueueSend(
        std::unique_ptr<webview::WebviewResponse> response) = 0;
    virtual void OnError(const std::string& error_message) = 0;
  };
  WebviewController(content::BrowserContext* browser_context, Client* client);
  ~WebviewController() override;

  // Cause the controller to be destroyed after giving the webpage a chance to
  // run unload events. This unsets the client so no more messages will be
  // sent.
  void Destroy();

  // Close the page. This will cause a stopped response to eventually be sent.
  void ClosePage();

  void ProcessRequest(const webview::WebviewRequest& request);

  // Attach this web contents to an aura window as a child.
  void AttachTo(aura::Window* window, int window_id);

 private:
  webview::AsyncPageEvent_State current_state();

  void ProcessInputEvent(const webview::InputEvent& ev);

  bool Check(bool condition, const char* error);

  // CastWebContents::Delegate
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;
  void OnPageStopped(CastWebContents* cast_web_contents,
                     int error_code) override;

  // CastWebContents::Observer
  void ResourceLoadFailed(CastWebContents* cast_web_contents) override;

  Client* client_;  // Not owned.
  std::unique_ptr<content::WebContents> contents_;
  std::unique_ptr<CastWebContents> cast_web_contents_;
  bool stopped_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebviewController);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_CONTROLLER_H_
