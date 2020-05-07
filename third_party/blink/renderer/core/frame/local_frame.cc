/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom-blink.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/ad_tagging/ad_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/child_frame_disconnector.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/editing/surrounding_text.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/frame_serializer.h"
#include "third_party/blink/renderer/core/frame/frame_serializer_delegate_impl.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_owner.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/idleness_detector.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/point.h"

#if defined(OS_MACOSX)
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "ui/gfx/range/range.h"
#endif

namespace blink {

namespace {

// Maximum number of burst download requests allowed.
const int kBurstDownloadLimit = 10;

inline float ParentPageZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->PageZoomFactor() : 1;
}

inline float ParentTextZoomFactor(LocalFrame* frame) {
  auto* parent_local_frame = DynamicTo<LocalFrame>(frame->Tree().Parent());
  return parent_local_frame ? parent_local_frame->TextZoomFactor() : 1;
}

#if defined(OS_MACOSX)
uint32_t GetCurrentCursorPositionInFrame(LocalFrame* local_frame) {
  blink::WebRange range =
      WebLocalFrameImpl::FromFrame(local_frame)->SelectionRange();
  return range.IsNull() ? 0U : static_cast<uint32_t>(range.StartOffset());
}
#endif

// Convert a data url to a message pipe handle that corresponds to a remote
// blob, so that it can be passed across processes.
mojo::ScopedMessagePipeHandle DataURLToMessagePipeHandle(
    const String& data_url) {
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendBytes(data_url.Utf8().data(), data_url.length());
  scoped_refptr<BlobDataHandle> blob_data_handle =
      BlobDataHandle::Create(std::move(blob_data), data_url.length());
  mojo::PendingRemote<mojom::blink::Blob> data_url_blob =
      blob_data_handle->CloneBlobRemote();
  return data_url_blob.PassPipe();
}

HitTestResult HitTestResultForRootFramePos(
    LocalFrame* main_frame,
    const PhysicalOffset& pos_in_root_frame) {
  DCHECK(main_frame->IsMainFrame());

  HitTestLocation location(
      main_frame->View()->ConvertFromRootFrame(pos_in_root_frame));
  HitTestResult result = main_frame->GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

RemoteFrame* SourceFrameForOptionalToken(
    const base::Optional<base::UnguessableToken>& source_frame_token) {
  if (!source_frame_token)
    return nullptr;
  return RemoteFrame::FromFrameToken(source_frame_token.value());
}

class WebBundleGenerationDelegate
    : public WebFrameSerializer::MHTMLPartsGenerationDelegate {
  STACK_ALLOCATED();

 public:
  WebBundleGenerationDelegate() = default;
  ~WebBundleGenerationDelegate() = default;

  WebBundleGenerationDelegate(const WebBundleGenerationDelegate&) = delete;
  WebBundleGenerationDelegate& operator=(const WebBundleGenerationDelegate&) =
      delete;

  bool ShouldSkipResource(const WebURL& url) override { return false; }
  bool UseBinaryEncoding() override { return false; }
  bool RemovePopupOverlay() override { return false; }
  bool UsePageProblemDetectors() override { return false; }
};

class ResourceSnapshotForWebBundleImpl
    : public data_decoder::mojom::blink::ResourceSnapshotForWebBundle {
 public:
  explicit ResourceSnapshotForWebBundleImpl(Deque<SerializedResource> resources)
      : resources_(std::move(resources)) {}
  ~ResourceSnapshotForWebBundleImpl() override = default;

  ResourceSnapshotForWebBundleImpl(const ResourceSnapshotForWebBundleImpl&) =
      delete;
  ResourceSnapshotForWebBundleImpl& operator=(
      const ResourceSnapshotForWebBundleImpl&) = delete;

  // data_decoder::mojom::blink::ResourceSnapshotForWebBundle:
  void GetResourceCount(GetResourceCountCallback callback) override {
    std::move(callback).Run(resources_.size());
  }
  void GetResourceInfo(uint64_t index,
                       GetResourceInfoCallback callback) override {
    if (index >= resources_.size()) {
      std::move(callback).Run(nullptr);
      return;
    }
    const auto& resource = resources_.at(SafeCast<WTF::wtf_size_t>(index));
    auto info = data_decoder::mojom::blink::SerializedResourceInfo::New();
    info->url = resource.url;
    info->mime_type = resource.mime_type;
    info->size = resource.data ? resource.data->size() : 0;
    std::move(callback).Run(std::move(info));
  }
  void GetResourceBody(uint64_t index,
                       GetResourceBodyCallback callback) override {
    if (index >= resources_.size()) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    const auto& resource = resources_.at(SafeCast<WTF::wtf_size_t>(index));
    if (!resource.data) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(
        mojo_base::BigBuffer(resource.data->CopyAs<std::vector<uint8_t>>()));
  }

 private:
  const Deque<SerializedResource> resources_;
};

}  // namespace

template class CORE_TEMPLATE_EXPORT Supplement<LocalFrame>;

void LocalFrame::Init() {
  CoreInitializer::GetInstance().InitLocalFrame(*this);

  GetRemoteNavigationAssociatedInterfaces()->GetInterface(
      local_frame_host_remote_.BindNewEndpointAndPassReceiver());
  GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &LocalFrame::BindToReceiver, WrapWeakPersistent(this)));

  loader_.Init();
}

void LocalFrame::SetView(LocalFrameView* view) {
  DCHECK(!view_ || view_ != view);
  DCHECK(!GetDocument() || !GetDocument()->IsActive());
  if (view_)
    view_->WillBeRemovedFromFrame();
  view_ = view;
}

void LocalFrame::CreateView(const IntSize& viewport_size,
                            const Color& background_color) {
  DCHECK(this);
  DCHECK(GetPage());

  bool is_local_root = this->IsLocalRoot();

  if (is_local_root && View())
    View()->SetParentVisible(false);

  SetView(nullptr);

  LocalFrameView* frame_view = nullptr;
  if (is_local_root) {
    frame_view = MakeGarbageCollected<LocalFrameView>(*this, viewport_size);

    // The layout size is set by WebViewImpl to support @viewport
    frame_view->SetLayoutSizeFixedToFrameSize(false);
  } else {
    frame_view = MakeGarbageCollected<LocalFrameView>(*this);
  }

  SetView(frame_view);

  frame_view->UpdateBaseBackgroundColorRecursively(background_color);

  if (is_local_root)
    frame_view->SetParentVisible(true);

  // FIXME: Not clear what the right thing for OOPI is here.
  if (OwnerLayoutObject()) {
    HTMLFrameOwnerElement* owner = DeprecatedLocalOwner();
    DCHECK(owner);
    // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
    // that it's owned by a FrameOwner that knows nothing about it. If we're
    // lying to this frame, don't let it clobber the existing
    // EmbeddedContentView.
    if (owner->ContentFrame() == this)
      owner->SetEmbeddedContentView(frame_view);
  }

  if (Owner()) {
    View()->SetCanHaveScrollbars(Owner()->ScrollbarMode() !=
                                 mojom::blink::ScrollbarMode::kAlwaysOff);
  }
}

LocalFrame::~LocalFrame() {
  // Verify that the LocalFrameView has been cleared as part of detaching
  // the frame owner.
  DCHECK(!view_);
  if (IsAdSubframe())
    InstanceCounters::DecrementCounter(InstanceCounters::kAdSubframeCounter);
}

void LocalFrame::Trace(Visitor* visitor) {
  visitor->Trace(ad_tracker_);
  visitor->Trace(probe_sink_);
  visitor->Trace(performance_monitor_);
  visitor->Trace(idleness_detector_);
  visitor->Trace(inspector_trace_events_);
  visitor->Trace(loader_);
  visitor->Trace(view_);
  visitor->Trace(dom_window_);
  visitor->Trace(page_popup_owner_);
  visitor->Trace(script_controller_);
  visitor->Trace(editor_);
  visitor->Trace(selection_);
  visitor->Trace(event_handler_);
  visitor->Trace(console_);
  visitor->Trace(smooth_scroll_sequencer_);
  visitor->Trace(content_capture_manager_);
  visitor->Trace(system_clipboard_);
  visitor->Trace(raw_system_clipboard_);
  Frame::Trace(visitor);
  Supplementable<LocalFrame>::Trace(visitor);
}

bool LocalFrame::IsLocalRoot() const {
  if (!Tree().Parent())
    return true;

  return Tree().Parent()->IsRemoteFrame();
}

void LocalFrame::Navigate(FrameLoadRequest& request,
                          WebFrameLoadType frame_load_type) {
  if (!navigation_rate_limiter().CanProceed())
    return;
  if (request.ClientRedirectReason() != ClientNavigationReason::kNone) {
    probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(),
                                    base::TimeDelta(),
                                    request.ClientRedirectReason());
    // Non-user navigation before the page has finished firing onload should not
    // create a new back/forward item. The spec only explicitly mentions this in
    // the context of navigating an iframe.
    if (!GetDocument()->LoadEventFinished() &&
        !HasTransientUserActivation(this) &&
        request.ClientRedirectReason() !=
            ClientNavigationReason::kAnchorClick) {
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
    }
  }

  // Navigations in portal contexts do not create back/forward entries.
  // TODO(mcnee): Similarly, we need to restrict orphaned contexts.
  if (GetPage()->InsidePortal() &&
      frame_load_type == WebFrameLoadType::kStandard) {
    frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  const ClientNavigationReason client_redirect_reason =
      request.ClientRedirectReason();
  loader_.StartNavigation(request, frame_load_type);

  if (client_redirect_reason != ClientNavigationReason::kNone)
    probe::FrameClearedScheduledNavigation(this);
}

void LocalFrame::DetachImpl(FrameDetachType type) {
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // BEGIN RE-ENTRANCY SAFE BLOCK
  // Starting here, the code must be safe against re-entrancy. Dispatching
  // events, et cetera can run Javascript, which can reenter Detach().
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  frame_color_overlay_.reset();

  if (IsLocalRoot()) {
    performance_monitor_->Shutdown();
    if (ad_tracker_)
      ad_tracker_->Shutdown();
  }
  idleness_detector_->Shutdown();
  if (inspector_trace_events_)
    probe_sink_->RemoveInspectorTraceEvents(inspector_trace_events_);
  inspector_task_runner_->Dispose();

  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  loader_.StopAllLoaders();
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached
  // frame on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(*GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // both when unloading itself and when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      GetDocument());
  loader_.DispatchUnloadEvent(nullptr, nullptr);
  DetachChildren();

  // All done if detaching the subframes brought about a detach of this frame
  // also.
  if (!Client())
    return;

  // Detach() needs to be called after detachChildren(), because
  // detachChildren() will trigger the unload event handlers of any child
  // frames, and those event handlers might start a new subresource load in this
  // frame which should be stopped by Detach.
  loader_.Detach();
  DomWindow()->FrameDestroyed();

  if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  // It seems to crash because Frame is detached before LocalFrameView.
  // Verify here that any LocalFrameView has been detached by now.
  if (view_ && view_->IsAttached()) {
    CHECK(DeprecatedLocalOwner());
    CHECK(DeprecatedLocalOwner()->OwnedEmbeddedContentView());
    CHECK_EQ(view_, DeprecatedLocalOwner()->OwnedEmbeddedContentView());
  }
  CHECK(!view_ || !view_->IsAttached());

  // This is the earliest that scripting can be disabled:
  // - FrameLoader::Detach() can fire XHR abort events
  // - Document::Shutdown() can dispose plugins which can run script.
  ScriptForbiddenScope forbid_script;
  if (!Client())
    return;

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // END RE-ENTRANCY SAFE BLOCK
  // Past this point, no script should be executed. If this method was
  // re-entered, then check for a non-null Client() above should have already
  // returned.
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  DCHECK(!IsDetached());

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!view_->IsAttached());
  Client()->WillBeDetached();
  // Notify ScriptController that the frame is closing, since its cleanup ends
  // up calling back to LocalFrameClient via WindowProxy.
  GetScriptController().ClearForClose();

  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!view_->IsAttached());
  SetView(nullptr);

  GetEventHandlerRegistry().DidRemoveAllEventHandlers(*DomWindow());

  probe::FrameDetachedFromParent(this);

  supplements_.clear();
  frame_scheduler_.reset();
  receiver_.reset();
  main_frame_receiver_.reset();
  WeakIdentifierMap<LocalFrame>::NotifyObjectDestroyed(this);
}

