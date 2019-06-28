// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/mojom/web_client_hints/web_client_hints_types.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8.h"

class GURL;

namespace blink {
class WebDocumentLoader;
class WebElement;
class WebFormElement;
class WebString;
struct WebURLError;
class WebWorkerFetchContext;
}

namespace network {
struct ResourceResponseHead;
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

class RendererPpapiHost;
class RenderFrame;
class RenderFrameImpl;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderFrameObserver : public IPC::Listener,
                                           public IPC::Sender {
 public:
  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_frame() becase the RenderFrame can
  // go away at any time.
  virtual void OnDestruct() = 0;

  // Called when a Pepper plugin is created.
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // Called when the RenderFrame visiblity is changed.
  virtual void WasHidden() {}
  virtual void WasShown() {}

  // Called when associated widget is about to close.
  virtual void WidgetWillClose() {}

  // Navigation callbacks.
  //
  // Each navigation starts with a DidStartNavigation call. Then it may be
  // followed by a ReadyToCommitNavigation (if the navigation has succeeded),
  // and should always end with a DidFinishNavigation.
  //
  // Unfortunately, this is currently a mess. For example, some started
  // navigations which did not commit won't receive any further notifications.
  // DidCommitProvisionalLoad will be called for same-document navigations,
  // without any other notifications. DidFailProvisionalLoad will be called
  // when committing error pages, in addition to all the methods (start, ready,
  // commit) for the error page load itself.

  // Called when the RenderFrame has started a navigation.
  // |url| is a url being navigated to. Note that final url might be different
  // due to redirects.
  // |navigation_type| is only present for renderer-initiated navigations, e.g.
  // JavaScript call, link click, form submit. User-initiated navigations from
  // the browser process (e.g. by typing a url) won't have a navigation type.
  virtual void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) {}

  // Called when a navigation is about to be committed and |document_loader|
  // will start loading a new document in the RenderFrame.
  virtual void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) {}

  // These match the Blink API notifications
  virtual void DidCreateNewDocument() {}
  virtual void DidCreateDocumentElement() {}
  // TODO(dgozman): replace next two methods with DidFinishNavigation.
  virtual void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                        ui::PageTransition transition) {}
  virtual void DidFailProvisionalLoad(const blink::WebURLError& error) {}
  virtual void DidFinishLoad() {}
  virtual void DidFinishDocumentLoad() {}
  virtual void DidHandleOnloadEvents() {}
  virtual void DidCreateScriptContext(v8::Local<v8::Context> context,
                                      int world_id) {}
  virtual void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                        int world_id) {}
  virtual void DidClearWindowObject() {}
  virtual void DidChangeScrollOffset() {}
  virtual void WillSendSubmitEvent(const blink::WebFormElement& form) {}
  virtual void WillSubmitForm(const blink::WebFormElement& form) {}
  virtual void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors) {}

  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached. Since the frame is
  // already detached from the DOM at this time, it should not be inspected.
  virtual void FrameDetached() {}

  // Called when we receive a console message from Blink for which we requested
  // extra details (like the stack trace). |message| is the error message,
  // |source| is the Blink-reported source of the error (either external or
  // internal), and |stack_trace| is the stack trace of the error in a
  // human-readable format (each frame is formatted as
  // "\n    at function_name (source:line_number:column_number)").
  virtual void DetailedConsoleMessageAdded(const base::string16& message,
                                           const base::string16& source,
                                           const base::string16& stack_trace,
                                           uint32_t line_number,
                                           int32_t severity_level) {}

  // Called when an interesting (from document lifecycle perspective),
  // compositor-driven layout had happened. This is a reasonable hook to use
  // to inspect the document and layout information, since it is in a clean
  // state and you won't accidentally force new layouts.
  // The interestingness of layouts is explained in WebMeaningfulLayout.h.
  virtual void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {}

  // Notifications when |PerformanceTiming| data becomes available
  virtual void DidChangePerformanceTiming() {}

  // Notifications when a cpu timing update becomes available, when a frame
  // has performed at least 100ms of tasks.
  virtual void DidChangeCpuTiming(base::TimeDelta time) {}

  // Notification when the renderer uses a particular code path during a page
  // load. This is used for metrics collection.
  virtual void DidObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) {}

  // Notification when the renderer observes a new use counter usage during a
  // page load. This is used for UseCounter metrics.
  virtual void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) {}
  virtual void DidObserveNewCssPropertyUsage(int css_property,
                                             bool is_animated) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  // This is called once for each janking animation frame, with the jank
  // fraction for that frame.  The cumulative jank score can be inferred by
  // summing the jank fractions. |after_input_or_scroll| indicates whether the
  // given |jank_fraction| was observed after an input or scroll occurred in the
  // associated document.
  virtual void DidObserveLayoutJank(double jank_fraction,
                                    bool after_input_or_scroll) {}

  // Reports lazy loaded behavior when the frame or image is fully deferred or
  // if the frame or image is loaded after being deferred by lazy load.
  // Called every time the behavior occurs. This does not apply to image
  // requests for placeholder images.
  virtual void DidObserveLazyLoadBehavior(
      blink::WebLocalFrameClient::LazyLoadBehavior lazy_load_behavior) {}

  // Notification when the renderer a response started, completed or canceled.
  // Complete or Cancel is guaranteed to be called for a response that started.
  // |request_id| uniquely identifies the request within this render frame.
  // |previews_state| is the PreviewsState if the request is a sub-resource. For
  // Document resources, |previews_state| should be reported as PREVIEWS_OFF.
  virtual void DidStartResponse(
      const GURL& response_url,
      int request_id,
      const network::ResourceResponseHead& response_head,
      content::ResourceType resource_type,
      PreviewsState previews_state) {}
  virtual void DidCompleteResponse(
      int request_id,
      const network::URLLoaderCompletionStatus& status) {}
  virtual void DidCancelResponse(int request_id) {}

  // Notification when the renderer observes data used during the page load.
  // This is used for page load metrics. |received_data_length| is the received
  // network bytes. |resource_id| uniquely identifies the resource within this
  // render frame.
  virtual void DidReceiveTransferSizeUpdate(int resource_id,
                                            int received_data_length) {}

  // Called when the focused element has changed to |element|.
  virtual void FocusedElementChanged(const blink::WebElement& element) {}

  // Called when accessibility is enabled or disabled.
  virtual void AccessibilityModeChanged() {}

  // Called when script in the page calls window.print().
  virtual void ScriptedPrint(bool user_initiated) {}

  // Called when draggable regions change.
  virtual void DraggableRegionsChanged() {}

  // Called when a worker fetch context will be created.
  virtual void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) {}

  // Called to give the embedder an opportunity to bind an interface request
  // for a frame. If the request can be bound, |interface_pipe| will be taken.
  virtual void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // Similar to above but for handling Channel-associated interface requests.
  // Returns |true| if the request is handled by the implementation (taking
  // ownership of |*handle|) and |false| otherwise (leaving |*handle|
  // unmodified).
  virtual bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  RenderFrame* render_frame() const;
  int routing_id() const { return routing_id_; }

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
  ~RenderFrameObserver() override;

 private:
  friend class RenderFrameImpl;

  // This is called by the RenderFrame when it's going away so that this object
  // can null out its pointer.
  void RenderFrameGone();

  RenderFrame* render_frame_;
  // The routing ID of the associated RenderFrame.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
