// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/content_constants_internal.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_visual_properties.h"
#include "content/common/input/ime_text_span_conversions.h"
#include "content/common/text_input_state.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(OS_MACOSX)
#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"
#include "content/common/frame_messages.h"
#endif

namespace content {

BrowserPluginGuest::BrowserPluginGuest(bool has_render_view,
                                       WebContentsImpl* web_contents,
                                       BrowserPluginGuestDelegate* delegate)
    : WebContentsObserver(web_contents),
      owner_web_contents_(nullptr),
      attached_(false),
      browser_plugin_instance_id_(browser_plugin::kInstanceIDNone),
      focused_(false),
      is_full_page_plugin_(false),
      has_render_view_(has_render_view),
      is_in_destruction_(false),
      initialized_(false),
      guest_render_view_routing_id_(MSG_ROUTING_NONE),
      last_drag_status_(blink::kWebDragStatusUnknown),
      seen_embedder_system_drag_ended_(false),
      seen_embedder_drag_source_ended_at_(false),
      ignore_dragged_url_(true),
      delegate_(delegate) {
  DCHECK(web_contents);
  DCHECK(delegate);
  RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Create"));
}

int BrowserPluginGuest::LoadURLWithParams(
    const NavigationController::LoadURLParams& load_params) {
  GetWebContents()->GetController().LoadURLWithParams(load_params);
  return MSG_ROUTING_NONE;
}

void BrowserPluginGuest::SizeContents(const gfx::Size& new_size) {
  // TODO(wjmaclean): Verify whether this is used via WebContentsViewChildFrame.
  GetWebContents()->GetView()->SizeContents(new_size);
}

void BrowserPluginGuest::WillDestroy() {
  is_in_destruction_ = true;

  // It is important that the WebContents is notified before detaching.
  GetWebContents()->BrowserPluginGuestWillDetach();

  attached_ = false;
  owner_web_contents_ = nullptr;
}

RenderWidgetHostImpl* BrowserPluginGuest::GetOwnerRenderWidgetHost() const {
  return static_cast<RenderWidgetHostImpl*>(
      delegate_->GetOwnerRenderWidgetHost());
}

RenderFrameHostImpl* BrowserPluginGuest::GetEmbedderFrame() const {
  return static_cast<RenderFrameHostImpl*>(delegate_->GetEmbedderFrame());
}

void BrowserPluginGuest::Init() {
  if (initialized_)
    return;
  initialized_ = true;

  WebContentsImpl* owner_web_contents = static_cast<WebContentsImpl*>(
      delegate_->GetOwnerWebContents());
  owner_web_contents->CreateBrowserPluginEmbedderIfNecessary();
  InitInternal(owner_web_contents);
}

base::WeakPtr<BrowserPluginGuest> BrowserPluginGuest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserPluginGuest::SetFocus(RenderWidgetHost* rwh,
                                  bool focused,
                                  blink::mojom::FocusType focus_type) {
  focused_ = focused;
  if (!rwh)
    return;

  if ((focus_type == blink::mojom::FocusType::kForward) ||
      (focus_type == blink::mojom::FocusType::kBackward)) {
    static_cast<RenderViewHostImpl*>(RenderViewHost::From(rwh))
        ->SetInitialFocus(focus_type == blink::mojom::FocusType::kBackward);
  }
  RenderWidgetHostImpl::From(rwh)->GetWidgetInputHandler()->SetFocus(focused);

  // Restore the last seen state of text input to the view.
  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      rwh->GetView());
  SendTextInputTypeChangedToView(rwhv);
}

WebContentsImpl* BrowserPluginGuest::CreateNewGuestWindow(
    const WebContents::CreateParams& params) {
  WebContentsImpl* new_contents =
      static_cast<WebContentsImpl*>(delegate_->CreateNewGuestWindow(params));
  DCHECK(new_contents);
  return new_contents;
}