bool LocalFrame::DetachDocument() {
  return Loader().DetachDocument(nullptr, nullptr);
}

void LocalFrame::CheckCompleted() {
  GetDocument()->CheckCompleted();
}

const SecurityContext* LocalFrame::GetSecurityContext() const {
  return GetDocument() ? &GetDocument()->GetSecurityContext() : nullptr;
}

void LocalFrame::PrintNavigationErrorMessage(const Frame& target_frame,
                                             const char* reason) {
  // URLs aren't available for RemoteFrames, so the error message uses their
  // origin instead.
  auto* target_local_frame = DynamicTo<LocalFrame>(&target_frame);
  String target_frame_description =
      target_local_frame
          ? "with URL '" +
                target_local_frame->GetDocument()->Url().GetString() + "'"
          : "with origin '" +
                target_frame.GetSecurityContext()
                    ->GetSecurityOrigin()
                    ->ToString() +
                "'";
  String message =
      "Unsafe JavaScript attempt to initiate navigation for frame " +
      target_frame_description + " from frame with URL '" +
      GetDocument()->Url().GetString() + "'. " + reason + "\n";

  DomWindow()->PrintErrorMessage(message);
}

void LocalFrame::PrintNavigationWarning(const String& message) {
  console_->AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kWarning, message));
}

bool LocalFrame::ShouldClose() {
  // TODO(dcheng): This should be fixed to dispatch beforeunload events to
  // both local and remote frames.
  return loader_.ShouldClose();
}

void LocalFrame::DetachChildren() {
  DCHECK(loader_.StateMachine()->CreatingInitialEmptyDocument() ||
         GetDocument());

  if (Document* document = this->GetDocument())
    ChildFrameDisconnector(*document).Disconnect();
}

void LocalFrame::DidAttachDocument() {
  Document* document = GetDocument();
  DCHECK(document);
  GetEditor().Clear();
  // Clearing the event handler clears many events, but notably can ensure that
  // for a drag started on an element in a frame that was moved (likely via
  // appendChild()), the drag source will detach and stop firing drag events
  // even after the frame reattaches.
  GetEventHandler().Clear();
  Selection().DidAttachDocument(document);
  if (IsCrossOriginToParentFrame() && !first_url_cross_origin_to_parent_) {
    first_url_cross_origin_to_parent_ = GetDocument()->Url().GetString();
  }
}

base::Optional<String> LocalFrame::FirstUrlCrossOriginToParent() const {
  return first_url_cross_origin_to_parent_;
}

void LocalFrame::Reload(WebFrameLoadType load_type) {
  DCHECK(IsReloadLoadType(load_type));
  if (!loader_.GetDocumentLoader()->GetHistoryItem())
    return;
  FrameLoadRequest request(
      nullptr, loader_.ResourceRequestForReload(
                   load_type, ClientRedirectPolicy::kClientRedirect));
  request.SetClientRedirectReason(ClientNavigationReason::kReload);
  probe::FrameScheduledNavigation(this, request.GetResourceRequest().Url(),
                                  base::TimeDelta(),
                                  ClientNavigationReason::kReload);
  loader_.StartNavigation(request, load_type);
  probe::FrameClearedScheduledNavigation(this);
}

LocalWindowProxy* LocalFrame::WindowProxy(DOMWrapperWorld& world) {
  return To<LocalWindowProxy>(Frame::GetWindowProxy(world));
}

LocalDOMWindow* LocalFrame::DomWindow() const {
  return To<LocalDOMWindow>(dom_window_.Get());
}

void LocalFrame::SetDOMWindow(LocalDOMWindow* dom_window) {
  DCHECK(dom_window);
  if (this->DomWindow())
    this->DomWindow()->Reset();
  GetScriptController().ClearWindowProxy();
  dom_window_ = dom_window;
}

Document* LocalFrame::GetDocument() const {
  return DomWindow() ? DomWindow()->document() : nullptr;
}

void LocalFrame::SetPagePopupOwner(Element& owner) {
  page_popup_owner_ = &owner;
}

LayoutView* LocalFrame::ContentLayoutObject() const {
  return GetDocument() ? GetDocument()->GetLayoutView() : nullptr;
}

void LocalFrame::DidChangeVisibilityState() {
  if (GetDocument())
    GetDocument()->DidChangeVisibilityState();

  Frame::DidChangeVisibilityState();
}

bool LocalFrame::IsCaretBrowsingEnabled() const {
  return GetSettings() ? GetSettings()->GetCaretBrowsingEnabled() : false;
}

void LocalFrame::HookBackForwardCacheEviction() {
  // Register a callback dispatched when JavaScript is executed on the frame.
  // The callback evicts the frame. If a frame is frozen by BackForwardCache,
  // the frame must not be mutated e.g., by JavaScript execution, then the
  // frame must be evicted in such cases.
  DCHECK(RuntimeEnabledFeatures::BackForwardCacheEnabled());
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  for (const auto& world : worlds) {
    ScriptState* script_state = ToScriptState(this, *world);
    ScriptState::Scope scope(script_state);
    script_state->GetContext()->SetAbortScriptExecution(
        [](v8::Isolate* isolate, v8::Local<v8::Context> context) {
          ScriptState* script_state = ScriptState::From(context);
          LocalDOMWindow* window = LocalDOMWindow::From(script_state);
          DCHECK(window);
          LocalFrame* frame = window->GetFrame();
          if (frame)
            frame->EvictFromBackForwardCache();
        });
  }
}

void LocalFrame::RemoveBackForwardCacheEviction() {
  DCHECK(RuntimeEnabledFeatures::BackForwardCacheEnabled());
  Vector<scoped_refptr<DOMWrapperWorld>> worlds;
  DOMWrapperWorld::AllWorldsInCurrentThread(worlds);
  for (const auto& world : worlds) {
    ScriptState* script_state = ToScriptState(this, *world);
    ScriptState::Scope scope(script_state);
    script_state->GetContext()->SetAbortScriptExecution(nullptr);
  }
}

void LocalFrame::SetIsInert(bool inert) {
  is_inert_ = inert;
  PropagateInertToChildFrames();
}

void LocalFrame::PropagateInertToChildFrames() {
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    // is_inert_ means that this Frame is inert because of a modal dialog or
    // inert element in an ancestor Frame. Otherwise, decide whether a child
    // Frame element is inert because of an element in this Frame.
    child->SetIsInert(is_inert_ ||
                      To<HTMLFrameOwnerElement>(child->Owner())->IsInert());
  }
}

void LocalFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ == touch_action)
    return;
  inherited_effective_touch_action_ = touch_action;
  GetDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(
          style_change_reason::kInheritedStyleChangeFromParentFrame));
}

bool LocalFrame::BubbleLogicalScrollFromChildFrame(
    mojom::blink::ScrollDirection direction,
    ScrollGranularity granularity,
    Frame* child) {
  FrameOwner* owner = child->Owner();
  auto* owner_element = DynamicTo<HTMLFrameOwnerElement>(owner);
  DCHECK(owner_element);

  return GetEventHandler().BubblingScroll(direction, granularity,
                                          owner_element);
}

void LocalFrame::DidFocus() {
  GetLocalFrameHostRemote().DidFocusFrame();
}

void LocalFrame::DidChangeThemeColor() {
  if (Tree().Parent())
    return;

  base::Optional<Color> color = GetDocument()->ThemeColor();
  base::Optional<SkColor> sk_color;
  if (color)
    sk_color = color->Rgb();

  GetLocalFrameHostRemote().DidChangeThemeColor(sk_color);
}

LocalFrame& LocalFrame::LocalFrameRoot() const {
  const LocalFrame* cur_frame = this;
  while (cur_frame && IsA<LocalFrame>(cur_frame->Tree().Parent()))
    cur_frame = To<LocalFrame>(cur_frame->Tree().Parent());

  return const_cast<LocalFrame&>(*cur_frame);
}

scoped_refptr<InspectorTaskRunner> LocalFrame::GetInspectorTaskRunner() {
  return inspector_task_runner_;
}

void LocalFrame::StartPrinting(const FloatSize& page_size,
                               const FloatSize& original_page_size,
                               float maximum_shrink_ratio) {
  SetPrinting(true, page_size, original_page_size, maximum_shrink_ratio);
}

void LocalFrame::EndPrinting() {
  SetPrinting(false, FloatSize(), FloatSize(), 0);
}

void LocalFrame::SetPrinting(bool printing,
                             const FloatSize& page_size,
                             const FloatSize& original_page_size,
                             float maximum_shrink_ratio) {
  // In setting printing, we should not validate resources already cached for
  // the document.  See https://bugs.webkit.org/show_bug.cgi?id=43704
  ResourceCacheValidationSuppressor validation_suppressor(
      GetDocument()->Fetcher());

  GetDocument()->SetPrinting(printing ? Document::kPrinting
                                      : Document::kFinishingPrinting);
  View()->AdjustMediaTypeForPrinting(printing);

  if (TextAutosizer* text_autosizer = GetDocument()->GetTextAutosizer())
    text_autosizer->UpdatePageInfo();

  if (ShouldUsePrintingLayout()) {
    View()->ForceLayoutForPagination(page_size, original_page_size,
                                     maximum_shrink_ratio);
  } else {
    if (LayoutView* layout_view = View()->GetLayoutView()) {
      layout_view->SetIntrinsicLogicalWidthsDirty();
      layout_view->SetNeedsLayout(layout_invalidation_reason::kPrintingChanged);
      layout_view->SetShouldDoFullPaintInvalidationForViewAndAllDescendants();
    }
    View()->UpdateLayout();
    View()->AdjustViewSize();
  }

  // Subframes of the one we're printing don't lay out to the page size.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      if (printing)
        child_local_frame->StartPrinting();
      else
        child_local_frame->EndPrinting();
    }
  }

  if (auto* layout_view = View()->GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }

  if (!printing)
    GetDocument()->SetPrinting(Document::kNotPrinting);
}

