// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_controller.h"

#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_layout_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"

namespace chromecast {

WebviewController::WebviewController(content::BrowserContext* browser_context,
                                     Client* client)
    : client_(client) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  contents_ = content::WebContents::Create(create_params);
  CastWebContents::InitParams cast_contents_init;
  cast_contents_init.is_root_window = true;
  cast_contents_init.enabled_for_dev = CAST_IS_DEBUG_BUILD();
  cast_contents_init.delegate = this;
  cast_web_contents_ = std::make_unique<CastWebContentsImpl>(
      contents_.get(), cast_contents_init);

  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  auto ax_id = contents_->GetMainFrame()->GetAXTreeID().ToString();
  response->mutable_create_response()
      ->mutable_accessibility_info()
      ->set_ax_tree_id(ax_id);
  client->EnqueueSend(std::move(response));
}

WebviewController::~WebviewController() {}

void WebviewController::ProcessRequest(const webview::WebviewRequest& request) {
  switch (request.type_case()) {
    case webview::WebviewRequest::kInput:
      ProcessInputEvent(request.input());
      break;

    case webview::WebviewRequest::kNavigate:
      if (request.has_navigate()) {
        LOG(INFO) << "Navigate webview to " << request.navigate().url();
        stopped_ = false;
        cast_web_contents_->LoadUrl(GURL(request.navigate().url()));
      } else {
        client_->OnError("navigate() not supplied");
      }
      break;

    case webview::WebviewRequest::kStopPage:
      if (request.has_stop_page()) {
        cast_web_contents_->Stop(request.stop_page().error_code());
      } else {
        client_->OnError("stop_page() not supplied");
      }
      break;

    default:
      client_->OnError("Unknown request code");
      break;
  }
}

void WebviewController::ClosePage() {
  cast_web_contents_->ClosePage();
}

void WebviewController::AttachTo(aura::Window* window, int window_id) {
  auto* contents_window = contents_->GetNativeView();
  window->SetLayoutManager(new WebviewLayoutManager(contents_window));
  contents_window->set_id(window_id);
  contents_window->SetBounds(gfx::Rect(window->bounds().size()));
  contents_window->Show();
  window->AddChild(contents_->GetNativeView());
}

void WebviewController::ProcessInputEvent(const webview::InputEvent& ev) {
  ui::EventHandler* handler = contents_->GetNativeView()->delegate();
  ui::EventType type = static_cast<ui::EventType>(ev.event_type());
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_CANCELLED:
      if (ev.has_touch()) {
        auto& touch = ev.touch();
        ui::TouchEvent evt(
            type, gfx::PointF(touch.x(), touch.y()),
            gfx::PointF(touch.root_x(), touch.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ui::PointerDetails(
                static_cast<ui::EventPointerType>(touch.pointer_type()),
                static_cast<ui::PointerId>(touch.pointer_id()),
                touch.radius_x(), touch.radius_y(), touch.force(),
                touch.twist(), touch.tilt_x(), touch.tilt_y(),
                touch.tangential_pressure()),
            ev.flags());
        handler->OnTouchEvent(&evt);
      } else {
        client_->OnError("touch() not supplied for touch event");
      }
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      if (ev.has_mouse()) {
        auto& mouse = ev.mouse();
        ui::MouseEvent evt(
            type, gfx::PointF(mouse.x(), mouse.y()),
            gfx::PointF(mouse.root_x(), mouse.root_y()),
            base::TimeTicks() +
                base::TimeDelta::FromMicroseconds(ev.timestamp()),
            ev.flags(), mouse.changed_button_flags());
        handler->OnMouseEvent(&evt);
      } else {
        client_->OnError("mouse() not supplied for mouse event");
      }
      break;
    default:
      break;
  }
}

webview::AsyncPageEvent_State WebviewController::current_state() {
  // The PB enum is defined identically.
  return static_cast<webview::AsyncPageEvent_State>(
      cast_web_contents_->page_state());
}

void WebviewController::OnPageStateChanged(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_current_page_state(current_state());
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::OnPageStopped(CastWebContents* cast_web_contents,
                                      int error_code) {
  stopped_ = true;
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_current_page_state(current_state());
    event->set_stopped_error_code(error_code);
    client_->EnqueueSend(std::move(response));
  } else {
    // Can't destroy in an observer callback, so post a task to do it.
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

void WebviewController::ResourceLoadFailed(CastWebContents* cast_web_contents) {
  if (client_) {
    std::unique_ptr<webview::WebviewResponse> response =
        std::make_unique<webview::WebviewResponse>();
    auto* event = response->mutable_page_event();
    event->set_current_page_state(current_state());
    event->set_resource_load_failed(true);
    client_->EnqueueSend(std::move(response));
  }
}

void WebviewController::Destroy() {
  // This webview is now abandoned and should close.
  client_ = nullptr;
  if (stopped_) {
    // If the page has been stopped this can be deleted immediately.
    delete this;
  } else {
    // This will eventually call OnPageStopped.
    cast_web_contents_->ClosePage();
  }
}

}  // namespace chromecast