void BrowserPluginGuest::InitInternal(WebContentsImpl* owner_web_contents) {
  focused_ = false;
  OnSetFocus(browser_plugin::kInstanceIDNone, focused_,
             blink::mojom::FocusType::kNone);

  is_full_page_plugin_ = false;
  frame_rect_ = gfx::Rect();

  if (owner_web_contents_ != owner_web_contents) {
    // Once a BrowserPluginGuest has an embedder WebContents, it's considered to
    // be attached.
    owner_web_contents_ = owner_web_contents;
  }

  blink::mojom::RendererPreferences* renderer_prefs =
      GetWebContents()->GetMutableRendererPrefs();
  blink::UserAgentOverride guest_user_agent_override =
      renderer_prefs->user_agent_override;
  // Copy renderer preferences (and nothing else) from the embedder's
  // WebContents to the guest.
  //
  // For GTK and Aura this is necessary to get proper renderer configuration
  // values for caret blinking interval, colors related to selection and
  // focus.
  *renderer_prefs = *owner_web_contents_->GetMutableRendererPrefs();
  renderer_prefs->user_agent_override = std::move(guest_user_agent_override);

  // Navigation is disabled in Chrome Apps. We want to make sure guest-initiated
  // navigations still continue to function inside the app.
  renderer_prefs->browser_handles_all_top_level_requests = false;
  // Disable "client blocked" error page for browser plugin.
  renderer_prefs->disable_client_blocked_error_page = true;

  DCHECK(GetWebContents()->GetRenderViewHost());

  // TODO(chrishtr): this code is wrong. The navigate_on_drag_drop field will
  // be reset again the next time preferences are updated.
  WebPreferences prefs =
      GetWebContents()->GetRenderViewHost()->GetWebkitPreferences();
  prefs.navigate_on_drag_drop = false;
  GetWebContents()->GetRenderViewHost()->UpdateWebkitPreferences(prefs);
}

BrowserPluginGuest::~BrowserPluginGuest() {
}

// static
void BrowserPluginGuest::CreateInWebContents(
    WebContentsImpl* web_contents,
    BrowserPluginGuestDelegate* delegate) {
  auto guest = base::WrapUnique(new BrowserPluginGuest(
      web_contents->HasOpener(), web_contents, delegate));
  delegate->SetGuestHost(guest.get());
  web_contents->SetBrowserPluginGuest(std::move(guest));
}

// static
bool BrowserPluginGuest::IsGuest(WebContentsImpl* web_contents) {
  return web_contents && web_contents->GetBrowserPluginGuest();
}

// static
bool BrowserPluginGuest::IsGuest(RenderViewHostImpl* render_view_host) {
  return render_view_host && IsGuest(
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(
          render_view_host)));
}

RenderWidgetHostView* BrowserPluginGuest::GetOwnerRenderWidgetHostView() {
  if (RenderWidgetHostImpl* owner = GetOwnerRenderWidgetHost())
    return owner->GetView();
  return nullptr;
}

BrowserPluginGuestManager*
BrowserPluginGuest::GetBrowserPluginGuestManager() const {
  return GetWebContents()->GetBrowserContext()->GetGuestManager();
}

gfx::Point BrowserPluginGuest::GetCoordinatesInEmbedderWebContents(
    const gfx::Point& relative_point) {
  RenderWidgetHostView* owner_rwhv = GetOwnerRenderWidgetHostView();
  if (!owner_rwhv)
    return relative_point;

  gfx::Point point(relative_point);

  // Add the offset form the embedder web contents view.
  point += owner_rwhv->TransformPointToRootCoordSpace(frame_rect_.origin())
               .OffsetFromOrigin();
  if (embedder_web_contents()->GetBrowserPluginGuest()) {
    // |point| is currently with respect to the top-most view (outermost
    // WebContents). We should subtract a displacement to find the point with
    // resepct to embedder's WebContents.
    point -= owner_rwhv->TransformPointToRootCoordSpace(gfx::Point())
                 .OffsetFromOrigin();
  }

  return point;
}

WebContentsImpl* BrowserPluginGuest::GetWebContents() const {
  return static_cast<WebContentsImpl*>(web_contents());
}

gfx::Point BrowserPluginGuest::GetScreenCoordinates(
    const gfx::Point& relative_position) const {
  if (!attached())
    return relative_position;

  gfx::Point screen_pos(relative_position);
  screen_pos += frame_rect_.OffsetFromOrigin();
  return screen_pos;
}