bool LocalFrame::ShouldUsePrintingLayout() const {
  if (!GetDocument()->Printing())
    return false;

  // Only the top frame being printed should be fitted to page size.
  // Subframes should be constrained by parents only.
  // This function considers the following two kinds of frames as top frames:
  // -- frame with no parent;
  // -- frame's parent is not in printing mode.
  // For the second type, it is a bit complicated when its parent is a remote
  // frame. In such case, we can not check its document or other internal
  // status. However, if the parent is in printing mode, this frame's printing
  // must have started with |use_printing_layout| as false in print context.
  auto* parent = Tree().Parent();
  if (!parent)
    return true;
  auto* local_parent = DynamicTo<LocalFrame>(parent);
  return local_parent ? !local_parent->GetDocument()->Printing()
                      : Client()->UsePrintingLayout();
}

FloatSize LocalFrame::ResizePageRectsKeepingRatio(
    const FloatSize& original_size,
    const FloatSize& expected_size) const {
  auto* layout_object = ContentLayoutObject();
  if (!layout_object)
    return FloatSize();

  bool is_horizontal = layout_object->StyleRef().IsHorizontalWritingMode();
  float width = original_size.Width();
  float height = original_size.Height();
  if (!is_horizontal)
    std::swap(width, height);
  DCHECK_GT(fabs(width), std::numeric_limits<float>::epsilon());
  float ratio = height / width;

  float result_width =
      floorf(is_horizontal ? expected_size.Width() : expected_size.Height());
  float result_height = floorf(result_width * ratio);
  if (!is_horizontal)
    std::swap(result_width, result_height);
  return FloatSize(result_width, result_height);
}

void LocalFrame::SetPageZoomFactor(float factor) {
  SetPageAndTextZoomFactors(factor, text_zoom_factor_);
}

void LocalFrame::SetTextZoomFactor(float factor) {
  SetPageAndTextZoomFactors(page_zoom_factor_, factor);
}

void LocalFrame::SetPageAndTextZoomFactors(float page_zoom_factor,
                                           float text_zoom_factor) {
  if (page_zoom_factor_ == page_zoom_factor &&
      text_zoom_factor_ == text_zoom_factor)
    return;

  Page* page = this->GetPage();
  if (!page)
    return;

  Document* document = this->GetDocument();
  if (!document)
    return;

  // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
  // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG
  // WG clarification.
  if (document->IsSVGDocument()) {
    if (!document->AccessSVGExtensions().ZoomAndPanEnabled())
      return;
  }

  page_zoom_factor_ = page_zoom_factor;
  text_zoom_factor_ = text_zoom_factor;

  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child)) {
      child_local_frame->SetPageAndTextZoomFactors(page_zoom_factor_,
                                                   text_zoom_factor_);
    }
  }

  document->MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  document->GetStyleEngine().MarkViewportStyleDirty();
  document->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kZoom));
  if (View() && View()->DidFirstLayout())
    document->UpdateStyleAndLayout(DocumentUpdateReason::kSizeChange);
}

void LocalFrame::DeviceScaleFactorChanged() {
  GetDocument()->MediaQueryAffectingValueChanged(MediaValueChange::kOther);
  GetDocument()->GetStyleEngine().MarkViewportStyleDirty();
  GetDocument()->GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create(style_change_reason::kZoom));
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->DeviceScaleFactorChanged();
  }
}

void LocalFrame::MediaQueryAffectingValueChangedForLocalSubtree(
    MediaValueChange value) {
  GetDocument()->MediaQueryAffectingValueChanged(value);
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->MediaQueryAffectingValueChangedForLocalSubtree(value);
  }
}

double LocalFrame::DevicePixelRatio() const {
  if (!page_)
    return 0;

  double ratio = page_->DeviceScaleFactorDeprecated();
  ratio *= PageZoomFactor();
  return ratio;
}

String LocalFrame::SelectedText() const {
  return Selection().SelectedText();
}

String LocalFrame::SelectedTextForClipboard() const {
  if (!GetDocument())
    return g_empty_string;
  DCHECK(!GetDocument()->NeedsLayoutTreeUpdate());
  return Selection().SelectedTextForClipboard();
}

PositionWithAffinity LocalFrame::PositionForPoint(
    const PhysicalOffset& frame_point) {
  HitTestLocation location(frame_point);
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(location);
  Node* node = result.InnerPossiblyPseudoNode();
  if (node && !node->IsPseudoElement())
    node = result.InnerNodeOrImageMapImage();
  if (!node)
    return PositionWithAffinity();
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return PositionWithAffinity();
  const PositionWithAffinity position =
      layout_object->PositionForPoint(result.LocalPoint());
  if (position.IsNull())
    return PositionWithAffinity(FirstPositionInOrBeforeNode(*node));
  return position;
}

Document* LocalFrame::DocumentAtPoint(
    const PhysicalOffset& point_in_root_frame) {
  if (!View())
    return nullptr;

  HitTestLocation location(View()->ConvertFromRootFrame(point_in_root_frame));

  if (!ContentLayoutObject())
    return nullptr;
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  return result.InnerNode() ? &result.InnerNode()->GetDocument() : nullptr;
}

void LocalFrame::RemoveSpellingMarkersUnderWords(const Vector<String>& words) {
  GetSpellChecker().RemoveSpellingMarkersUnderWords(words);
}

String LocalFrame::GetLayerTreeAsTextForTesting(unsigned flags) const {
  if (!ContentLayoutObject())
    return String();

  std::unique_ptr<JSONObject> layers;
  if (!(flags & kOutputAsLayerTree)) {
    layers = View()->CompositedLayersAsJSON(static_cast<LayerTreeFlags>(flags));
  } else {
    if (const auto* root_layer =
            ContentLayoutObject()->Compositor()->RootGraphicsLayer()) {
      if (flags & kLayerTreeIncludesAllLayers && IsMainFrame()) {
        while (root_layer->Parent())
          root_layer = root_layer->Parent();
      }
      layers = GraphicsLayerTreeAsJSON(root_layer,
                                       static_cast<LayerTreeFlags>(flags));
    }
  }
  return layers ? layers->ToPrettyJSONString() : String();
}

bool LocalFrame::ShouldThrottleRendering() const {
  return View() && View()->ShouldThrottleRendering();
}

LocalFrame::LocalFrame(LocalFrameClient* client,
                       Page& page,
                       FrameOwner* owner,
                       const base::UnguessableToken& frame_token,
                       WindowAgentFactory* inheriting_agent_factory,
                       InterfaceRegistry* interface_registry,
                       const base::TickClock* clock)
    : Frame(client,
            page,
            owner,
            frame_token,
            MakeGarbageCollected<LocalWindowProxyManager>(*this),
            inheriting_agent_factory),
      frame_scheduler_(page.GetPageScheduler()->CreateFrameScheduler(
          this,
          client->GetFrameBlameContext(),
          IsMainFrame() ? FrameScheduler::FrameType::kMainFrame
                        : FrameScheduler::FrameType::kSubframe)),
      loader_(this),
      script_controller_(MakeGarbageCollected<ScriptController>(
          *this,
          *static_cast<LocalWindowProxyManager*>(GetWindowProxyManager()))),
      editor_(MakeGarbageCollected<Editor>(*this)),
      selection_(MakeGarbageCollected<FrameSelection>(*this)),
      event_handler_(MakeGarbageCollected<EventHandler>(*this)),
      console_(MakeGarbageCollected<FrameConsole>(*this)),
      navigation_disable_count_(0),
      page_zoom_factor_(ParentPageZoomFactor(this)),
      text_zoom_factor_(ParentTextZoomFactor(this)),
      in_view_source_mode_(false),
      inspector_task_runner_(InspectorTaskRunner::Create(
          GetTaskRunner(TaskType::kInternalInspector))),
      interface_registry_(interface_registry
                              ? interface_registry
                              : InterfaceRegistry::GetEmptyInterfaceRegistry()),
      is_save_data_enabled_(GetNetworkStateNotifier().SaveDataEnabled()),
      lifecycle_state_(mojom::FrameLifecycleState::kRunning) {
  if (IsLocalRoot()) {
    probe_sink_ = MakeGarbageCollected<CoreProbeSink>();
    performance_monitor_ = MakeGarbageCollected<PerformanceMonitor>(this);
    inspector_trace_events_ = MakeGarbageCollected<InspectorTraceEvents>();
    probe_sink_->AddInspectorTraceEvents(inspector_trace_events_);
    if (RuntimeEnabledFeatures::AdTaggingEnabled()) {
      ad_tracker_ = MakeGarbageCollected<AdTracker>(this);
    }
  } else {
    // Inertness only needs to be updated if this frame might inherit the
    // inert state from a higher-level frame. If this is an OOPIF local root,
    // it will be updated later.
    UpdateInertIfPossible();
    UpdateInheritedEffectiveTouchActionIfPossible();
    probe_sink_ = LocalFrameRoot().probe_sink_;
    ad_tracker_ = LocalFrameRoot().ad_tracker_;
    performance_monitor_ = LocalFrameRoot().performance_monitor_;
  }
  idleness_detector_ = MakeGarbageCollected<IdlenessDetector>(this, clock);
  inspector_task_runner_->InitIsolate(V8PerIsolateData::MainThreadIsolate());

  if (ad_tracker_) {
    SetIsAdSubframeIfNecessary();
  }
  DCHECK(ad_tracker_ ? RuntimeEnabledFeatures::AdTaggingEnabled()
                     : !RuntimeEnabledFeatures::AdTaggingEnabled());

  Initialize();

  probe::FrameAttachedToParent(this);
#if defined(OS_MACOSX)
  // It should be bound before accessing TextInputHost which is the interface to
  // respond to GetCharacterIndexAtPoint.
  GetBrowserInterfaceBroker().GetInterface(
      text_input_host_.BindNewPipeAndPassReceiver());
#endif
}

FrameScheduler* LocalFrame::GetFrameScheduler() {
  return frame_scheduler_.get();
}

EventHandlerRegistry& LocalFrame::GetEventHandlerRegistry() const {
  return event_handler_->GetEventHandlerRegistry();
}

scoped_refptr<base::SingleThreadTaskRunner> LocalFrame::GetTaskRunner(
    TaskType type) {
  DCHECK(IsMainThread());
  return frame_scheduler_->GetTaskRunner(type);
}

void LocalFrame::ScheduleVisualUpdateUnlessThrottled() {
  if (ShouldThrottleRendering())
    return;
  GetPage()->Animator().ScheduleVisualUpdate(this);
}

static bool CanAccessAncestor(const SecurityOrigin& active_security_origin,
                              const Frame* target_frame) {
  // targetFrame can be 0 when we're trying to navigate a top-level frame
  // that has a 0 opener.
  if (!target_frame)
    return false;

  const bool is_local_active_origin = active_security_origin.IsLocal();
  for (const Frame* ancestor_frame = target_frame; ancestor_frame;
       ancestor_frame = ancestor_frame->Tree().Parent()) {
    const SecurityOrigin* ancestor_security_origin =
        ancestor_frame->GetSecurityContext()->GetSecurityOrigin();
    if (active_security_origin.CanAccess(ancestor_security_origin))
      return true;

    // Allow file URL descendant navigation even when
    // allowFileAccessFromFileURLs is false.
    // FIXME: It's a bit strange to special-case local origins here. Should we
    // be doing something more general instead?
    if (is_local_active_origin && ancestor_security_origin->IsLocal())
      return true;
  }

  return false;
}

