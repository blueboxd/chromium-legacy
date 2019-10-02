// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_controller.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_contents_impl.h"
#include "chromecast/browser/webview/proto/webview.pb.h"
#include "chromecast/browser/webview/webview_layout_manager.h"
#include "chromecast/browser/webview/webview_navigation_throttle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"

namespace chromecast {

namespace {

const void* kWebviewResponseUserDataKey = &kWebviewResponseUserDataKey;

class WebviewUserData : public base::SupportsUserData::Data {
 public:
  explicit WebviewUserData(WebviewController* controller);
  ~WebviewUserData() override;

  std::unique_ptr<Data> Clone() override;

  WebviewController* controller() const { return controller_; }

 private:
  WebviewController* controller_;
};

}  // namespace

WebviewController::WebviewController(content::BrowserContext* browser_context,
                                     Client* client)
    : client_(client) {
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  contents_ = content::WebContents::Create(create_params);
  contents_->SetUserData(kWebviewResponseUserDataKey,
                         std::make_unique<WebviewUserData>(this));
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

std::unique_ptr<content::NavigationThrottle>
WebviewController::MaybeGetNavigationThrottle(
    content::NavigationHandle* handle) {
  auto* web_contents = handle->GetWebContents();
  auto* webview_user_data = static_cast<WebviewUserData*>(
      web_contents->GetUserData(kWebviewResponseUserDataKey));
  if (webview_user_data &&
      webview_user_data->controller()->has_navigation_delegate_) {
    return std::make_unique<WebviewNavigationThrottle>(
        handle, webview_user_data->controller());
  }
  return nullptr;
}

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

    case webview::WebviewRequest::kEvaluateJavascript:
      if (request.has_evaluate_javascript()) {
        HandleEvaluateJavascript(request.id(), request.evaluate_javascript());
      } else {
        client_->OnError("evaluate_javascript() not supplied");
      }
      break;

    case webview::WebviewRequest::kAddJavascriptChannels:
      if (request.has_add_javascript_channels()) {
        HandleAddJavascriptChannels(request.add_javascript_channels());
      } else {
        client_->OnError("add_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kRemoveJavascriptChannels:
      if (request.has_remove_javascript_channels()) {
        HandleRemoveJavascriptChannels(request.remove_javascript_channels());
      } else {
        client_->OnError("remove_javascript_channels() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetCurrentUrl:
      HandleGetCurrentUrl(request.id());
      break;

    case webview::WebviewRequest::kCanGoBack:
      HandleCanGoBack(request.id());
      break;

    case webview::WebviewRequest::kCanGoForward:
      HandleCanGoForward(request.id());
      break;

    case webview::WebviewRequest::kGoBack:
      contents_->GetController().GoBack();
      break;

    case webview::WebviewRequest::kGoForward:
      contents_->GetController().GoForward();
      break;

    case webview::WebviewRequest::kReload:
      // TODO(dnicoara): Are the default parameters correct?
      contents_->GetController().Reload(content::ReloadType::NORMAL,
                                        /*check_for_repost=*/true);
      break;

    case webview::WebviewRequest::kClearCache:
      HandleClearCache();
      break;

    case webview::WebviewRequest::kUpdateSettings:
      if (request.has_update_settings()) {
        HandleUpdateSettings(request.update_settings());
      } else {
        client_->OnError("update_settings() not supplied");
      }
      break;

    case webview::WebviewRequest::kGetTitle:
      HandleGetTitle(request.id());
      break;

    case webview::WebviewRequest::kSetAutoMediaPlaybackPolicy:
      if (request.has_set_auto_media_playback_policy()) {
        HandleSetAutoMediaPlaybackPolicy(
            request.set_auto_media_playback_policy());
      } else {
        client_->OnError("set_auto_media_playback_policy() not supplied");
      }
      break;
    case webview::WebviewRequest::kNavigationDecision:
      if (current_navigation_throttle_) {
        current_navigation_throttle_->ProcessNavigationDecision(
            request.navigation_decision());
        current_navigation_throttle_ = nullptr;
      }
      break;
    default:
      client_->OnError("Unknown request code");
      break;
  }
}

void WebviewController::SendNavigationEvent(WebviewNavigationThrottle* throttle,
                                            const GURL& gurl) {
  DCHECK(!current_navigation_throttle_);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->mutable_navigation_event()->set_url(gurl.spec());
  current_navigation_throttle_ = throttle;
  client_->EnqueueSend(std::move(response));
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

void WebviewController::JavascriptCallback(int64_t id, base::Value result) {
  std::string json;
  base::JSONWriter::Write(result, &json);
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();
  response->set_id(id);
  response->mutable_evaluate_javascript()->set_json(json);
  client_->EnqueueSend(std::move(response));
}

void WebviewController::HandleEvaluateJavascript(
    int64_t id,
    const webview::EvaluateJavascriptRequest& request) {
  contents_->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(request.javascript_blob()),
      base::BindOnce(&WebviewController::JavascriptCallback,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void WebviewController::HandleAddJavascriptChannels(
    const webview::AddJavascriptChannelsRequest& request) {
  // TODO(dnicoara): Handle this.
  client_->OnError("Unimplemented add_javascript_channels()");
}

void WebviewController::HandleRemoveJavascriptChannels(
    const webview::RemoveJavascriptChannelsRequest& request) {
  // TODO(dnicoara): Handle this.
  client_->OnError("Unimplemented remove_javascript_channels()");
}

void WebviewController::HandleGetCurrentUrl(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_current_url()->set_url(contents_->GetURL().spec());
  client_->EnqueueSend(std::move(response));
}

void WebviewController::HandleCanGoBack(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_back()->set_can_go_back(
      contents_->GetController().CanGoBack());
  client_->EnqueueSend(std::move(response));
}

void WebviewController::HandleCanGoForward(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_can_go_forward()->set_can_go_forward(
      contents_->GetController().CanGoForward());
  client_->EnqueueSend(std::move(response));
}

void WebviewController::HandleClearCache() {
  // TODO(dnicoara): See if there is a generic way to inform the renderer to
  // clear cache.
  // Android has a specific renderer message for this:
  // https://cs.chromium.org/chromium/src/android_webview/common/render_view_messages.h?rcl=65107121555167a3db39de5633c3297f7e861315&l=44

  // Remove disk cache.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(
          contents_->GetBrowserContext());
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
}

void WebviewController::HandleGetTitle(int64_t id) {
  std::unique_ptr<webview::WebviewResponse> response =
      std::make_unique<webview::WebviewResponse>();

  response->set_id(id);
  response->mutable_get_title()->set_title(
      base::UTF16ToUTF8(contents_->GetTitle()));
  client_->EnqueueSend(std::move(response));
}

void WebviewController::HandleUpdateSettings(
    const webview::UpdateSettingsRequest& request) {
  content::WebPreferences prefs =
      contents_->GetRenderViewHost()->GetWebkitPreferences();
  prefs.javascript_enabled = request.javascript_enabled();
  contents_->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  has_navigation_delegate_ = request.has_navigation_delegate();

  // Given that cast_shell enables devtools unconditionally there isn't
  // anything that needs to be done for |request.debugging_enabled()|. Though,
  // as a note, remote debugging is always on.

  if (request.has_user_agent() &&
      request.user_agent().type_case() == webview::UserAgent::kValue) {
    contents_->SetUserAgentOverride(request.user_agent().value(), true);
  }
}

void WebviewController::HandleSetAutoMediaPlaybackPolicy(
    const webview::SetAutoMediaPlaybackPolicyRequest& request) {
  content::WebPreferences prefs =
      contents_->GetRenderViewHost()->GetWebkitPreferences();
  prefs.autoplay_policy = request.require_user_gesture()
                              ? content::AutoplayPolicy::kUserGestureRequired
                              : content::AutoplayPolicy::kNoUserGestureRequired;
  contents_->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
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
    event->set_url(contents_->GetURL().spec());
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
    event->set_url(contents_->GetURL().spec());
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
    event->set_url(contents_->GetURL().spec());
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

WebviewUserData::WebviewUserData(WebviewController* controller)
    : controller_(controller) {}

WebviewUserData::~WebviewUserData() = default;

std::unique_ptr<base::SupportsUserData::Data> WebviewUserData::Clone() {
  return std::make_unique<WebviewUserData>(controller_);
}

}  // namespace chromecast