void BrowserPluginGuest::DragSourceEndedAt(float client_x,
                                           float client_y,
                                           float screen_x,
                                           float screen_y,
                                           blink::WebDragOperation operation) {
  web_contents()->GetRenderViewHost()->GetWidget()->DragSourceEndedAt(
      gfx::PointF(client_x, client_y), gfx::PointF(screen_x, screen_y),
      operation);
  seen_embedder_drag_source_ended_at_ = true;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::EndSystemDragIfApplicable() {
  // Ideally we'd want either WebDragStatusDrop or WebDragStatusLeave...
  // Call guest RVH->DragSourceSystemDragEnded() correctly on the guest where
  // the drag was initiated. Calling DragSourceSystemDragEnded() correctly
  // means we call it in all cases and also make sure we only call it once.
  // This ensures that the input state of the guest stays correct, otherwise
  // it will go stale and won't accept any further input events.
  //
  // The strategy used here to call DragSourceSystemDragEnded() on the RVH
  // is when the following conditions are met:
  //   a. Embedder has seen SystemDragEnded()
  //   b. Embedder has seen DragSourceEndedAt()
  //   c. The guest has seen some drag status update other than
  //      WebDragStatusUnknown. Note that this step should ideally be done
  //      differently: The guest has seen at least one of
  //      {WebDragStatusOver, WebDragStatusDrop}. However, if a user drags
  //      a source quickly outside of <webview> bounds, then the
  //      BrowserPluginGuest never sees any of these drag status updates,
  //      there we just check whether we've seen any drag status update or
  //      not.
  if (last_drag_status_ != blink::kWebDragStatusOver &&
      seen_embedder_drag_source_ended_at_ && seen_embedder_system_drag_ended_) {
    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        GetWebContents()->GetRenderViewHost());
    guest_rvh->GetWidget()->DragSourceSystemDragEnded();
    last_drag_status_ = blink::kWebDragStatusUnknown;
    seen_embedder_system_drag_ended_ = false;
    seen_embedder_drag_source_ended_at_ = false;
    ignore_dragged_url_ = true;
  }
}

void BrowserPluginGuest::EmbedderSystemDragEnded() {
  seen_embedder_system_drag_ended_ = true;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::SendTextInputTypeChangedToView(
    RenderWidgetHostViewBase* guest_rwhv) {
  if (!guest_rwhv)
    return;

  if (!owner_web_contents_) {
    // If we were showing an interstitial, then we can end up here during
    // embedder shutdown or when the embedder navigates to a different page.
    // The call stack is roughly:
    // BrowserPluginGuest::SetFocus()
    // content::InterstitialPageImpl::Hide()
    // content::InterstitialPageImpl::DontProceed().
    //
    // TODO(lazyboy): Write a WebUI test once http://crbug.com/463674 is fixed.
    return;
  }

  if (last_text_input_state_.get()) {
    guest_rwhv->TextInputStateChanged(*last_text_input_state_);
    if (auto* rwh = guest_rwhv->host()) {
      // We need composition range information for some IMEs. To get the
      // updates, we need to explicitly ask the renderer to monitor and send the
      // composition information changes. RenderWidgetHostView of the page will
      // send the request to its process but the machinery for forwarding it to
      // BrowserPlugin is not there. Therefore, we send a direct request to the
      // guest process to start monitoring the state (see
      // https://crbug.com/714771).
      rwh->RequestCompositionUpdates(
          false, last_text_input_state_->type != ui::TEXT_INPUT_TYPE_NONE);
    }
  }
}

void BrowserPluginGuest::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted())
    RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.DidNavigate"));
}

void BrowserPluginGuest::RenderProcessGone(base::TerminationStatus status) {
  switch (status) {
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Killed"));
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.Crashed"));
      break;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.AbnormalDeath"));
      break;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      RecordAction(base::UserMetricsAction("BrowserPlugin.Guest.LaunchFailed"));
      break;
    default:
      break;
  }
}

void BrowserPluginGuest::DidSetHasTouchEventHandlers(bool accept) {
}