bool LocalFrame::CanNavigate(const Frame& target_frame,
                             const KURL& destination_url) {
  if (&target_frame == this)
    return true;

  // Navigating window.opener cross origin, without user activation. See
  // https://crbug.com/813643.
  if (Client()->Opener() == target_frame && !HasTransientUserActivation(this) &&
      !target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          SecurityOrigin::Create(destination_url).get())) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kOpenerNavigationWithoutGesture);
  }

  if (destination_url.ProtocolIsJavaScript() &&
      !GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          target_frame.GetSecurityContext()->GetSecurityOrigin())) {
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation must be same-origin with the target "
        "if navigating to a javascript: url");
    return false;
  }

  if (GetSecurityContext()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kNavigation)) {
    if (!target_frame.Tree().IsDescendantOf(this) &&
        !target_frame.IsMainFrame()) {
      PrintNavigationErrorMessage(
          target_frame,
          "The frame attempting navigation is sandboxed, and is therefore "
          "disallowed from navigating its ancestors.");
      return false;
    }

    // Sandboxed frames can also navigate popups, if the
    // 'allow-sandbox-escape-via-popup' flag is specified, or if
    // 'allow-popups' flag is specified, or if the
    if (target_frame.IsMainFrame() && target_frame != Tree().Top() &&
        GetSecurityContext()->IsSandboxed(
            network::mojom::blink::WebSandboxFlags::
                kPropagatesToAuxiliaryBrowsingContexts) &&
        (GetSecurityContext()->IsSandboxed(
             network::mojom::blink::WebSandboxFlags::kPopups) ||
         target_frame.Client()->Opener() != this)) {
      PrintNavigationErrorMessage(
          target_frame,
          "The frame attempting navigation is sandboxed and is trying "
          "to navigate a popup, but is not the popup's opener and is not "
          "set to propagate sandboxing to popups.");
      return false;
    }

    // Top navigation is forbidden unless opted-in. allow-top-navigation or
    // allow-top-navigation-by-user-activation will also skips origin checks.
    if (target_frame == Tree().Top()) {
      if (GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kTopNavigation) &&
          GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::
                  kTopNavigationByUserActivation)) {
        PrintNavigationErrorMessage(
            target_frame,
            "The frame attempting navigation of the top-level window is "
            "sandboxed, but the flag of 'allow-top-navigation' or "
            "'allow-top-navigation-by-user-activation' is not set.");
        return false;
      }

      if (GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::kTopNavigation) &&
          !GetSecurityContext()->IsSandboxed(
              network::mojom::blink::WebSandboxFlags::
                  kTopNavigationByUserActivation) &&
          !LocalFrame::HasTransientUserActivation(this)) {
        // With only 'allow-top-navigation-by-user-activation' (but not
        // 'allow-top-navigation'), top navigation requires a user gesture.
        GetLocalFrameHostRemote().DidBlockNavigation(
            destination_url, GetDocument()->Url(),
            mojom::NavigationBlockedReason::kRedirectWithNoUserGestureSandbox);
        PrintNavigationErrorMessage(
            target_frame,
            "The frame attempting navigation of the top-level window is "
            "sandboxed with the 'allow-top-navigation-by-user-activation' "
            "flag, but has no user activation (aka gesture). See "
            "https://www.chromestatus.com/feature/5629582019395584.");
        return false;
      }
      return true;
    }
  }

  DCHECK(GetSecurityContext()->GetSecurityOrigin());
  const SecurityOrigin& origin = *GetSecurityContext()->GetSecurityOrigin();

  // This is the normal case. A document can navigate its decendant frames,
  // or, more generally, a document can navigate a frame if the document is
  // in the same origin as any of that frame's ancestors (in the frame
  // hierarchy).
  //
  // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
  // historical information about this security check.
  if (CanAccessAncestor(origin, &target_frame))
    return true;

  // Top-level frames are easier to navigate than other frames because they
  // display their URLs in the address bar (in most browsers). However, there
  // are still some restrictions on navigation to avoid nuisance attacks.
  // Specifically, a document can navigate a top-level frame if that frame
  // opened the document or if the document is the same-origin with any of
  // the top-level frame's opener's ancestors (in the frame hierarchy).
  //
  // In both of these cases, the document performing the navigation is in
  // some way related to the frame being navigate (e.g., by the "opener"
  // and/or "parent" relation). Requiring some sort of relation prevents a
  // document from navigating arbitrary, unrelated top-level frames.
  if (!target_frame.Tree().Parent()) {
    if (target_frame == Client()->Opener())
      return true;
    if (CanAccessAncestor(origin, target_frame.Client()->Opener()))
      return true;
  }

  if (target_frame == Tree().Top()) {
    // A frame navigating its top may blocked if the document initiating
    // the navigation has never received a user gesture and the navigation
    // isn't same-origin with the target.
    if (HasStickyUserActivation() ||
        target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            SecurityOrigin::Create(destination_url).get())) {
      return true;
    }

    String target_domain = network_utils::GetDomainAndRegistry(
        target_frame.GetSecurityContext()->GetSecurityOrigin()->Domain(),
        network_utils::kIncludePrivateRegistries);
    String destination_domain = network_utils::GetDomainAndRegistry(
        destination_url.Host(), network_utils::kIncludePrivateRegistries);
    if (!target_domain.IsEmpty() && !destination_domain.IsEmpty() &&
        target_domain == destination_domain) {
      return true;
    }
    if (auto* settings_client = Client()->GetContentSettingsClient()) {
      if (settings_client->AllowPopupsAndRedirects(false /* default_value*/))
        return true;
    }
    PrintNavigationErrorMessage(
        target_frame,
        "The frame attempting navigation is targeting its top-level window, "
        "but is neither same-origin with its target nor has it received a "
        "user gesture. See "
        "https://www.chromestatus.com/features/5851021045661696.");
    GetLocalFrameHostRemote().DidBlockNavigation(
        destination_url, GetDocument()->Url(),
        mojom::NavigationBlockedReason::kRedirectWithNoUserGesture);

  } else {
    PrintNavigationErrorMessage(target_frame,
                                "The frame attempting navigation is neither "
                                "same-origin with the target, "
                                "nor is it the target's parent or opener.");
  }
  return false;
}

void LocalFrame::SetIsAdSubframeIfNecessary() {
  DCHECK(ad_tracker_);
  if (IsAdSubframe())
    return;

  Frame* parent = Tree().Parent();
  if (!parent)
    return;

  bool parent_is_ad = parent->IsAdSubframe();

  if (parent_is_ad ||
      ad_tracker_->IsAdScriptInStack(AdTracker::StackType::kBottomAndTop)) {
    SetIsAdSubframe(parent_is_ad ? blink::mojom::AdFrameType::kChildAd
                                 : blink::mojom::AdFrameType::kRootAd);
  }
}

ContentCaptureManager* LocalFrame::GetContentCaptureManager() {
  DCHECK(Client());
  if (!IsLocalRoot())
    return nullptr;

  if (auto* content_capture_client = Client()->GetWebContentCaptureClient()) {
    if (!content_capture_manager_) {
      content_capture_manager_ =
          MakeGarbageCollected<ContentCaptureManager>(*this);
    }
  } else if (content_capture_manager_) {
    content_capture_manager_->Shutdown();
    content_capture_manager_ = nullptr;
  }
  return content_capture_manager_;
}

BrowserInterfaceBrokerProxy& LocalFrame::GetBrowserInterfaceBroker() {
  DCHECK(Client());
  return Client()->GetBrowserInterfaceBroker();
}

AssociatedInterfaceProvider*
LocalFrame::GetRemoteNavigationAssociatedInterfaces() {
  DCHECK(Client());
  return Client()->GetRemoteNavigationAssociatedInterfaces();
}

LocalFrameClient* LocalFrame::Client() const {
  return static_cast<LocalFrameClient*>(Frame::Client());
}

FrameWidget* LocalFrame::GetWidgetForLocalRoot() {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(this);
  if (!web_frame)
    return nullptr;
  // This WebFrameWidgetBase upcasts to a FrameWidget which is the interface
  // exposed to Blink core.
  return web_frame->LocalRootFrameWidget();
}

WebContentSettingsClient* LocalFrame::GetContentSettingsClient() {
  return Client() ? Client()->GetContentSettingsClient() : nullptr;
}

PluginData* LocalFrame::GetPluginData() const {
  if (!Loader().AllowPlugins(kNotAboutToInstantiatePlugin))
    return nullptr;
  return GetPage()->GetPluginData(
      Tree().Top().GetSecurityContext()->GetSecurityOrigin());
}

void LocalFrame::SetAdTrackerForTesting(AdTracker* ad_tracker) {
  if (ad_tracker_)
    ad_tracker_->Shutdown();
  ad_tracker_ = ad_tracker;
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame)

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : frame_(&frame) {
  frame_->DisableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler() {
  frame_->EnableNavigation();
}

namespace {

bool IsScopedFrameBlamerEnabled() {
  // Must match the category used in content::FrameBlameContext.
  static const auto* enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("blink");
  return *enabled;
}

}  // namespace

ScopedFrameBlamer::ScopedFrameBlamer(LocalFrame* frame)
    : frame_(IsScopedFrameBlamerEnabled() ? frame : nullptr) {
  if (LIKELY(!frame_))
    return;
  LocalFrameClient* client = frame_->Client();
  if (!client)
    return;
  if (BlameContext* context = client->GetFrameBlameContext())
    context->Enter();
}

void ScopedFrameBlamer::LeaveContext() {
  LocalFrameClient* client = frame_->Client();
  if (!client)
    return;
  if (BlameContext* context = client->GetFrameBlameContext())
    context->Leave();
}

LocalFrame::LazyLoadImageSetting LocalFrame::GetLazyLoadImageSetting() const {
  DCHECK(GetSettings());
  if (!RuntimeEnabledFeatures::LazyImageLoadingEnabled() ||
      !GetSettings()->GetLazyLoadEnabled()) {
    return LocalFrame::LazyLoadImageSetting::kDisabled;
  }
  // Disable explicit and automatic lazyload for backgrounded or prerendered
  // pages.
  if (!GetDocument()->IsPageVisible() || GetDocument()->IsPrefetchOnly()) {
    return LocalFrame::LazyLoadImageSetting::kDisabled;
  }

  if (!RuntimeEnabledFeatures::AutomaticLazyImageLoadingEnabled())
    return LocalFrame::LazyLoadImageSetting::kEnabledExplicit;
  if (RuntimeEnabledFeatures::
          RestrictAutomaticLazyImageLoadingToDataSaverEnabled() &&
      !is_save_data_enabled_) {
    return LocalFrame::LazyLoadImageSetting::kEnabledExplicit;
  }

  // Skip automatic lazyload when reloading a page.
  if (!RuntimeEnabledFeatures::AutoLazyLoadOnReloadsEnabled() &&
      Loader().GetDocumentLoader() &&
      IsReloadLoadType(Loader().GetDocumentLoader()->LoadType())) {
    return LocalFrame::LazyLoadImageSetting::kEnabledExplicit;
  }

  if (Owner() && !Owner()->ShouldLazyLoadChildren())
    return LocalFrame::LazyLoadImageSetting::kEnabledExplicit;
  return LocalFrame::LazyLoadImageSetting::kEnabledAutomatic;
}

bool LocalFrame::ShouldForceDeferScript() const {
  // Check if we should not defer script in subframe.
  if (base::FeatureList::IsEnabled(features::kDisableForceDeferInChildFrames) &&
      !IsLocalRoot()) {
    return false;
  }

  // Check if enabled by runtime feature (for testing/evaluation) or if enabled
  // by PreviewsState (for live intervention).
  return RuntimeEnabledFeatures::ForceDeferScriptInterventionEnabled() ||
         (Loader().GetDocumentLoader() &&
          Loader().GetDocumentLoader()->GetPreviewsState() ==
              WebURLRequest::kDeferAllScriptOn);
}

WebURLLoaderFactory* LocalFrame::GetURLLoaderFactory() {
  if (!url_loader_factory_)
    url_loader_factory_ = Client()->CreateURLLoaderFactory();
  return url_loader_factory_.get();
}

WebPluginContainerImpl* LocalFrame::GetWebPluginContainer(Node* node) const {
  if (auto* plugin_document = DynamicTo<PluginDocument>(GetDocument())) {
    return plugin_document->GetPluginView();
  }
  if (!node) {
    DCHECK(GetDocument());
    node = GetDocument()->FocusedElement();
  }

  if (node) {
    return node->GetWebPluginContainer();
  }
  return nullptr;
}

void LocalFrame::WasHidden() {
  intersection_state_ = ViewportIntersectionState();
  // The initial value of occlusion_state is kUnknown, and if we leave that
  // value intact then IntersectionObserver will abort processing. The frame is
  // hidden, so for the purpose of computing visibility, kPossiblyOccluded will
  // give the desired behavior (i.e., nothing in the iframe will be visible).
  intersection_state_.occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  // An iframe may get a "was hidden" notification before it has been attached
  // to the frame tree; in that case, skip running IntersectionObserver.
  if (!Owner() || IsProvisional() || !GetDocument() ||
      !GetDocument()->IsActive()) {
    return;
  }
  if (LocalFrameView* frame_view = View())
    frame_view->ForceUpdateViewportIntersections();
}

void LocalFrame::WasShown() {
  if (LocalFrameView* frame_view = View())
    frame_view->ScheduleAnimation();
}

bool LocalFrame::ClipsContent() const {
  // A paint preview shouldn't clip to the viewport if it is the main frame or a
  // root remote frame.
  if (GetDocument()->IsPaintingPreview() && IsLocalRoot())
    return false;

  if (IsMainFrame())
    return GetSettings()->GetMainFrameClipsContent();
  // By default clip to viewport.
  return true;
}

void LocalFrame::SetViewportIntersectionFromParent(
    const ViewportIntersectionState& intersection_state) {
  DCHECK(IsLocalRoot());
  // Notify the render frame observers when the main frame intersection changes.
  if (intersection_state_.main_frame_document_intersection !=
      intersection_state.main_frame_document_intersection) {
    Client()->OnMainFrameDocumentIntersectionChanged(
        intersection_state.main_frame_document_intersection);
  }

  bool can_skip_sticky_frame_tracking =
      intersection_state.can_skip_sticky_frame_tracking ||
      !base::FeatureList::IsEnabled(
          features::kForceExtraRenderingToTrackStickyFrame);

  // We only schedule an update if the viewport intersection or occlusion state
  // has changed, or if we cannot skip sticky frame tracking; neither the
  // viewport offset nor the compositing bounds will affect
  // IntersectionObserver.
  bool needs_update =
      !can_skip_sticky_frame_tracking ||
      intersection_state_.viewport_intersection !=
          intersection_state.viewport_intersection ||
      intersection_state_.occlusion_state != intersection_state.occlusion_state;
  intersection_state_ = intersection_state;
  if (needs_update) {
    if (LocalFrameView* frame_view = View()) {
      frame_view->SetIntersectionObservationState(LocalFrameView::kRequired);
      frame_view->ScheduleAnimation();
    }
  }
}

IntSize LocalFrame::GetMainFrameViewportSize() const {
  LocalFrame& local_root = LocalFrameRoot();
  return local_root.IsMainFrame()
             ? local_root.View()
                   ->GetScrollableArea()
                   ->VisibleContentRect()
                   .Size()
             : IntSize(local_root.intersection_state_.main_frame_viewport_size);
}

IntPoint LocalFrame::GetMainFrameScrollOffset() const {
  LocalFrame& local_root = LocalFrameRoot();
  return local_root.IsMainFrame()
             ? FlooredIntPoint(
                   local_root.View()->GetScrollableArea()->GetScrollOffset())
             : IntPoint(
                   local_root.intersection_state_.main_frame_scroll_offset);
}

FrameOcclusionState LocalFrame::GetOcclusionState() const {
  // TODO(dcheng): Get rid of this branch for the main frame.
  if (IsMainFrame())
    return FrameOcclusionState::kGuaranteedNotOccluded;
  if (IsLocalRoot())
    return intersection_state_.occlusion_state;
  return LocalFrameRoot().GetOcclusionState();
}

bool LocalFrame::NeedsOcclusionTracking() const {
  if (Document* document = GetDocument()) {
    if (IntersectionObserverController* controller =
            document->GetIntersectionObserverController()) {
      return controller->NeedsOcclusionTracking();
    }
  }
  return false;
}

void LocalFrame::ForceSynchronousDocumentInstall(
    const AtomicString& mime_type,
    scoped_refptr<SharedBuffer> data) {
  CHECK(loader_.StateMachine()->IsDisplayingInitialEmptyDocument());
  DCHECK(!Client()->IsLocalFrameClientImpl());

  // Any Document requires Shutdown() before detach, even the initial empty
  // document.
  GetDocument()->Shutdown();

  DomWindow()->InstallNewDocument(
      DocumentInit::Create()
          .WithDocumentLoader(loader_.GetDocumentLoader())
          .WithTypeFrom(mime_type));
  loader_.StateMachine()->AdvanceTo(
      FrameLoaderStateMachine::kCommittedFirstRealLoad);

  GetDocument()->OpenForNavigation(kForceSynchronousParsing, mime_type,
                                   AtomicString("UTF-8"));
  for (const auto& segment : *data)
    GetDocument()->Parser()->AppendBytes(segment.data(), segment.size());
  GetDocument()->Parser()->Finish();

  // Upon loading of SVGImages, log PageVisits in UseCounter.
  // Do not track PageVisits for inspector, web page popups, and validation
  // message overlays (the other callers of this method).
  if (GetDocument()->IsSVGDocument())
    loader_.GetDocumentLoader()->GetUseCounterHelper().DidCommitLoad(this);
}

bool LocalFrame::IsProvisional() const {
  // Calling this after the frame is marked as completely detached is a bug, as
  // this state can no longer be accurately calculated.
  CHECK(!IsDetached());

  if (IsMainFrame()) {
    return GetPage()->MainFrame() != this;
  }

  DCHECK(Owner());
  return Owner()->ContentFrame() != this;
}

void LocalFrame::SetIsAdSubframe(blink::mojom::AdFrameType ad_frame_type) {
  DCHECK(!IsMainFrame());

  // Once |ad_frame_type_| has been set to an ad type on this frame, it cannot
  // be changed.
  if (ad_frame_type == blink::mojom::AdFrameType::kNonAd)
    return;
  if (ad_frame_type_ != blink::mojom::AdFrameType::kNonAd)
    return;
  if (auto* document = GetDocument()) {
    // TODO(fdoray): It is possible for the document not to be installed when
    // this method is called. Consider inheriting frame bit in the graph instead
    // of sending an IPC.
    auto* document_resource_coordinator = document->GetResourceCoordinator();
    if (document_resource_coordinator)
      document_resource_coordinator->SetIsAdFrame();
  }
  ad_frame_type_ = ad_frame_type;
  UpdateAdHighlight();
  frame_scheduler_->SetIsAdFrame();
  InstanceCounters::IncrementCounter(InstanceCounters::kAdSubframeCounter);
}

void LocalFrame::UpdateAdHighlight() {
  if (!IsAdRoot()) {
    // Verify that non root ad subframes do not have an overlay.
    DCHECK(IsMainFrame() || !frame_color_overlay_);
    return;
  }
  if (GetPage()->GetSettings().GetHighlightAds())
    SetSubframeColorOverlay(SkColorSetARGB(128, 255, 0, 0));
  else
    SetSubframeColorOverlay(Color::kTransparent);
}

void LocalFrame::PauseSubresourceLoading(
    mojo::PendingReceiver<blink::mojom::blink::PauseSubresourceLoadingHandle>
        receiver) {
  auto handle = GetFrameScheduler()->GetPauseSubresourceLoadingHandle();
  if (!handle)
    return;
  pause_handle_receivers_.Add(std::move(handle), std::move(receiver));
}

void LocalFrame::ResumeSubresourceLoading() {
  pause_handle_receivers_.Clear();
}

void LocalFrame::AnimateSnapFling(base::TimeTicks monotonic_time) {
  GetEventHandler().AnimateSnapFling(monotonic_time);
}

SmoothScrollSequencer& LocalFrame::GetSmoothScrollSequencer() {
  if (!IsLocalRoot())
    return LocalFrameRoot().GetSmoothScrollSequencer();
  if (!smooth_scroll_sequencer_)
    smooth_scroll_sequencer_ = MakeGarbageCollected<SmoothScrollSequencer>();
  return *smooth_scroll_sequencer_;
}

ukm::UkmRecorder* LocalFrame::GetUkmRecorder() {
  Document* document = GetDocument();
  if (!document)
    return nullptr;
  return document->UkmRecorder();
}

int64_t LocalFrame::GetUkmSourceId() {
  Document* document = GetDocument();
  if (!document)
    return ukm::kInvalidSourceId;
  return document->UkmSourceID();
}

void LocalFrame::UpdateTaskTime(base::TimeDelta time) {
  Client()->DidChangeCpuTiming(time);
}

void LocalFrame::UpdateActiveSchedulerTrackedFeatures(uint64_t features_mask) {
  GetLocalFrameHostRemote().DidChangeActiveSchedulerTrackedFeatures(
      features_mask);
}

const base::UnguessableToken& LocalFrame::GetAgentClusterId() const {
  return GetDocument() ? GetDocument()->GetWindowAgent().cluster_id()
                       : base::UnguessableToken::Null();
}

const mojo::Remote<mojom::blink::ReportingServiceProxy>&
LocalFrame::GetReportingService() const {
  if (!reporting_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        reporting_service_.BindNewPipeAndPassReceiver());
  }
  return reporting_service_;
}

// static
void LocalFrame::NotifyUserActivation(LocalFrame* frame,
                                      bool need_browser_verification) {
  if (frame)
    frame->NotifyUserActivation(need_browser_verification);
}

// static
bool LocalFrame::HasTransientUserActivation(LocalFrame* frame) {
  return frame ? frame->Frame::HasTransientUserActivation() : false;
}