void BrowserPluginGuest::DidTextInputStateChange(const TextInputState& params) {
  // Save the state of text input so we can restore it on focus.
  last_text_input_state_ = std::make_unique<TextInputState>(params);

  SendTextInputTypeChangedToView(static_cast<RenderWidgetHostViewBase*>(
      web_contents()->GetRenderWidgetHostView()));
}

void BrowserPluginGuest::DidUnlockMouse() {
}

void BrowserPluginGuest::OnDetach(int browser_plugin_instance_id) {
  if (!attached())
    return;

  // It is important that the WebContents is notified before detaching.
  GetWebContents()->BrowserPluginGuestWillDetach();

  // This tells BrowserPluginGuest to queue up all IPCs to BrowserPlugin until
  // it's attached again.
  attached_ = false;

  RenderWidgetHostViewChildFrame* rwhv =
      static_cast<RenderWidgetHostViewChildFrame*>(
          web_contents()->GetRenderWidgetHostView());
  // If the guest is terminated, our host may already be gone.
  if (rwhv) {
    rwhv->UnregisterFrameSinkId();
  }

  delegate_->DidDetach();
}

void BrowserPluginGuest::OnDragStatusUpdate(int browser_plugin_instance_id,
                                            blink::WebDragStatus drag_status,
                                            const DropData& drop_data,
                                            blink::WebDragOperationsMask mask,
                                            const gfx::PointF& location) {
  RenderViewHost* host = GetWebContents()->GetRenderViewHost();
  auto* embedder = owner_web_contents_->GetBrowserPluginEmbedder();
  DropData filtered_data(drop_data);
  // TODO(paulmeyer): This will need to target the correct specific
  // RenderWidgetHost to work with OOPIFs. See crbug.com/647249.
  RenderWidgetHost* widget = host->GetWidget();
  widget->FilterDropData(&filtered_data);
  switch (drag_status) {
    case blink::kWebDragStatusEnter:
      widget->DragTargetDragEnter(filtered_data, location, location, mask,
                                  drop_data.key_modifiers);
      // Only track the URL being dragged over the guest if the link isn't
      // coming from the guest.
      if (!embedder->DragEnteredGuest(this))
        ignore_dragged_url_ = false;
      break;
    case blink::kWebDragStatusOver:
      widget->DragTargetDragOver(location, location, mask,
                                 drop_data.key_modifiers);
      break;
    case blink::kWebDragStatusLeave:
      embedder->DragLeftGuest(this);
      widget->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
      ignore_dragged_url_ = true;
      break;
    case blink::kWebDragStatusDrop:
      widget->DragTargetDrop(filtered_data, location, location,
                             drop_data.key_modifiers);
      if (!ignore_dragged_url_ && filtered_data.url.is_valid())
        delegate_->DidDropLink(filtered_data.url);
      ignore_dragged_url_ = true;
      break;
    case blink::kWebDragStatusUnknown:
      ignore_dragged_url_ = true;
      NOTREACHED();
  }
  last_drag_status_ = drag_status;
  EndSystemDragIfApplicable();
}

void BrowserPluginGuest::OnExecuteEditCommand(int browser_plugin_instance_id,
                                              const std::string& name) {
  RenderFrameHostImpl* focused_frame =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetFocusedFrame());
  if (!focused_frame || !focused_frame->GetFrameInputHandler())
    return;

  focused_frame->GetFrameInputHandler()->ExecuteEditCommand(name,
                                                            base::nullopt);
}

void BrowserPluginGuest::OnImeCommitText(
    int browser_plugin_instance_id,
    const base::string16& text,
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  std::vector<ui::ImeTextSpan> ui_ime_text_spans =
      ConvertBlinkImeTextSpansToUiImeTextSpans(ime_text_spans);
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->ImeCommitText(text, ui_ime_text_spans, replacement_range,
                      relative_cursor_pos, base::OnceClosure());
}

void BrowserPluginGuest::OnImeFinishComposingText(
    int browser_plugin_instance_id,
    bool keep_selection) {
  DCHECK_EQ(browser_plugin_instance_id_, browser_plugin_instance_id);
  GetWebContents()
      ->GetRenderViewHost()
      ->GetWidget()
      ->GetWidgetInputHandler()
      ->ImeFinishComposingText(keep_selection);
}