// static
bool LocalFrame::ConsumeTransientUserActivation(
    LocalFrame* frame,
    UserActivationUpdateSource update_source) {
  return frame ? frame->ConsumeTransientUserActivation(update_source) : false;
}

void LocalFrame::NotifyUserActivation(bool need_browser_verification) {
  mojom::blink::UserActivationUpdateType update_type =
      need_browser_verification
          ? mojom::blink::UserActivationUpdateType::
                kNotifyActivationPendingBrowserVerification
          : mojom::blink::UserActivationUpdateType::kNotifyActivation;

  GetLocalFrameHostRemote().UpdateUserActivationState(update_type);
  Client()->NotifyUserActivation();
  NotifyUserActivationInLocalTree();
}

bool LocalFrame::ConsumeTransientUserActivation(
    UserActivationUpdateSource update_source) {
  if (update_source == UserActivationUpdateSource::kRenderer) {
    GetLocalFrameHostRemote().UpdateUserActivationState(
        mojom::blink::UserActivationUpdateType::kConsumeTransientActivation);
  }
  return ConsumeTransientUserActivationInLocalTree();
}

namespace {

class FrameColorOverlay final : public FrameOverlay::Delegate {
 public:
  explicit FrameColorOverlay(LocalFrame* frame, SkColor color)
      : color_(color), frame_(frame) {}

 private:
  void PaintFrameOverlay(const FrameOverlay& frame_overlay,
                         GraphicsContext& graphics_context,
                         const IntSize&) const override {
    const auto* view = frame_->View();
    DCHECK(view);
    if (view->Width() == 0 || view->Height() == 0)
      return;
    ScopedPaintChunkProperties properties(
        graphics_context.GetPaintController(),
        view->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
        frame_overlay, DisplayItem::kFrameOverlay);
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            graphics_context, frame_overlay, DisplayItem::kFrameOverlay))
      return;
    DrawingRecorder recorder(graphics_context, frame_overlay,
                             DisplayItem::kFrameOverlay);
    FloatRect rect(0, 0, view->Width(), view->Height());
    graphics_context.FillRect(rect, color_);
  }

  SkColor color_;
  Persistent<LocalFrame> frame_;
};

}  // namespace

void LocalFrame::SetMainFrameColorOverlay(SkColor color) {
  DCHECK(IsMainFrame());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetSubframeColorOverlay(SkColor color) {
  DCHECK(!IsMainFrame());
  SetFrameColorOverlay(color);
}

void LocalFrame::SetFrameColorOverlay(SkColor color) {
  frame_color_overlay_.reset();

  if (color == Color::kTransparent)
    return;

  frame_color_overlay_ = std::make_unique<FrameOverlay>(
      this, std::make_unique<FrameColorOverlay>(this, color));
}

void LocalFrame::UpdateFrameColorOverlayPrePaint() {
  if (frame_color_overlay_)
    frame_color_overlay_->UpdatePrePaint();
}

void LocalFrame::PaintFrameColorOverlay(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (frame_color_overlay_)
    frame_color_overlay_->Paint(context);
}

void LocalFrame::ForciblyPurgeV8Memory() {
  DomWindow()->NotifyContextDestroyed();

  WindowProxyManager* window_proxy_manager = GetWindowProxyManager();
  window_proxy_manager->ClearForV8MemoryPurge();
  Loader().StopAllLoaders();
}

void LocalFrame::DispatchBeforeUnloadEventForFreeze() {
  auto* document_resource_coordinator = GetDocument()->GetResourceCoordinator();
  if (document_resource_coordinator &&
      lifecycle_state_ == mojom::FrameLifecycleState::kRunning &&
      !RuntimeEnabledFeatures::BackForwardCacheEnabled()) {
    // TODO(yuzus): Skip this block if DidFreeze is triggered by bfcache.

    // Determine if there is a beforeunload handler by dispatching a
    // beforeunload that will *not* launch a user dialog. If
    // |proceed| is false then there is a non-empty beforeunload
    // handler indicating potentially unsaved user state.
    bool unused_did_allow_navigation = false;
    bool proceed = GetDocument()->DispatchBeforeUnloadEvent(
        nullptr, false /* is_reload */, unused_did_allow_navigation);

    // DispatchBeforeUnloadEvent dispatches JS events, which may detach |this|.
    if (!IsAttached())
      return;
    document_resource_coordinator->SetHasNonEmptyBeforeUnload(!proceed);
  }
}

void LocalFrame::DidFreeze() {
  DCHECK(IsAttached());
  GetDocument()->DispatchFreezeEvent();
  // DispatchFreezeEvent dispatches JS events, which may detach |this|.
  if (!IsAttached())
    return;
  // TODO(fmeawad): Move the following logic to the page once we have a
  // PageResourceCoordinator in Blink. http://crbug.com/838415
  if (auto* document_resource_coordinator =
          GetDocument()->GetResourceCoordinator()) {
    document_resource_coordinator->SetLifecycleState(
        performance_manager::mojom::LifecycleState::kFrozen);
  }
}

void LocalFrame::DidResume() {
  DCHECK(IsAttached());
  const base::TimeTicks resume_event_start = base::TimeTicks::Now();
  GetDocument()->DispatchEvent(*Event::Create(event_type_names::kResume));
  const base::TimeTicks resume_event_end = base::TimeTicks::Now();
  base::UmaHistogramMicrosecondsTimes("DocumentEventTiming.ResumeDuration",
                                      resume_event_end - resume_event_start);
  // TODO(fmeawad): Move the following logic to the page once we have a
  // PageResourceCoordinator in Blink
  if (auto* document_resource_coordinator =
          GetDocument()->GetResourceCoordinator()) {
    document_resource_coordinator->SetLifecycleState(
        performance_manager::mojom::LifecycleState::kRunning);
  }
}

void LocalFrame::PauseContext() {
  GetDocument()->Fetcher()->SetDefersLoading(true);
  DomWindow()->SetLifecycleState(lifecycle_state_);
  Loader().SetDefersLoading(true);
  GetFrameScheduler()->SetPaused(true);
}

void LocalFrame::UnpauseContext() {
  GetDocument()->Fetcher()->SetDefersLoading(false);
  DomWindow()->SetLifecycleState(mojom::FrameLifecycleState::kRunning);
  Loader().SetDefersLoading(false);
  GetFrameScheduler()->SetPaused(false);
}

void LocalFrame::SetLifecycleState(mojom::FrameLifecycleState state) {
  // Don't allow lifecycle state changes for detached frames.
  if (!IsAttached())
    return;
  // If we have asked to be frozen we will only do this once the
  // load event has fired.
  if ((state == mojom::FrameLifecycleState::kFrozen ||
       state == mojom::FrameLifecycleState::kFrozenAutoResumeMedia) &&
      IsLoading() && !RuntimeEnabledFeatures::BackForwardCacheEnabled()) {
    // TODO(yuzus): We violate the spec and when bfcache is enabled,
    // |pending_lifecycle_state_| is not set.
    // With bfcache, the decision as to whether the frame gets frozen or not is
    // already made on the browser side and should not be overridden here.
    // https://wicg.github.io/page-lifecycle/#update-document-frozenness-steps
    pending_lifecycle_state_ = state;
    return;
  }
  pending_lifecycle_state_ = base::nullopt;

  if (state == lifecycle_state_)
    return;

  bool is_frozen = lifecycle_state_ != mojom::FrameLifecycleState::kRunning;
  bool freeze = state != mojom::FrameLifecycleState::kRunning;

  // TODO(dtapuska): Determine if we should dispatch events if we are
  // transitioning across frozen states. ie. kPaused->kFrozen should
  // pause media.

  // If we are transitioning from one frozen state to another just return.
  if (is_frozen == freeze)
    return;
  mojom::FrameLifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;

  if (freeze) {
    if (lifecycle_state_ != mojom::FrameLifecycleState::kPaused) {
      DidFreeze();
      // DidFreeze can dispatch JS events, which may detach |this|.
      if (!IsAttached())
        return;
    }
    PauseContext();
  } else {
    UnpauseContext();
    if (old_state != mojom::FrameLifecycleState::kPaused) {
      DidResume();
      // DidResume can dispatch JS events, which may detach |this|.
      if (!IsAttached())
        return;
    }
  }
  GetLocalFrameHostRemote().LifecycleStateChanged(state);
}

void LocalFrame::MaybeLogAdClickNavigation() {
  if (HasTransientUserActivation(this) && IsAdSubframe())
    UseCounter::Count(GetDocument(), WebFeature::kAdClickNavigation);
}

void LocalFrame::CountUseIfFeatureWouldBeBlockedByFeaturePolicy(
    mojom::WebFeature blocked_cross_origin,
    mojom::WebFeature blocked_same_origin) {
  // Get the origin of the top-level document
  const SecurityOrigin* topOrigin =
      Tree().Top().GetSecurityContext()->GetSecurityOrigin();

  // Check if this frame is same-origin with the top-level
  if (!GetSecurityContext()->GetSecurityOrigin()->CanAccess(topOrigin)) {
    // This frame is cross-origin with the top-level frame, and so would be
    // blocked without a feature policy.
    UseCounter::Count(GetDocument(), blocked_cross_origin);
    return;
  }

  // Walk up the frame tree looking for any cross-origin embeds. Even if this
  // frame is same-origin with the top-level, if it is embedded by a cross-
  // origin frame (like A->B->A) it would be blocked without a feature policy.
  const Frame* f = this;
  while (!f->IsMainFrame()) {
    if (!f->GetSecurityContext()->GetSecurityOrigin()->CanAccess(topOrigin)) {
      UseCounter::Count(GetDocument(), blocked_same_origin);
      return;
    }
    f = f->Tree().Parent();
  }
}

void LocalFrame::FinishedLoading(FrameLoader::NavigationFinishState state) {
  DomWindow()->FinishedLoading(state);

  if (pending_lifecycle_state_) {
    DCHECK(!IsLoading());
    SetLifecycleState(pending_lifecycle_state_.value());
  }
}

void LocalFrame::UpdateFaviconURL() {
  if (!IsMainFrame())
    return;

  // The URL to the icon may be in the header. As such, only
  // ask the loader for the icon if it's finished loading.
  if (!GetDocument()->LoadEventFinished())
    return;

  int icon_types_mask =
      1 << static_cast<int>(mojom::blink::FaviconIconType::kFavicon) |
      1 << static_cast<int>(mojom::blink::FaviconIconType::kTouchIcon) |
      1 << static_cast<int>(
          mojom::blink::FaviconIconType::kTouchPrecomposedIcon);
  Vector<IconURL> icon_urls = GetDocument()->IconURLs(icon_types_mask);
  if (icon_urls.IsEmpty())
    return;

  Vector<mojom::blink::FaviconURLPtr> urls;
  urls.ReserveCapacity(icon_urls.size());
  for (const auto& icon_url : icon_urls) {
    urls.push_back(mojom::blink::FaviconURL::New(
        icon_url.icon_url_, icon_url.icon_type_, icon_url.sizes_));
  }
  DCHECK_EQ(icon_urls.size(), urls.size());

  GetLocalFrameHostRemote().UpdateFaviconURL(std::move(urls));
}

void LocalFrame::SetIsCapturingMediaCallback(
    IsCapturingMediaCallback callback) {
  is_capturing_media_callback_ = std::move(callback);
}

bool LocalFrame::IsCapturingMedia() const {
  return is_capturing_media_callback_ ? is_capturing_media_callback_.Run()
                                      : false;
}

SystemClipboard* LocalFrame::GetSystemClipboard() {
  if (!system_clipboard_)
    system_clipboard_ = MakeGarbageCollected<SystemClipboard>(this);

  return system_clipboard_.Get();
}

RawSystemClipboard* LocalFrame::GetRawSystemClipboard() {
  if (!raw_system_clipboard_)
    raw_system_clipboard_ = MakeGarbageCollected<RawSystemClipboard>(this);

  return raw_system_clipboard_.Get();
}

void LocalFrame::WasAttachedAsLocalMainFrame() {
  GetInterfaceRegistry()->AddAssociatedInterface(WTF::BindRepeating(
      &LocalFrame::BindToMainFrameReceiver, WrapWeakPersistent(this)));
}

void LocalFrame::EvictFromBackForwardCache() {
  GetLocalFrameHostRemote().EvictFromBackForwardCache();
}

void LocalFrame::AnimateDoubleTapZoom(const gfx::Point& point,
                                      const gfx::Rect& rect) {
  GetPage()->GetChromeClient().AnimateDoubleTapZoom(point, rect);
}

void LocalFrame::SetScaleFactor(float scale_factor) {
  DCHECK(IsMainFrame());

  const PageScaleConstraints& constraints =
      GetPage()->GetPageScaleConstraintsSet().FinalConstraints();
  scale_factor = constraints.ClampToConstraints(scale_factor);
  if (scale_factor == GetPage()->GetVisualViewport().Scale())
    return;
  GetPage()->GetVisualViewport().SetScale(scale_factor);
}

void LocalFrame::ClosePage(
    mojom::blink::LocalMainFrame::ClosePageCallback completion_callback) {
  SECURITY_CHECK(IsMainFrame());

  // There are two ways to close a page:
  //
  // 1/ Via webview()->Close() that currently sets the WebView's delegate_ to
  // NULL, and prevent any JavaScript dialogs in the onunload handler from
  // appearing.
  //
  // 2/ Calling the FrameLoader's CloseURL method directly.
  //
  // TODO(creis): Having a single way to close that can run onunload is also
  // useful for fixing http://b/issue?id=753080.

  SubframeLoadingDisabler disabler(GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // when unloading itself.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      GetDocument());
  Loader().DispatchUnloadEvent(nullptr, nullptr);

  std::move(completion_callback).Run();
}

void LocalFrame::PluginActionAt(const gfx::Point& location,
                                mojom::blink::PluginActionType action) {
  SECURITY_CHECK(IsMainFrame());

  // TODO(bokan): Location is probably in viewport coordinates
  HitTestResult result =
      HitTestResultForRootFramePos(this, PhysicalOffset(IntPoint(location)));
  Node* node = result.InnerNode();
  if (!IsA<HTMLObjectElement>(*node) && !IsA<HTMLEmbedElement>(*node))
    return;

  LayoutObject* object = node->GetLayoutObject();
  if (!object || !object->IsLayoutEmbeddedContent())
    return;

  WebPluginContainerImpl* plugin_view =
      ToLayoutEmbeddedContent(object)->Plugin();
  if (!plugin_view)
    return;

  switch (action) {
    case mojom::blink::PluginActionType::kRotate90Clockwise:
      plugin_view->Plugin()->RotateView(WebPlugin::kRotationType90Clockwise);
      return;
    case mojom::blink::PluginActionType::kRotate90Counterclockwise:
      plugin_view->Plugin()->RotateView(
          WebPlugin::kRotationType90Counterclockwise);
      return;
  }
  NOTREACHED();
}

void LocalFrame::SetInitialFocus(bool reverse) {
  GetDocument()->ClearFocusedElement();
  GetPage()->GetFocusController().SetInitialFocus(
      reverse ? mojom::blink::FocusType::kBackward
              : mojom::blink::FocusType::kForward);
}

void LocalFrame::EnablePreferredSizeChangedMode() {
  GetPage()->GetChromeClient().EnablePreferredSizeChangedMode();
}

void LocalFrame::ZoomToFindInPageRect(const gfx::Rect& rect_in_root_frame) {
  GetPage()->GetChromeClient().ZoomToFindInPageRect(
      WebRect(rect_in_root_frame));
}

#if defined(OS_MACOSX)
void LocalFrame::GetCharacterIndexAtPoint(const gfx::Point& point) {
  HitTestLocation location(View()->ViewportToFrame(IntPoint(point)));
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  uint32_t index =
      Selection().CharacterIndexForPoint(result.RoundedPointInInnerNodeFrame());
  GetTextInputHost().GotCharacterIndexAtPoint(index);
}

void LocalFrame::GetFirstRectForRange(const gfx::Range& range) {
  gfx::Rect rect;
  WebLocalFrameClient* client = WebLocalFrameImpl::FromFrame(this)->Client();
  if (!client)
    return;

  if (!client->GetCaretBoundsFromFocusedPlugin(rect)) {
    blink::WebRect web_rect;
    // When request range is invalid we will try to obtain it from current
    // frame selection. The fallback value will be 0.
    uint32_t start =
        range.IsValid() ? range.start() : GetCurrentCursorPositionInFrame(this);

    WebLocalFrameImpl::FromFrame(this)->FirstRectForCharacterRange(
        start, range.length(), web_rect);
    rect.SetRect(web_rect.x, web_rect.y, web_rect.width, web_rect.height);
  }

  GetTextInputHost().GotFirstRectForRange(rect);
}
#endif

HitTestResult LocalFrame::HitTestResultForVisualViewportPos(
    const IntPoint& pos_in_viewport) {
  IntPoint root_frame_point(
      GetPage()->GetVisualViewport().ViewportToRootFrame(pos_in_viewport));
  HitTestLocation location(View()->ConvertFromRootFrame(root_frame_point));
  HitTestResult result = GetEventHandler().HitTestResultAtLocation(
      location, HitTestRequest::kReadOnly | HitTestRequest::kActive);
  result.SetToShadowHostIfInRestrictedShadowRoot();
  return result;
}

void LocalFrame::DidChangeVisibleToHitTesting() {
  // LayoutEmbeddedContent does not propagate style updates to descendants.
  // Need to update the field manually.
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    child->UpdateVisibleToHitTesting();
  }
}

WebPrescientNetworking* LocalFrame::PrescientNetworking() {
  if (!prescient_networking_) {
    prescient_networking_ = WebLocalFrameImpl::FromFrame(this)
                                ->Client()
                                ->CreatePrescientNetworking();
  }
  return prescient_networking_.get();
}

void LocalFrame::SetPrescientNetworkingForTesting(
    std::unique_ptr<WebPrescientNetworking> prescient_networking) {
  prescient_networking_ = std::move(prescient_networking);
}

mojom::blink::LocalFrameHost& LocalFrame::GetLocalFrameHostRemote() {
  return *local_frame_host_remote_.get();
}

void LocalFrame::SetEmbeddingToken(
    const base::UnguessableToken& embedding_token) {
  DCHECK(Tree().Parent());
  DCHECK(Tree().Parent()->IsRemoteFrame());
  embedding_token_ = embedding_token;
}

const base::Optional<base::UnguessableToken>& LocalFrame::GetEmbeddingToken()
    const {
  return embedding_token_;
}

void LocalFrame::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  blink::SurroundingText surrounding_text(this, max_length);

  // |surrounding_text| might not be correctly initialized, for example if
  // |frame_->SelectionRange().IsNull()|, in other words, if there was no
  // selection.
  if (surrounding_text.IsEmpty()) {
    // Don't use WTF::String's default constructor so that we make sure that we
    // always send a valid empty string over the wire instead of a null pointer.
    std::move(callback).Run(g_empty_string, 0, 0);
    return;
  }

  std::move(callback).Run(surrounding_text.TextContent(),
                          surrounding_text.StartOffsetInTextContent(),
                          surrounding_text.EndOffsetInTextContent());
}

void LocalFrame::SendInterventionReport(const String& id,
                                        const String& message) {
  Intervention::GenerateReport(this, id, message);
}

void LocalFrame::SetFrameOwnerProperties(
    mojom::blink::FrameOwnerPropertiesPtr properties) {
  GetDocument()->WillChangeFrameOwnerProperties(
      properties->margin_width, properties->margin_height,
      properties->scrollbar_mode, properties->is_display_none);

  Frame::ApplyFrameOwnerProperties(std::move(properties));
}

void LocalFrame::NotifyUserActivation() {
  NotifyUserActivation(false);
}

void LocalFrame::AddMessageToConsole(mojom::blink::ConsoleMessageLevel level,
                                     const WTF::String& message,
                                     bool discard_duplicates) {
  GetDocument()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(mojom::ConsoleMessageSource::kOther,
                                           level, message),
      discard_duplicates);
}

void LocalFrame::AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr info) {
  if (GetPage()) {
    GetPage()->GetInspectorIssueStorage().AddInspectorIssue(DomWindow(), std::move(info));
  }
}

void LocalFrame::StopLoading() {
  Loader().StopAllLoaders();

  // The stopLoading handler may run script, which may cause this frame to be
  // detached/deleted. If that happens, return immediately.
  if (!IsAttached())
    return;

  // Notify RenderFrame observers.
  WebLocalFrameClient* client = Client()->GetWebFrame()->Client();
  if (client)
    client->OnStopLoading();
}

void LocalFrame::Collapse(bool collapsed) {
  FrameOwner* owner = Owner();
  To<HTMLFrameOwnerElement>(owner)->SetCollapsed(collapsed);
}

void LocalFrame::EnableViewSourceMode() {
  DCHECK(!Tree().Parent());
  SetInViewSourceMode(true);
}

void LocalFrame::Focus() {
  FocusImpl();
}

void LocalFrame::ClearFocusedElement() {
  Document* document = GetDocument();
  Element* old_focused_element = document->FocusedElement();
  document->ClearFocusedElement();
  if (!old_focused_element)
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  document->UpdateStyleAndLayoutTree();
  if (HasEditableStyle(*old_focused_element) ||
      old_focused_element->IsTextControl())
    Selection().Clear();
}

void LocalFrame::GetResourceSnapshotForWebBundle(
    mojo::PendingReceiver<
        data_decoder::mojom::blink::ResourceSnapshotForWebBundle> receiver) {
  Deque<SerializedResource> resources;

  HeapHashSet<WeakMember<const Element>> shadow_template_elements;
  WebBundleGenerationDelegate web_delegate;
  FrameSerializerDelegateImpl core_delegate(web_delegate,
                                            shadow_template_elements);
  FrameSerializer serializer(resources, core_delegate);
  serializer.SerializeFrame(*this);

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ResourceSnapshotForWebBundleImpl>(std::move(resources)),
      std::move(receiver));
}