void BrowserPluginGuest::OnExtendSelectionAndDelete(
    int browser_plugin_instance_id,
    int before,
    int after) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      web_contents()->GetFocusedFrame());
  if (rfh && rfh->GetFrameInputHandler())
    rfh->GetFrameInputHandler()->ExtendSelectionAndDelete(before, after);
}

void BrowserPluginGuest::OnSetFocus(int browser_plugin_instance_id,
                                    bool focused,
                                    blink::mojom::FocusType focus_type) {
  RenderWidgetHostView* rwhv = web_contents()->GetRenderWidgetHostView();
  RenderWidgetHost* rwh = rwhv ? rwhv->GetRenderWidgetHost() : nullptr;
  SetFocus(rwh, focused, focus_type);
}

void BrowserPluginGuest::OnSynchronizeVisualProperties(
    int browser_plugin_instance_id,
    const FrameVisualProperties& visual_properties) {
  if ((local_surface_id_allocation_.local_surface_id() >
       visual_properties.local_surface_id_allocation.local_surface_id()) ||
      ((frame_rect_.size() != visual_properties.screen_space_rect.size() ||
        screen_info_ != visual_properties.screen_info ||
        capture_sequence_number_ != visual_properties.capture_sequence_number ||
        zoom_level_ != visual_properties.zoom_level) &&
       local_surface_id_allocation_.local_surface_id() ==
           visual_properties.local_surface_id_allocation.local_surface_id())) {
    SiteInstance* owner_site_instance = delegate_->GetOwnerSiteInstance();
    bad_message::ReceivedBadMessage(
        owner_site_instance->GetProcess(),
        bad_message::BPG_RESIZE_PARAMS_CHANGED_LOCAL_SURFACE_ID_UNCHANGED);
    return;
  }

  screen_info_ = visual_properties.screen_info;
  frame_rect_ = visual_properties.screen_space_rect;
  zoom_level_ = visual_properties.zoom_level;

  GetWebContents()->SendScreenRects();
  local_surface_id_allocation_ = visual_properties.local_surface_id_allocation;
  bool capture_sequence_number_changed =
      capture_sequence_number_ != visual_properties.capture_sequence_number;
  capture_sequence_number_ = visual_properties.capture_sequence_number;

  RenderWidgetHostView* view = web_contents()->GetRenderWidgetHostView();
  if (!view)
    return;

  // We could add functionality to set a specific capture sequence number on the
  // |view|, but knowing that it's changed is sufficient for us simply request
  // that our RenderWidgetHostView synchronizes its surfaces. Note that this
  // should only happen during web tests, since that is the only call that
  // should trigger the capture sequence number to change.
  if (capture_sequence_number_changed)
    view->EnsureSurfaceSynchronizedForWebTest();

  RenderWidgetHostImpl* render_widget_host =
      RenderWidgetHostImpl::From(view->GetRenderWidgetHost());
  DCHECK(render_widget_host);

  render_widget_host->SetAutoResize(visual_properties.auto_resize_enabled,
                                    visual_properties.min_size_for_auto_resize,
                                    visual_properties.max_size_for_auto_resize);

  render_widget_host->SynchronizeVisualProperties();
}

#if defined(OS_MACOSX)
bool BrowserPluginGuest::ShowPopupMenu(
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr>* menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
  gfx::Rect translated_bounds(bounds);
  WebContents* guest = web_contents();
  translated_bounds.set_origin(
      guest->GetRenderWidgetHostView()->TransformPointToRootCoordSpace(
          translated_bounds.origin()));
  BrowserPluginPopupMenuHelper popup_menu_helper(
      owner_web_contents_->GetMainFrame(), render_frame_host,
      std::move(*popup_client));
  popup_menu_helper.ShowPopupMenu(translated_bounds, item_height, font_size,
                                  selected_item, std::move(*menu_items),
                                  right_aligned, allow_multiple_selection);
  return true;
}
#endif

}  // namespace content