void LocalFrame::CopyImageAtViewportPoint(const IntPoint& viewport_point) {
  HitTestResult result = HitTestResultForVisualViewportPos(viewport_point);
  if (!IsA<HTMLCanvasElement>(result.InnerNodeOrImageMapImage()) &&
      result.AbsoluteImageURL().IsEmpty()) {
    // There isn't actually an image at these coordinates.  Might be because
    // the window scrolled while the context menu was open or because the page
    // changed itself between when we thought there was an image here and when
    // we actually tried to retrieve the image.
    //
    // FIXME: implement a cache of the most recent HitTestResult to avoid having
    //        to do two hit tests.
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  GetEditor().CopyImage(result);
}

void LocalFrame::CopyImageAt(const gfx::Point& window_point) {
  blink::WebFloatRect viewport_position(window_point.x(), window_point.y(), 0,
                                        0);
  GetPage()->GetChromeClient().WindowToViewportRect(*this, &viewport_position);
  CopyImageAtViewportPoint(IntPoint(viewport_position.x, viewport_position.y));
}

void LocalFrame::SaveImageAt(const gfx::Point& window_point) {
  blink::WebFloatRect viewport_position(window_point.x(), window_point.y(), 0,
                                        0);
  GetPage()->GetChromeClient().WindowToViewportRect(*this, &viewport_position);

  IntPoint location(viewport_position.x, viewport_position.y);
  Node* node =
      HitTestResultForVisualViewportPos(location).InnerNodeOrImageMapImage();
  if (!node || !(IsA<HTMLCanvasElement>(*node) || IsA<HTMLImageElement>(*node)))
    return;

  String url = To<Element>(*node).ImageSourceURL();
  if (!KURL(NullURL(), url).ProtocolIsData())
    return;

  auto params = mojom::blink::DownloadURLParams::New();
  params->data_url_blob = DataURLToMessagePipeHandle(url);
  GetLocalFrameHostRemote().DownloadURL(std::move(params));
}

void LocalFrame::ReportBlinkFeatureUsage(
    const Vector<mojom::blink::WebFeature>& features) {
  DCHECK(!features.IsEmpty());

  // Assimilate all features used/performed by the browser into UseCounter.
  auto* document = GetDocument();
  DCHECK(document);
  for (const auto& feature : features)
    document->CountUse(feature);
}

void LocalFrame::RenderFallbackContent() {
  // TODO(ekaramad): If the owner renders its own content, then the current
  // ContentFrame() should detach (see https://crbug.com/850223).
  auto* owner = DeprecatedLocalOwner();
  DCHECK(IsA<HTMLObjectElement>(owner));
  owner->RenderFallbackContent(this);
}

void LocalFrame::BeforeUnload(bool is_reload, BeforeUnloadCallback callback) {
  base::TimeTicks before_unload_start_time = base::TimeTicks::Now();

  // This will execute the BeforeUnload event in this frame and all of its
  // local descendant frames, including children of remote frames.  The browser
  // process will send separate IPCs to dispatch beforeunload in any
  // out-of-process child frames.
  bool proceed = Loader().ShouldClose(is_reload);

  DCHECK(!callback.is_null());
  base::TimeTicks before_unload_end_time = base::TimeTicks::Now();
  std::move(callback).Run(proceed, before_unload_start_time,
                          before_unload_end_time);
}

void LocalFrame::MediaPlayerActionAtViewportPoint(
    const IntPoint& viewport_position,
    const blink::mojom::blink::MediaPlayerActionType type,
    bool enable) {
  HitTestResult result = HitTestResultForVisualViewportPos(viewport_position);
  Node* node = result.InnerNode();
  if (!IsA<HTMLVideoElement>(*node) && !IsA<HTMLAudioElement>(*node))
    return;

  auto* media_element = To<HTMLMediaElement>(node);
  switch (type) {
    case blink::mojom::blink::MediaPlayerActionType::kPlay:
      if (enable)
        media_element->Play();
      else
        media_element->pause();
      break;
    case blink::mojom::blink::MediaPlayerActionType::kMute:
      media_element->setMuted(enable);
      break;
    case blink::mojom::blink::MediaPlayerActionType::kLoop:
      media_element->SetLoop(enable);
      break;
    case blink::mojom::blink::MediaPlayerActionType::kControls:
      media_element->SetBooleanAttribute(html_names::kControlsAttr, enable);
      break;
    case blink::mojom::blink::MediaPlayerActionType::kPictureInPicture:
      DCHECK(IsA<HTMLVideoElement>(media_element));
      if (enable) {
        PictureInPictureController::From(node->GetDocument())
            .EnterPictureInPicture(To<HTMLVideoElement>(media_element),
                                   nullptr /* promise */,
                                   nullptr /* options */);
      } else {
        PictureInPictureController::From(node->GetDocument())
            .ExitPictureInPicture(To<HTMLVideoElement>(media_element), nullptr);
      }

      break;
  }
}

void LocalFrame::DownloadURL(
    const ResourceRequest& request,
    network::mojom::blink::RedirectMode cross_origin_redirect_behavior) {
  DCHECK(GetDocument());
  mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token;
  if (request.Url().ProtocolIs("blob")) {
    GetDocument()->GetPublicURLManager().Resolve(
        request.Url(), blob_url_token.InitWithNewPipeAndPassReceiver());
  }

  DownloadURL(request, cross_origin_redirect_behavior,
              blob_url_token.PassPipe());
}

void LocalFrame::DownloadURL(
    const ResourceRequest& request,
    network::mojom::blink::RedirectMode cross_origin_redirect_behavior,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  if (ShouldThrottleDownload())
    return;

  auto params = mojom::blink::DownloadURLParams::New();
  const KURL& url = request.Url();
  // Pass data URL through blob.
  if (url.ProtocolIs("data")) {
    params->url = KURL();
    params->data_url_blob = DataURLToMessagePipeHandle(url.GetString());
  } else {
    params->url = url;
  }

  params->referrer = mojom::blink::Referrer::New();
  params->referrer->url = KURL(request.ReferrerString());
  params->referrer->policy = request.GetReferrerPolicy();
  params->initiator_origin = request.RequestorOrigin();
  if (request.GetSuggestedFilename().has_value())
    params->suggested_name = *request.GetSuggestedFilename();
  params->cross_origin_redirects = cross_origin_redirect_behavior;
  params->blob_url_token = std::move(blob_url_token);

  GetLocalFrameHostRemote().DownloadURL(std::move(params));
}

void LocalFrame::MediaPlayerActionAt(
    const gfx::Point& window_point,
    blink::mojom::blink::MediaPlayerActionPtr action) {
  blink::WebFloatRect viewport_position(window_point.x(), window_point.y(), 0,
                                        0);
  GetPage()->GetChromeClient().WindowToViewportRect(*this, &viewport_position);
  IntPoint location(viewport_position.x, viewport_position.y);

  MediaPlayerActionAtViewportPoint(location, action->type, action->enable);
}

void LocalFrame::AdvanceFocusInFrame(
    mojom::blink::FocusType focus_type,
    const base::Optional<base::UnguessableToken>& source_frame_token) {
  RemoteFrame* source_frame = SourceFrameForOptionalToken(source_frame_token);
  if (!source_frame) {
    SetInitialFocus(focus_type == mojom::blink::FocusType::kBackward);
    return;
  }

  GetPage()->GetFocusController().AdvanceFocusAcrossFrames(focus_type,
                                                           source_frame, this);
}

void LocalFrame::AdvanceFocusInForm(mojom::blink::FocusType focus_type) {
  auto* focused_frame = GetPage()->GetFocusController().FocusedFrame();
  if (focused_frame != this)
    return;

  DCHECK(GetDocument());
  Element* element = GetDocument()->FocusedElement();
  if (!element)
    return;

  Element* next_element =
      GetPage()->GetFocusController().NextFocusableElementInForm(element,
                                                                 focus_type);
  if (!next_element)
    return;

  next_element->scrollIntoViewIfNeeded(true /*centerIfNeeded*/);
  next_element->focus();
}

void LocalFrame::ReportContentSecurityPolicyViolation(
    network::mojom::blink::CSPViolationPtr violation) {
  auto source_location = std::make_unique<SourceLocation>(
      violation->source_location->url, violation->source_location->line,
      violation->source_location->column, nullptr);

  console_->AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError, violation->console_message,
      source_location->Clone()));

  auto directive_type =
      ContentSecurityPolicy::GetDirectiveType(violation->effective_directive);
  LocalFrame* context_frame =
      directive_type == ContentSecurityPolicy::DirectiveType::kFrameAncestors
          ? this
          : nullptr;

  GetDocument()->GetContentSecurityPolicy()->ReportViolation(
      violation->directive, directive_type, violation->console_message,
      violation->blocked_url, violation->report_endpoints,
      violation->use_reporting_api, violation->header, violation->type,
      ContentSecurityPolicy::ViolationType::kURLViolation,
      std::move(source_location), context_frame,
      violation->after_redirect ? RedirectStatus::kFollowedRedirect
                                : RedirectStatus::kNoRedirect,
      nullptr /* Element */);
}

void LocalFrame::DidUpdateFramePolicy(const FramePolicy& frame_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  SECURITY_CHECK(IsA<RemoteFrameOwner>(Owner()));
  To<RemoteFrameOwner>(Owner())->SetFramePolicy(frame_policy);
}

void LocalFrame::OnScreensChange() {
  if (RuntimeEnabledFeatures::ScreenEnumerationEnabled()) {
    DomWindow()->DispatchEvent(
        *Event::Create(event_type_names::kScreenschange));
  }
}

bool LocalFrame::ShouldThrottleDownload() {
  const auto now = base::TimeTicks::Now();
  if (num_burst_download_requests_ == 0) {
    burst_download_start_time_ = now;
  } else if (num_burst_download_requests_ >= kBurstDownloadLimit) {
    static constexpr auto kBurstDownloadLimitResetInterval =
        base::TimeDelta::FromSeconds(1);
    if (now - burst_download_start_time_ > kBurstDownloadLimitResetInterval) {
      num_burst_download_requests_ = 1;
      burst_download_start_time_ = now;
      return false;
    }
    return true;
  }

  num_burst_download_requests_++;
  return false;
}

#if defined(OS_MACOSX)
mojom::blink::TextInputHost& LocalFrame::GetTextInputHost() {
  DCHECK(text_input_host_);
  return *text_input_host_.get();
}
#endif

void LocalFrame::BindToReceiver(
    blink::LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::LocalFrame> receiver) {
  DCHECK(frame);
  frame->receiver_.Bind(
      std::move(receiver),
      frame->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void LocalFrame::BindToMainFrameReceiver(
    blink::LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrame> receiver) {
  DCHECK(frame);
  frame->main_frame_receiver_.Bind(
      std::move(receiver),
      frame->GetTaskRunner(blink::TaskType::kInternalDefault));
}

SpellChecker& LocalFrame::GetSpellChecker() const {
  DCHECK(DomWindow());
  return DomWindow()->GetSpellChecker();
}

InputMethodController& LocalFrame::GetInputMethodController() const {
  DCHECK(DomWindow());
  return DomWindow()->GetInputMethodController();
}

TextSuggestionController& LocalFrame::GetTextSuggestionController() const {
  DCHECK(DomWindow());
  return DomWindow()->GetTextSuggestionController();
}

}  // namespace blink
