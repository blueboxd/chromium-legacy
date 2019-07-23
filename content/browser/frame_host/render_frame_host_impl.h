// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/media/old_render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/old_render_frame_audio_output_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/renderer_host/media/render_frame_audio_output_stream_factory.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/csp_context.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_delete_intention.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/navigation_params.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_response_headers.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "services/viz/public/interfaces/hit_test/input_target_client.mojom.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/blocked_navigation_types.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/platform/web_scroll_types.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "services/device/public/mojom/nfc.mojom.h"
#else
#include "third_party/blink/public/mojom/serial/serial.mojom.h"
#endif

class GURL;
struct AccessibilityHostMsg_EventBundleParams;
struct AccessibilityHostMsg_FindInPageResultParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_TextTrackSettings_Params;
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace blink {
class AssociatedInterfaceProvider;
class AssociatedInterfaceRegistry;
struct FramePolicy;
struct TransferableMessage;
struct WebFullscreenOptions;
struct WebScrollIntoViewParams;

namespace mojom {
class WebUsbService;
}
}  // namespace blink

namespace gfx {
class Range;
}

namespace network {
class ResourceRequestBody;
struct ResourceResponse;
}  // namespace network

namespace content {
class AppCacheNavigationHandle;
class AuthenticatorImpl;
class BundledExchangesFactory;
class FrameTree;
class FrameTreeNode;
class GeolocationServiceImpl;
class KeepAliveHandleFactory;
class MediaInterfaceProxy;
class NavigationEntryImpl;
class NavigationHandleImpl;
class NavigationRequest;
class PermissionServiceContext;
class PrefetchedSignedExchangeCache;
class PresentationServiceImpl;
class PushMessagingManager;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class SensorProviderProxyImpl;
class SerialService;
class TimeoutMonitor;
class WebBluetoothServiceImpl;
struct CommonNavigationParams;
struct ContextMenuParams;
struct FrameOwnerProperties;
struct PendingNavigation;
struct CommitNavigationParams;
struct ResourceTimingInfo;
struct SubresourceLoaderParams;

class MediaStreamDispatcherHost;

// To be called when a RenderFrameHostImpl receives an event.
// Provides the host, the event fired, and which node id the event was for.
typedef base::RepeatingCallback<
    void(RenderFrameHostImpl*, ax::mojom::Event, int)>
    AccessibilityCallbackForTesting;

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      public base::SupportsUserData,
      public mojom::FrameHost,
      public BrowserAccessibilityDelegate,
      public RenderProcessHostObserver,
      public SiteInstanceImpl::Observer,
      public service_manager::mojom::InterfaceProvider,
      public blink::mojom::DocumentInterfaceBroker,
      public CSPContext,
      public ui::AXActionHandler {
 public:
  using AXTreeSnapshotCallback =
      base::OnceCallback<void(const ui::AXTreeUpdate&)>;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  static const int kMaxAccessibilityResets = 5;

  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromAXTreeID(ui::AXTreeID ax_tree_id);
  static RenderFrameHostImpl* FromOverlayRoutingToken(
      const base::UnguessableToken& token);

  // Allows overriding the URLLoaderFactory creation for subresources.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback = base::RepeatingCallback<void(
      network::mojom::URLLoaderFactoryRequest request,
      int process_id,
      network::mojom::URLLoaderFactoryPtrInfo original_factory)>;
  static void SetNetworkFactoryForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  ui::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  bool IsDescendantOf(RenderFrameHost*) override;
  int GetFrameTreeNodeId() override;
  base::UnguessableToken GetDevToolsFrameToken() override;
  const std::string& GetFrameName() override;
  bool IsFrameDisplayNone() override;
  const base::Optional<gfx::Size>& GetFrameSize() override;
  size_t GetFrameDepth() override;
  bool IsCrossProcessSubframe() override;
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         JavaScriptResultCallback callback) override;
  void ExecuteJavaScriptInIsolatedWorld(const base::string16& javascript,
                                        JavaScriptResultCallback callback,
                                        int world_id) override;
  void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      JavaScriptResultCallback callback,
      int world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript,
      int world_id = ISOLATED_WORLD_ID_GLOBAL) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(VisualStateCallback callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  blink::AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  content::PageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameCreated() override;
  bool IsRenderFrameLive() override;
  bool IsCurrent() override;
  size_t GetProxyCount() override;
  bool HasSelection() override;
  void RequestTextSurroundingSelection(
      TextSurroundingSelectionCallback callback,
      int max_length) override;
  void AllowBindings(int binding_flags) override;
  int GetEnabledBindings() override;
  void DisableBeforeUnloadHangMonitorForTesting() override;
  bool IsBeforeUnloadHangMonitorDisabledForTesting() override;
  bool GetSuddenTerminationDisablerState(
      blink::WebSuddenTerminationDisablerType disabler_type) override;
  bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature) override;
  bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature,
                        blink::PolicyValue threshold_value) override;
  void ViewSource() override;
  blink::mojom::PauseSubresourceLoadingHandlePtr PauseSubresourceLoading()
      override;
  void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point&,
      const blink::WebMediaPlayerAction& action) override;
  bool CreateNetworkServiceDefaultFactory(
      network::mojom::URLLoaderFactoryRequest default_factory_request) override;
  void MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
      base::flat_set<url::Origin> request_initiators,
      bool push_to_renderer_now) override;
  bool IsSandboxed(blink::WebSandboxFlags flags) override;
  void FlushNetworkAndNavigationInterfacesForTesting() override;
  void PrepareForInnerWebContentsAttach(
      PrepareForInnerWebContentsAttachCallback callback) override;
  void UpdateSubresourceLoaderFactories() override;
  blink::FrameOwnerElementType GetFrameOwnerElementType() override;
  bool HasTransientUserActivation() override;

  void SendAccessibilityEventsToManager(
      const AXEventNotificationDetails& details);

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // BrowserAccessibilityDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() const override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  float AccessibilityGetDeviceScaleFactor() const override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessibleForWindow()
      override;
  WebContents* AccessibilityWebContents() override;
  bool AccessibilityIsMainFrame() const override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // SiteInstanceImpl::Observer
  void RenderProcessGone(SiteInstanceImpl* site_instance) override;

  // CSPContext
  void ReportContentSecurityPolicyViolation(
      const CSPViolationParams& violation_params) override;
  bool SchemeShouldBypassCSP(const base::StringPiece& scheme) override;
  void SanitizeDataForUseInCspViolation(
      bool is_redirect,
      CSPDirective::Name directive,
      GURL* blocked_url,
      SourceLocation* source_location) const override;

  // ui::AXActionHandler:
  void PerformAction(const ui::AXActionData& data) override;
  bool RequiresPerformActionPointInPixels() const override;

  mojom::FrameInputHandler* GetFrameInputHandler();

  viz::mojom::InputTargetClient* GetInputTargetClient() {
    return input_target_client_;
  }

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(int previous_routing_id,
                         int opener_routing_id,
                         int parent_routing_id,
                         int previous_sibling_routing_id);

  // Deletes the RenderFrame in the renderer process.
  // Postcondition: |is_active()| will return false.
  void DeleteRenderFrame(FrameDeleteIntention intent);

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  This is currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  // Returns true if the frame recently plays an audio.
  bool is_audible() const { return is_audible_; }
  void OnAudibleStateChanged(bool is_audible);

  int routing_id() const { return routing_id_; }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|
  // and |devtools_frame_token|), and is forwarded here. The renderer has
  // already been told to create a RenderFrame with the specified ID values.
  // |interface_provider_request| is the request end of the InterfaceProvider
  // interface that the RenderFrameHost corresponding to the child frame should
  // bind to expose services to the renderer process. The caller takes care of
  // sending down the client end of the pipe to the child RenderFrame to use.
  // |document_interface_broker_content_handle| and
  // |document_interface_broker_blink_handle| are the pipe handles bound by
  // to request ends of DocumentInterfaceProviderInterface in content and blink
  // parts of the child frame. RenderFrameHost should bind these handles to
  // expose services to the renderer process. The caller takes care of sending
  // down the client end of the pipe to the child RenderFrame to use.
  void OnCreateChildFrame(int new_routing_id,
                          service_manager::mojom::InterfaceProviderRequest
                              interface_provider_request,
                          blink::mojom::DocumentInterfaceBrokerRequest
                              document_interface_broker_content_request,
                          blink::mojom::DocumentInterfaceBrokerRequest
                              document_interface_broker_blink_request,
                          blink::WebTreeScopeType scope,
                          const std::string& frame_name,
                          const std::string& frame_unique_name,
                          bool is_created_by_script,
                          const base::UnguessableToken& devtools_frame_token,
                          const blink::FramePolicy& frame_policy,
                          const FrameOwnerProperties& frame_owner_properties,
                          blink::FrameOwnerElementType owner_type);

  // Update this frame's state at the appropriate time when a navigation
  // commits. This is called by NavigatorImpl::DidNavigate as a helper, in the
  // midst of a DidCommitProvisionalLoad call.
  void DidNavigate(const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
                   bool is_same_document_navigation);

  RenderViewHostImpl* render_view_host() { return render_view_host_.get(); }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  // Methods to add/remove/reset/query child FrameTreeNodes of this frame.
  // See class-level comment for FrameTreeNode for how the frame tree is
  // represented.
  size_t child_count() { return children_.size(); }
  FrameTreeNode* child_at(size_t index) const { return children_[index].get(); }
  FrameTreeNode* AddChild(std::unique_ptr<FrameTreeNode> child,
                          int process_id,
                          int frame_routing_id);
  void RemoveChild(FrameTreeNode* child);
  void ResetChildren();

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void SetLastCommittedUrl(const GURL& url);

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }

  // Computes site_for_cookies to be used when navigating this frame to
  // |destination|.
  GURL ComputeSiteForCookiesForNavigation(const GURL& destination) const;

  // Allows overriding the last committed origin in tests.
  void SetLastCommittedOriginForTesting(const url::Origin& origin);

  // Fetch the link-rel canonical URL to be used for sharing to external
  // applications.
  void GetCanonicalUrlForSharing(
      mojom::Frame::GetCanonicalUrlForSharingCallback callback);

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the pending WebUI, or null if none applies.
  WebUIImpl* pending_web_ui() const {
    return should_reuse_web_ui_ ? web_ui_.get() : pending_web_ui_.get();
  }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Returns true if this is a top-level frame, or if this frame's RenderFrame
  // is in a different process from its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!GetLocalRenderWidgetHost(); }

  // Returns the RenderWidgetHostImpl attached to this frame or the nearest
  // ancestor frame, which could potentially be the root. For most input
  // and rendering related purposes, GetView() should be preferred and
  // RenderWidgetHostViewBase methods used. GetRenderWidgetHost() will not
  // return a nullptr, whereas GetView() potentially will (for instance,
  // after a renderer crash).
  //
  // This method crashes if this RenderFrameHostImpl does not own a
  // a RenderWidgetHost and nor does any of its ancestors. That would
  // typically mean that the frame has been detached from the frame tree.
  virtual RenderWidgetHostImpl* GetRenderWidgetHost();

  GlobalFrameRoutingId GetGlobalFrameRoutingId();

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // A NavigationRequest for a pending cross-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // A NavigationRequest for a pending same-document navigation in this frame,
  // if any. This is cleared when the navigation commits.
  NavigationRequest* same_document_navigation_request() {
    return same_document_navigation_request_.get();
  }

  // Returns the NavigationHandleImpl stored in the NavigationRequest returned
  // by GetNavigationRequest(), if any.
  NavigationHandleImpl* GetNavigationHandle();

  // Resets the NavigationRequests stored in this RenderFrameHost.
  void ResetNavigationRequests();

  // Called when a navigation is ready to commit in this
  // RenderFrameHost. Transfers ownership of the NavigationRequest associated
  // with the navigation to this RenderFrameHost.
  void SetNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer should preserve the RenderFrameProxy object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderFrames, but not until WasSwappedOut is called.
  void SwapOut(RenderFrameProxyHost* proxy, bool is_loading);

  // Remove this frame and its children. This happens asynchronously, an IPC
  // round trip with the renderer process is needed to ensure children's unload
  // handlers are run.
  // Postcondition: is_active() is false.
  void DetachFromProxy();

  // Whether an ongoing navigation in this frame is waiting for a BeforeUnload
  // ACK either from this RenderFrame or from one of its subframes.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut();

  // This method returns true from the time this RenderFrameHost is created
  // until it is pending deletion. Pending deletion starts when SwapOut is
  // called on the frame or one of its ancestors.
  // BackForwardCache: Returns false when the frame is in the BackForwardCache.
  bool is_active() {
    return unload_state_ == UnloadState::NotRun && !is_in_back_forward_cache_;
  }

  // Navigates to an interstitial page represented by the provided data URL.
  void NavigateToInterstitialURL(const GURL& data_url);

  // Stop the load in progress.
  void Stop();

  enum class BeforeUnloadType {
    BROWSER_INITIATED_NAVIGATION,
    RENDERER_INITIATED_NAVIGATION,
    TAB_CLOSE,
    // This reason is used before a tab is discarded in order to free up
    // resources. When this is used and the handler returns a non-empty string,
    // the confirmation dialog will not be displayed and the discard will
    // automatically be canceled.
    DISCARD,
    // This reason is used when preparing a FrameTreeNode for attaching an inner
    // delegate. In this case beforeunload is dispatched in the frame and all
    // the nested child frames.
    INNER_DELEGATE_ATTACH,
  };

  // Runs the beforeunload handler for this frame and its subframes. |type|
  // indicates whether this call is for a navigation or tab close. |is_reload|
  // indicates whether the navigation is a reload of the page.  If |type|
  // corresponds to tab close and not a navigation, |is_reload| should be
  // false.
  void DispatchBeforeUnload(BeforeUnloadType type, bool is_reload);

  // Simulate beforeunload ack on behalf of renderer if it's unrenresponsive.
  void SimulateBeforeUnloadAck(bool proceed);

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC.  This can be called on a main frame or subframe.  If
  // |check_subframes_only| is false, it covers handlers for the frame
  // itself and all its descendants.  If |check_subframes_only| is true, it
  // only checks the frame's descendants but not the frame itself.
  bool ShouldDispatchBeforeUnload(bool check_subframes_only);

  // Allow tests to override how long to wait for beforeunload ACKs to arrive
  // before timing out.
  void SetBeforeUnloadTimeoutDelayForTesting(const base::TimeDelta& timeout);

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this frame as focused in the renderer process.  This supports
  // cross-process window.focus() calls.
  void SetFocusedFrame();

  // Continues sequential focus navigation in this frame. |source_proxy|
  // represents the frame that requested a focus change. It must be in the same
  // process as this or |nullptr|.
  void AdvanceFocus(blink::WebFocusType type,
                    RenderFrameProxyHost* source_proxy);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input);

  // Get the accessibility mode from the delegate and Send a message to the
  // renderer process to change the accessibility mode.
  void UpdateAccessibilityMode();

#if defined(OS_ANDROID)
  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  using ExtractSmartClipDataCallback = base::OnceCallback<
      void(const base::string16&, const base::string16&, const gfx::Rect&)>;

  void RequestSmartClipExtract(ExtractSmartClipDataCallback callback,
                               gfx::Rect rect);

  void OnSmartClipDataExtracted(int32_t callback_id,
                                const base::string16& text,
                                const base::string16& html,
                                const gfx::Rect& clip_rect);
#endif  // defined(OS_ANDROID)

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                             ui::AXMode ax_mode);

  // Resets the accessibility serializer in the renderer.
  void AccessibilityReset();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process.
  void SetAccessibilityCallbackForTesting(
      const AccessibilityCallbackForTesting& callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Set the AX tree ID of the embedder RFHI, if this is a browser plugin guest.
  void set_browser_plugin_embedder_ax_tree_id(ui::AXTreeID ax_tree_id) {
    browser_plugin_embedder_ax_tree_id_ = ax_tree_id;
  }

  // Send a message to the render process to change text track style settings.
  void SetTextTrackSettings(const FrameMsg_TextTrackSettings_Params& params);

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // NULL.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#else
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif
#endif

  // Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  // |subresource_loader_params| is used in network service land to pass
  // the parameters to create a custom subresource loader in the renderer
  // process, e.g. by AppCache etc.
  void CommitNavigation(
      NavigationRequest* navigation_request,
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params,
      network::ResourceResponse* response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      bool is_view_source,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      const base::UnguessableToken& devtools_navigation_token,
      std::unique_ptr<BundledExchangesFactory> bundled_exchanges_factory);

  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(NavigationRequest* navigation_request,
                        const CommonNavigationParams& common_params,
                        const CommitNavigationParams& commit_params,
                        bool has_stale_copy_in_cache,
                        int error_code,
                        const base::Optional<std::string>& error_page_content);

  // Seneds a renderer-debug URL to the renderer process for handling.
  void HandleRendererDebugURL(const GURL& url);

  // Sets up the Mojo connection between this instance and its associated render
  // frame if it has not yet been set up.
  void SetUpMojoIfNeeded();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void InvalidateMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Updates the pending WebUI of this RenderFrameHost based on the provided
  // |dest_url|, setting it to either none, a new instance or to reuse the
  // currently active one. Returns true if the pending WebUI was updated.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool UpdatePendingWebUI(const GURL& dest_url, int entry_bindings);

  // Updates the active WebUI with the pending one set by the last call to
  // UpdatePendingWebUI and then clears any pending data. If UpdatePendingWebUI
  // was not called the active WebUI will simply be cleared.
  void CommitPendingWebUI();

  // Destroys the pending WebUI and resets related data.
  void ClearPendingWebUI();

  // Destroys all WebUI instances and resets related data.
  void ClearAllWebUI();

  // Returns the Mojo ImageDownloader service.
  const blink::mojom::ImageDownloaderPtr& GetMojoImageDownloader();

  // Returns remote to renderer side FindInPage associated with this frame.
  const mojo::AssociatedRemote<blink::mojom::FindInPage>& GetFindInPage();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the feature policy which should be enforced on this RenderFrame.
  blink::FeaturePolicy* feature_policy() { return feature_policy_.get(); }

  // Tells the renderer that this RenderFrame will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the ScopedPageLoadDeferrers of any current dialogs are
  // no longer on the stack when we attempt to swap it out.
  void SuppressFurtherDialogs();

  void ClearFocusedElement();

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // Returns the PreviewsState of the last successful navigation
  // that made a network request. The PreviewsState is a bitmask of potentially
  // several Previews optimizations.
  PreviewsState last_navigation_previews_state() const {
    return last_navigation_previews_state_;
  }

  bool has_focused_editable_element() const {
    return has_focused_editable_element_;
  }

  // Note: The methods for blocking / resuming / cancelling requests per
  // RenderFrameHost are deprecated and will not work in the network service,
  // please avoid using them.
  //
  // Causes all new requests for the root RenderFrameHost and its children to
  // be blocked (not being started) until ResumeBlockedRequestsForFrame is
  // called.
  void BlockRequestsForFrame();

  // Resumes any blocked request for the specified root RenderFrameHost and
  // child frame hosts.
  void ResumeBlockedRequestsForFrame();

  // Cancels any blocked request for the frame and its subframes.
  void CancelBlockedRequestsForFrame();

  // Binds a DevToolsAgent interface for debugging.
  void BindDevToolsAgent(blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
                         blink::mojom::DevToolsAgentAssociatedRequest request);

#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost();
  service_manager::InterfaceProvider* GetJavaInterfaces() override;
#endif

  // Propagates the visibility state along the immediate local roots by calling
  // RenderWidgetHostViewChildFrame::Show()/Hide(). Calling this on a pending
  // or speculative RenderFrameHost (that has not committed) should be avoided.
  void SetVisibilityForChildViews(bool visible);

  // Returns an unguessable token for this RFHI.  This provides a temporary way
  // to identify a RenderFrameHost that's compatible with IPC.  Else, one needs
  // to send pid + RoutingID, but one cannot send pid.  One can get it from the
  // channel, but this makes it much harder to get wrong.
  // Once media switches to mojo, we should be able to remove this in favor of
  // sending a mojo overlay factory.
  const base::UnguessableToken& GetOverlayRoutingToken();

  // Binds the request end of the InterfaceProvider interface through which
  // services provided by this RenderFrameHost are exposed to the corresponding
  // RenderFrame. The caller is responsible for plumbing the client end to the
  // the renderer process.
  void BindInterfaceProviderRequest(
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request);

  // Binds content and blink request ends of the DocumentInterfaceProvider
  // interface through which services provided by this RenderFrameHost are
  // exposed to the corresponding RenderFrame. The caller is responsible for
  // plumbing the client ends to the the renderer process.
  void BindDocumentInterfaceBrokerRequest(
      blink::mojom::DocumentInterfaceBrokerRequest content_request,
      blink::mojom::DocumentInterfaceBrokerRequest blink_request);

  // Exposed so that tests can swap out the implementation and intercept calls.
  mojo::AssociatedBinding<mojom::FrameHost>& frame_host_binding_for_testing() {
    return frame_host_associated_binding_;
  }

  mojo::Binding<service_manager::mojom::InterfaceProvider>&
  document_scoped_interface_provider_binding_for_testing() {
    return document_scoped_interface_provider_binding_;
  }
  void SetKeepAliveTimeoutForTesting(base::TimeDelta timeout);

  blink::WebSandboxFlags active_sandbox_flags() {
    return active_sandbox_flags_;
  }

  bool is_mhtml_document() { return is_mhtml_document_; }

  // Notifies the render frame about a user activation from the browser side.
  void NotifyUserActivation();

  // Notifies the render frame that |frame_tree_node_| has had the sticky
  // user activation bit set for the first time.
  void DidReceiveFirstUserActivation();

  // Returns the current size for this frame.
  const base::Optional<gfx::Size>& frame_size() const { return frame_size_; }

  // Allow tests to override the timeout used to keep subframe processes alive
  // for unload handler processing.
  void SetSubframeUnloadTimeoutForTesting(const base::TimeDelta& timeout);

  // Returns the list of NavigationEntry ids corresponding to NavigationRequests
  // waiting to commit in this RenderFrameHost.
  std::set<int> GetNavigationEntryIdsPendingCommit();

  service_manager::BinderRegistry& BinderRegistryForTesting() {
    return *registry_;
  }
  blink::mojom::FileChooserPtr BindFileChooserForTesting();

  // Called when the WebAudio AudioContext given by |audio_context_id| has
  // started (or stopped) playing audible audio.
  void AudioContextPlaybackStarted(int audio_context_id);
  void AudioContextPlaybackStopped(int audio_context_id);

  // BackForwardCache:
  //
  // When a RenderFrameHostImpl enters the BackForwardCache, the document enters
  // in a "Frozen" state where no Javascript can run.
  void EnterBackForwardCache();  // The document enters the BackForwardCache.
  void LeaveBackForwardCache();  // The document leaves the BackForwardCache.
  bool is_in_back_forward_cache() { return is_in_back_forward_cache_; }

  // Request a new NavigationClient interface from the renderer and returns the
  // ownership of the AssociatedPtr. This is intended for use by the
  // NavigationRequest. Only used with PerNavigationMojoInterface enabled.
  mojom::NavigationClientAssociatedPtr
  GetNavigationClientFromInterfaceProvider();

  // Called to signify the RenderFrameHostImpl that one of its ongoing
  // NavigationRequest's has been cancelled.
  void NavigationRequestCancelled(NavigationRequest* navigation_request);

  // Called on the main frame of a page embedded in a Portal when it is
  // activated. The frame has the option to adopt the previous page as a portal
  // identified by |portal_token| with the interface |portal|. The activation
  // can optionally include a message |data| dispatched with the
  // PortalActivateEvent.
  void OnPortalActivated(
      const base::UnguessableToken& portal_token,
      mojo::PendingAssociatedRemote<blink::mojom::Portal> portal,
      mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> portal_client,
      blink::TransferableMessage data,
      base::OnceCallback<void(bool)> callback);

  // Called on the main frame of a page embedded in a Portal to forward a
  // message from the host of a portal.
  void ForwardMessageFromHost(blink::TransferableMessage message,
                              const url::Origin& source_origin,
                              const base::Optional<url::Origin>& target_origin);

  // mojom::FrameHost:
  void VisibilityChanged(blink::mojom::FrameVisibility) override;

  blink::mojom::FrameVisibility visibility() const { return visibility_; }

  // A CommitCallbackInterceptor is used to modify parameters for or cancel a
  // DidCommitNavigation call in tests.
  // WillProcessDidCommitNavigation will be run right after entering a
  // navigation callback and if returning false, will return straight away.
  class CommitCallbackInterceptor {
   public:
    CommitCallbackInterceptor() {}
    virtual ~CommitCallbackInterceptor() {}

    virtual bool WillProcessDidCommitNavigation(
        NavigationRequest* navigation_request,
        ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
        mojom::DidCommitProvisionalLoadInterfaceParamsPtr*
            interface_params) = 0;
  };

  // Sets the specified |interceptor|.
  void SetCommitCallbackInterceptorForTesting(
      CommitCallbackInterceptor* interceptor);

  // Posts a message from a frame in another process to the current renderer.
  void PostMessageEvent(int32_t source_routing_id,
                        const base::string16& source_origin,
                        const base::string16& target_origin,
                        blink::TransferableMessage message);

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderFrameHost() const;

  // Scheduler-relevant features this frame is using, for use in metrics.
  // See comments at |scheduler_tracked_features_|.
  uint64_t scheduler_tracked_features() const {
    return renderer_reported_scheduler_tracked_features_ |
           browser_reported_scheduler_tracked_features_;
  }

  // Returns a PrefetchedSignedExchangeCache which is attached to |this| iff
  // SignedExchangeSubresourcePrefetch feature or
  // SignedExchangePrefetchCacheForNavigations feature is enabled.
  scoped_refptr<PrefetchedSignedExchangeCache>
  EnsurePrefetchedSignedExchangeCache();

  // Adds |message| to the DevTools console only if it is unique (i.e. has not
  // been added to the console previously from this frame).
  virtual void AddUniqueMessageToConsole(
      blink::mojom::ConsoleMessageLevel level,
      const std::string& message);

  // Add cookie SameSite deprecation messages to the DevTools console.
  // TODO(crbug.com/977040): Remove when no longer needed.
  void AddSameSiteCookieDeprecationMessage(
      const std::string& cookie_url,
      net::CanonicalCookie::CookieInclusionStatus status);

  // Notify the scheduler that this frame used a feature which impacts the
  // scheduling policy (e.g. whether the frame can be frozen or put into the
  // back-forward cache).
  void OnSchedulerTrackedFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature feature);

  // Returns true if frame is frozen.
  bool IsFrozen();

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      scoped_refptr<RenderViewHostImpl> render_view_host,
                      RenderFrameHostDelegate* delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      int32_t widget_routing_id,
                      bool hidden,
                      bool renderer_initiated_creation);

  // The SendCommit* functions below are wrappers for commit calls
  // made to mojom::FrameNavigationControl and mojom::NavigationClient.
  // These exist to be overridden in tests to retain mojo callbacks.
  // Note: |navigation_id| is used in test overrides, but is unused otherwise.
  virtual void SendCommitNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      const content::CommonNavigationParams& common_params,
      const content::CommitNavigationParams& commit_params,
      const network::ResourceResponseHead& response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      blink::mojom::ControllerServiceWorkerInfoPtr
          controller_service_worker_info,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          prefetch_loader_factory,
      const base::UnguessableToken& devtools_navigation_token);
  virtual void SendCommitFailedNavigation(
      mojom::NavigationClient* navigation_client,
      NavigationRequest* navigation_request,
      const content::CommonNavigationParams& common_params,
      const content::CommitNavigationParams& commit_params,
      bool has_stale_copy_in_cache,
      int32_t error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories);

  // The Build*Callback functions below are responsible for building the
  // callbacks for possible Interface/Commit type combinations.
  // Protected because they need to be called from test overrides.
  mojom::FrameNavigationControl::CommitNavigationCallback
  BuildCommitNavigationCallback(NavigationRequest* navigation_request);
  mojom::FrameNavigationControl::CommitFailedNavigationCallback
  BuildCommitFailedNavigationCallback(NavigationRequest* navigation_request);
  mojom::NavigationClient::CommitNavigationCallback
  BuildNavigationClientCommitNavigationCallback(
      NavigationRequest* navigation_request);
  mojom::NavigationClient::CommitFailedNavigationCallback
  BuildNavigationClientCommitFailedNavigationCallback(
      NavigationRequest* navigation_request);

 private:
  friend class RenderFrameHostFeaturePolicyTest;
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;
  friend class WebContentsSplitCacheBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(NavigatorTest, TwoNavigationsRacingCommit);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           SubframeShowsDialogWhenMainFrameNavigates);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBeforeUnloadBrowserTest,
                           TimerNotRestartedBySecondDialog);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, DontSelectInvalidFiles);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreSubframeFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RenderViewInitAfterNewProxyAndProcessKill);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           UnloadPushStateOnCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           WebUIJavascriptDisallowedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, LastCommittedOrigin);
  FRIEND_TEST_ALL_PREFIXES(
      RenderFrameHostManagerUnloadBrowserTest,
      PendingDeleteRFHProcessShutdownDoesNotRemoveSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, FindImmediateLocalRoots);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedSwapOutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostStaysActiveWithLateSwapoutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LoadEventForwardingWhilePendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ContextMenuAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ActiveSandboxFlagsRetainedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LastCommittedURLRetainedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderFrameProxyNotRecreatedDuringProcessShutdown);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           SwapOutACKArrivesPriorToProcessShutdownRequest);
  FRIEND_TEST_ALL_PREFIXES(SecurityExploitBrowserTest,
                           AttemptDuplicateRenderViewHost);
  FRIEND_TEST_ALL_PREFIXES(WebContentsImplBrowserTest,
                           FullscreenAfterFrameSwap);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, UnloadHandlerSubframes);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, Unload_ABAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadNestedPendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, PartialUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           PendingDeletionCheckCompletedOnSubtree);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           DetachedIframeUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionAB);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigationCommitInIframePendingDeletionABC);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           CommittedOriginIncompatibleWithOriginLock);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerSameProcess);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessBrowserTest,
      IsDetachedSubframeObservableDuringUnloadHandlerCrossProcess);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadHandlersArePowerful);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           UnloadHandlersArePowerfulGrandChild);

  class DroppedInterfaceRequestLogger;

  // IPC Message handlers.
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnUpdateState(const PageState& state);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnContextMenu(const ContextMenuParams& params);
  void OnVisualStateResponse(uint64_t id);
  void OnRunJavaScriptDialog(const base::string16& message,
                             const base::string16& default_prompt,
                             JavaScriptDialogType dialog_type,
                             IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(bool is_reload, IPC::Message* reply_msg);
  void OnDidAccessInitialDocument();
  void OnDidChangeOpener(int32_t opener_routing_id);

  // A new set of CSP |policies| has been added to the document.
  void OnDidAddContentSecurityPolicies(
      const std::vector<ContentSecurityPolicy>& policies);

  void OnDidChangeFramePolicy(int32_t frame_routing_id,
                              const blink::FramePolicy& frame_policy);
  void OnDidChangeFrameOwnerProperties(int32_t frame_routing_id,
                                       const FrameOwnerProperties& properties);
  void OnUpdateTitle(const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnDidBlockNavigation(const GURL& blocked_url,
                            const GURL& initiator_url,
                            blink::NavigationBlockedReason reason);
  // Only used with PerNavigationMojoInterface disabled.
  void OnAbortNavigation();
  void OnForwardResourceTimingToParent(
      const ResourceTimingInfo& resource_timing);
  void OnDispatchLoad();
  void OnAccessibilityEvents(
      const AccessibilityHostMsg_EventBundleParams& params,
      int reset_token,
      int ack_token);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnAccessibilityFindInPageResult(
      const AccessibilityHostMsg_FindInPageResultParams& params);
  void OnAccessibilityChildFrameHitTestResult(
      int action_request_id,
      const gfx::Point& point,
      int child_frame_routing_id,
      int child_frame_browser_plugin_instance_id,
      ax::mojom::Event event_to_fire);
  void OnAccessibilitySnapshotResponse(int callback_id,
                                       const AXContentTreeUpdate& snapshot);
  void OnEnterFullscreen(const blink::WebFullscreenOptions& options);
  void OnExitFullscreen();
  void OnSuddenTerminationDisablerChanged(
      bool present,
      blink::WebSuddenTerminationDisablerType disabler_type);
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnSelectionChanged(const base::string16& text,
                          uint32_t offset,
                          const gfx::Range& range);
  void OnFocusedNodeChanged(bool is_editable_element,
                            const gfx::Rect& bounds_in_frame_widget);
  void OnUpdateUserActivationState(blink::UserActivationUpdateType update_type);
  void OnSetHasReceivedUserGestureBeforeNavigation(bool value);
  void OnSetNeedsOcclusionTracking(bool needs_tracking);
  void OnScrollRectToVisibleInParentFrame(
      const gfx::Rect& rect_to_scroll,
      const blink::WebScrollIntoViewParams& params);
  void OnBubbleLogicalScrollInParentFrame(
      blink::WebScrollDirection direction,
      ui::input_types::ScrollGranularity granularity);
  void OnFrameDidCallFocus();
  void OnRenderFallbackContentInParentProcess();

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif
#if defined(OS_ANDROID)
  void ForwardGetInterfaceToRenderFrame(const std::string& interface_name,
                                        mojo::ScopedMessagePipeHandle pipe);
#endif

  // Called when the frame would like an overlay routing token.  This will
  // create one if needed.  Either way, it will send it to the frame.
  void OnRequestOverlayRoutingToken();

  // mojom::FrameHost:
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       CreateNewWindowCallback callback) override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override;
  void AdoptPortal(const base::UnguessableToken& portal_token,
                   AdoptPortalCallback callback) override;
  void IssueKeepAliveHandle(mojom::KeepAliveHandleRequest request) override;
  void DidCommitProvisionalLoad(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params)
      override;

  // This function mimics DidCommitProvisionalLoad but is a direct mojo
  // callback from NavigationClient::CommitNavigation.
  // This only used when PerNavigationMojoInterface is enabled, and will
  // replace DidCommitProvisionalLoad in the long run.
  void DidCommitPerNavigationMojoInterfaceNavigation(
      NavigationRequest* committing_navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  void DidCommitSameDocumentNavigation(
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params) override;
  void BeginNavigation(
      const CommonNavigationParams& common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator) override;
  void SubresourceResponseStarted(const GURL& url,
                                  net::CertStatus cert_status) override;
  void ResourceLoadComplete(
      mojom::ResourceLoadInfoPtr resource_load_info) override;
  void DidChangeName(const std::string& name,
                     const std::string& unique_name) override;
  void EnforceInsecureRequestPolicy(
      blink::WebInsecureRequestPolicy policy) override;
  void EnforceInsecureNavigationsSet(const std::vector<uint32_t>& set) override;
  void DidSetFramePolicyHeaders(
      blink::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& parsed_header) override;
  void CancelInitialHistoryLoad() override;
  void UpdateEncoding(const std::string& encoding) override;
  void FrameSizeChanged(const gfx::Size& frame_size) override;
  void FullscreenStateChanged(bool is_fullscreen) override;
  void LifecycleStateChanged(blink::mojom::FrameLifecycleState state) override;
  void DocumentOnLoadCompleted() override;
  void UpdateActiveSchedulerTrackedFeatures(uint64_t features_mask) override;
  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void DidFailProvisionalLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description,
      bool showing_repost_interstitial) override;
  void DidFailLoadWithError(const GURL& url,
                            int error_code,
                            const base::string16& error_description) override;
  void TransferUserActivationFrom(int32_t source_routing_id) override;
  void ShowCreatedWindow(int32_t pending_widget_routing_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) override;
#if defined(OS_ANDROID)
  void UpdateUserGestureCarryoverInfo() override;
#endif

  // Registers Mojo interfaces that this frame host makes available.
  void RegisterMojoInterfaces();

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given origin is allowed to commit in the current
  // RenderFrameHost. The |url| is used to ensure it matches the origin in cases
  // where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  bool CanCommitOrigin(const url::Origin& origin, const GURL& url);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const network::ResourceRequestBody& body);

  void UpdatePermissionsForNavigation(
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params);

  // Creates a Network Service-backed factory from appropriate |NetworkContext|
  // and sets a connection error handler to trigger
  // |OnNetworkServiceConnectionError()| if the factory is out-of-process.  If
  // this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.
  //
  // |origin| is the origin that the RenderFrame is either committing (in the
  // case of navigation) or has last committed (when handling network process
  // crashes).
  bool CreateNetworkServiceDefaultFactoryAndObserve(
      const base::Optional<url::Origin>& origin,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // |origin| is the origin that the RenderFrame is either committing (in the
  // case of navigation) or has last committed (when handling network process
  // crashes).
  bool CreateNetworkServiceDefaultFactoryInternal(
      const base::Optional<url::Origin>& origin,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  bool CanExecuteJavaScript();

  // Map a routing ID from a frame in the same frame tree to a globally
  // unique AXTreeID.
  ui::AXTreeID RoutingIDToAXTreeID(int routing_id);

  // Map a browser plugin instance ID to the AXTreeID of the plugin's
  // main frame.
  ui::AXTreeID BrowserPluginInstanceIDToAXTreeID(int routing_id);

  // Convert the content-layer-specific AXContentNodeData to a general-purpose
  // AXNodeData structure.
  void AXContentNodeDataToAXNodeData(const AXContentNodeData& src,
                                     ui::AXNodeData* dst);

  // Convert the content-layer-specific AXContentTreeData to a general-purpose
  // AXTreeData structure.
  void AXContentTreeDataToAXTreeData(ui::AXTreeData* dst);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Returns the child FrameTreeNode if |child_frame_routing_id| is an
  // immediate child of this FrameTreeNode.  |child_frame_routing_id| is
  // considered untrusted, so the renderer process is killed if it refers to a
  // FrameTreeNode that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Creates Web Bluetooth Service owned by the frame. Returns a raw pointer
  // to it.
  WebBluetoothServiceImpl* CreateWebBluetoothService(
      blink::mojom::WebBluetoothServiceRequest request);

  // Deletes the Web Bluetooth Service owned by the frame.
  void DeleteWebBluetoothService(
      WebBluetoothServiceImpl* web_bluetooth_service);

  // Creates connections to WebUSB interfaces bound to this frame.
  void CreateWebUsbService(
      mojo::InterfaceRequest<blink::mojom::WebUsbService> request);

  void CreateAudioInputStreamFactory(
      mojom::RendererAudioInputStreamFactoryRequest request);
  void CreateAudioOutputStreamFactory(
      mojom::RendererAudioOutputStreamFactoryRequest request);

  void BindMediaInterfaceFactoryRequest(
      media::mojom::InterfaceFactoryRequest request);

  void CreateWebSocketConnector(
      blink::mojom::WebSocketConnectorRequest request);

  void CreateDedicatedWorkerHostFactory(
      blink::mojom::DedicatedWorkerHostFactoryRequest request);

  // Callback for connection error on the media::mojom::InterfaceFactory client.
  void OnMediaInterfaceFactoryConnectionError();

#if defined(OS_ANDROID)
  void BindNFCRequest(device::mojom::NFCRequest request);
#endif

#if !defined(OS_ANDROID)
  void BindSerialServiceRequest(blink::mojom::SerialServiceRequest request);
  void BindAuthenticatorRequest(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver);
#endif

  void BindPresentationServiceRequest(
      blink::mojom::PresentationServiceRequest request);

  void BindIdleManagerRequest(blink::mojom::IdleManagerRequest request);

  void BindSmsReceiverRequest(blink::mojom::SmsReceiverRequest request);

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // blink::mojom::DocumentInterfaceBroker:
  void GetFrameHostTestInterface(
      mojo::PendingReceiver<blink::mojom::FrameHostTestInterface> receiver)
      override;
  void GetAudioContextManager(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver)
      override;
  void GetCredentialManager(
      mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) override;
  void GetAuthenticator(
      mojo::PendingReceiver<blink::mojom::Authenticator> receiver) override;
  void GetPushMessaging(
      mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) override;
  void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver) override;
  void RegisterAppCacheHost(
      mojo::PendingReceiver<blink::mojom::AppCacheHost> host_receiver,
      mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend_remote,
      const base::UnguessableToken& host_id) override;

  // Allows tests to disable the swapout event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableSwapOutTimerForTesting();

  void SendJavaScriptDialogReply(IPC::Message* reply_msg,
                                 bool success,
                                 const base::string16& user_input);

  // Creates a NavigationRequest to use for commit. This should only be used
  // when no appropriate NavigationRequest has been found.
  std::unique_ptr<NavigationRequest> CreateNavigationRequestForCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_same_document,
      NavigationEntryImpl* entry_for_request);

  // Whether the |request| corresponds to a navigation to the pending
  // NavigationEntry. This is used at commit time, when the NavigationRequest
  // does not match the data sent by the renderer to re-create a
  // NavigationRequest and associate it with the pending NavigationEntry if
  // needed.
  // TODO(clamy): We should handle the mismatches gracefully without deleting
  // the NavigationRequest and having to re-create one.
  bool NavigationRequestWasIntendedForPendingEntry(
      NavigationRequest* request,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool same_document);

  // Helper to process the beforeunload ACK. |proceed| indicates whether the
  // navigation or tab close should be allowed to proceed.  If
  // |treat_as_final_ack| is true, the frame should stop waiting for any
  // further ACKs from subframes. ACKs received from the renderer set
  // |treat_as_final_ack| to false, whereas a beforeunload timeout sets it to
  // true.
  void ProcessBeforeUnloadACK(
      bool proceed,
      bool treat_as_final_ack,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);

  // Find the frame that triggered the beforeunload handler to run in this
  // frame, which might be the frame itself or its ancestor.  This will
  // return the frame that is navigating, or the main frame if beforeunload was
  // triggered by closing the current tab.  It will return null if no
  // beforeunload is currently in progress.
  RenderFrameHostImpl* GetBeforeUnloadInitiator();

  // Called when a particular frame finishes running a beforeunload handler,
  // possibly as part of processing beforeunload for an ancestor frame.  In
  // that case, this is called on the ancestor frame that is navigating or
  // closing, and |frame| indicates which beforeunload ACK is received.  If a
  // beforeunload timeout occurred, |treat_as_final_ack| is set to true.
  // |is_frame_being_destroyed| is set to true if this was called as part of
  // destroying |frame|.
  void ProcessBeforeUnloadACKFromFrame(
      bool proceed,
      bool treat_as_final_ack,
      RenderFrameHostImpl* frame,
      bool is_frame_being_destroyed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);

  // Helper function to check whether the current frame and its subframes need
  // to run beforeunload and, if |send_ipc| is true, send all the necessary
  // IPCs for this frame's subtree. If |send_ipc| is false, this only checks
  // whether beforeunload is needed and returns the answer.  |subframes_only|
  // indicates whether to only check subframes of the current frame, and skip
  // the current frame itself.
  bool CheckOrDispatchBeforeUnloadForSubtree(bool subframes_only,
                                             bool send_ipc,
                                             bool is_reload);

  // Called by |beforeunload_timeout_| when the beforeunload timeout fires.
  void BeforeUnloadTimeout();

  // Update this frame's last committed origin.
  void SetLastCommittedOrigin(const url::Origin& origin);

  // Set the |last_committed_origin_| of |this| frame, inheriting the origin
  // from |new_frame_creator| as appropriate (e.g. depending on whether |this|
  // frame should be sandboxed / should have an opaque origin instead).
  void SetOriginOfNewFrame(const url::Origin& new_frame_creator);

  // Called when a navigation commits succesfully to |url|. This will update
  // |last_committed_site_url_| with the site URL corresponding to |url|.
  // Note that this will recompute the site URL from |url| rather than using
  // GetSiteInstance()->GetSiteURL(), so that |last_committed_site_url_| is
  // always meaningful: e.g., without site isolation, b.com could commit in a
  // SiteInstance for a.com, but this function will still compute the last
  // committed site URL as b.com.  For example, this can be used to track which
  // sites have committed in which process.
  void SetLastCommittedSiteUrl(const GURL& url);

  // Clears any existing policy and constructs a new policy for this frame,
  // based on its parent frame.
  void ResetFeaturePolicy();

  // TODO(ekaramad): One major purpose behind the API is to traverse the frame
  // tree top-down to visit the  RenderWidgetHostViews of interest in the most
  // efficient way. We might want to revisit this API, remove it from RFHImpl,
  // and perhaps consolidate it with some of the existing ones such as
  // WebContentsImpl::GetRenderWidgetHostViewsInTree() into a new more
  // appropriate API for dealing with (virtual) RenderWidgetHost(View) tree.
  // (see https://crbug.com/754726).
  // Runs |callback| for all the local roots immediately under this frame, i.e.
  // local roots which are under this frame and their first ancestor which is a
  // local root is either this frame or this frame's local root. For instance,
  // in a frame tree such as:
  //                    A0                   //
  //                 /  |   \                //
  //                B   A1   E               //
  //               /   /  \   \              //
  //              D  A2    C   F             //
  // RFHs at nodes B, E, D, C, and F are all local roots in the given frame tree
  // under the root at A0, but only B, C, and E are considered immediate local
  // roots of A0. Note that this will exclude any speculative or pending RFHs.
  void ForEachImmediateLocalRoot(
      const base::Callback<void(RenderFrameHostImpl*)>& callback);

  // Lazily initializes and returns the mojom::FrameNavigationControl interface
  // for this frame. May be overridden by friend subclasses for e.g. tests which
  // wish to intercept outgoing navigation control messages.
  virtual mojom::FrameNavigationControl* GetNavigationControl();

  // Utility function used to validate potentially harmful parameters sent by
  // the renderer during the commit notification.
  // A return value of true means that the commit should proceed.
  bool ValidateDidCommitParams(
      NavigationRequest* navigation_request,
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
      bool is_same_document_navigation);

  // Updates the site url if the navigation was successful and the page is not
  // an interstitial.
  void UpdateSiteURL(const GURL& url, bool url_is_unreachable);

  // The actual implementation of DidCommitProvisionalLoad and
  // DidCommitPerNavigationMojoInterfaceNavigation.
  void DidCommitNavigation(
      std::unique_ptr<NavigationRequest> committing_navigation_request,
      std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
          validated_params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

  // Called when we receive the confirmation that a navigation committed in the
  // renderer. Used by both DidCommitSameDocumentNavigation and
  // DidCommitNavigation.
  // Returns true if the navigation did commit properly, false if the commit
  // state should be restored to its pre-commit value.
  bool DidCommitNavigationInternal(
      std::unique_ptr<NavigationRequest> navigation_request,
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
      bool is_same_document_navigation);

  // Called by the renderer process when it is done processing a same-document
  // commit request.
  void OnSameDocumentCommitProcessed(int64_t navigation_id,
                                     bool should_replace_current_entry,
                                     blink::mojom::CommitResult result);

  // Called by the renderer process when it is done processing a cross-document
  // commit request.
  void OnCrossDocumentCommitProcessed(NavigationRequest* navigation_request,
                                      blink::mojom::CommitResult result);

  // Creates a TracedValue object containing the details of a committed
  // navigation, so it can be logged with the tracing system.
  std::unique_ptr<base::trace_event::TracedValue> CommitAsTracedValue(
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params) const;

  // Creates initiator-specific URLLoaderFactory objects for
  // |initiator_origins|.
  blink::URLLoaderFactoryBundleInfo::OriginMap
  CreateInitiatorSpecificURLLoaderFactories(
      const base::flat_set<url::Origin>& initiator_origins);

  // Based on the termination |status| and |exit_code|, may generate a crash
  // report to be routed to the Reporting API.
  void MaybeGenerateCrashReport(base::TerminationStatus status, int exit_code);

  // Move every child frame into the pending deletion state.
  // For each process, send the command to delete the local subtree and execute
  // the unload handlers.
  void StartPendingDeletionOnSubtree();

  // This function checks whether a pending deletion frame and all of its
  // subframes have completed running unload handlers. If so, this function
  // destroys this frame. This will happen as soon as...
  // 1) The children in other processes have been deleted.
  // 2) The ack (FrameHostMsg_Swapout_ACK or FrameHostMsg_Detach) has been
  //    received. It means this frame in the renderer process is gone.
  void PendingDeletionCheckCompleted();

  // Call |PendingDeletionCheckCompleted| recursively on this frame and its
  // children. This is useful for pruning frames with no unload handlers from
  // this frame's subtree.
  void PendingDeletionCheckCompletedOnSubtree();

  // In this frame and its children, removes every:
  // - NavigationRequest.
  // - Speculative RenderFrameHost.
  void ResetNavigationsForPendingDeletion();

  // Called on an unloading frame when its unload timeout is reached. This
  // immediately deletes the RenderFrameHost.
  void OnUnloadTimeout();

  // Update the frozen state of the frame applying current inputs (visibility,
  // loaded state) to determine the new state.
  void UpdateFrameFrozenState();

  // Runs interception set up in testing code, if any.
  // Returns true if we should proceed to the Commit callback, false otherwise.
  bool MaybeInterceptCommitCallback(
      NavigationRequest* navigation_request,
      FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params);

  // If this RenderFrameHost is a local root (i.e., either the main frame or a
  // subframe in a different process than its parent), this returns the
  // RenderWidgetHost corresponding to this frame. Otherwise this returns null.
  // See also GetRenderWidgetHost(), which walks up the tree to find the nearest
  // local root.
  // Main frame: RenderWidgetHost is owned by the RenderViewHost.
  // Subframe: RenderWidgetHost is owned by this RenderFrameHost.
  RenderWidgetHostImpl* GetLocalRenderWidgetHost() const;

  // Called after a new document commit. Every children of the previous document
  // are expected to be deleted or at least to be pending deletion waiting for
  // unload completion. A compromised renderer process or bugs can cause the
  // renderer to "forget" to start deletion. In this case the browser deletes
  // them immediately, without waiting for unload completion.
  // https://crbug.com/950625.
  void EnsureDescendantsAreUnloading();

  // Implements AddMessageToConsole() and AddUniqueMessageToConsole().
  void AddMessageToConsoleImpl(blink::mojom::ConsoleMessageLevel level,
                               const std::string& message,
                               bool discard_duplicates);

  // Returns whether a cookie SameSite deprecation message should be sent for
  // the given cookie url.
  // TODO(crbug.com/977040): Remove when no longer needed.
  bool ShouldAddCookieSameSiteDeprecationMessage(
      const std::string& cookie_url,
      base::circular_deque<size_t>* already_seen_url_hashes);

  // The RenderViewHost that this RenderFrameHost is associated with.
  //
  // It is kept alive as long as any RenderFrameHosts or RenderFrameProxyHosts
  // are using it.
  //
  // The refcount allows each RenderFrameHostManager to just care about
  // RenderFrameHosts, while ensuring we have a RenderViewHost for each
  // RenderFrameHost.
  //
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  const scoped_refptr<RenderViewHostImpl> render_view_host_;

  RenderFrameHostDelegate* const delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The renderer process this RenderFrameHost is associated with. It is
  // initialized through a call to site_instance_->GetProcess() at creation
  // time. RenderFrameHost::GetProcess() uses this cached pointer to avoid
  // recreating the renderer process if it has crashed, since using
  // SiteInstance::GetProcess() has the side effect of creating the process
  // again if it is gone.
  RenderProcessHost* const process_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* const frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* const frame_tree_node_;

  // The immediate children of this specific frame.
  std::vector<std::unique_ptr<FrameTreeNode>> children_;

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value.
  RenderFrameHostImpl* parent_;

  // Track this frame's last committed URL.
  GURL last_committed_url_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  // Track the site URL of the last site we committed successfully, as obtained
  // from SiteInstance::GetSiteURL.
  GURL last_committed_site_url_;

  // The most recent non-error URL to commit in this frame.  Remove this in
  // favor of GetLastCommittedURL() once PlzNavigate is enabled or cross-process
  // transfers work for net errors.  See https://crbug.com/588314.
  GURL last_successful_url_;

  std::map<uint64_t, VisualStateCallback> visual_state_callbacks_;

  // Local root subframes directly own their RenderWidgetHost.
  // Please see comments about the GetLocalRenderWidgetHost() function.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  std::unique_ptr<RenderWidgetHostImpl> owned_render_widget_host_;

  const int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for FrameHostMsg_SwapOut_ACK and thus pending deletion.
  bool is_waiting_for_swapout_ack_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  Currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  bool render_frame_created_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when |is_waiting_for_beforeunload_ack_| is true. This indicates
  // whether a subsequent request to launch a modal dialog should be honored or
  // whether it should implicitly cause the unload to be canceled.
  bool beforeunload_dialog_request_cancels_unload_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true).
  bool unload_ack_is_for_navigation_;

  // The timeout monitor that runs from when the beforeunload is started in
  // DispatchBeforeUnload() until either the render process ACKs it with an IPC
  // to OnBeforeUnloadACK(), or until the timeout triggers.
  std::unique_ptr<TimeoutMonitor> beforeunload_timeout_;

  // The delay to use for the beforeunload timeout monitor above.
  base::TimeDelta beforeunload_timeout_delay_;

  // When this frame is asked to execute beforeunload, this maintains a list of
  // frames that need to receive beforeunload ACKs.  This may include this
  // frame and/or its descendant frames.  This excludes frames that don't have
  // beforeunload handlers defined.
  //
  // TODO(alexmos): For now, this always includes the navigating frame.  Make
  // this include the navigating frame only if it has a beforeunload handler
  // defined.
  std::set<RenderFrameHostImpl*> beforeunload_pending_replies_;

  // During beforeunload, keeps track whether a dialog has already been shown.
  // Used to enforce at most one dialog per navigation.  This is tracked on the
  // frame that is being navigated, and not on any of its subframes that might
  // have triggered a dialog.
  bool has_shown_beforeunload_dialog_ = false;

  // Returns whether the tab was previously discarded.
  // This is passed to CommitNavigationParams in NavigationRequest.
  bool was_discarded_;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_;

  // Used to swap out or shut down this RFH when the unload event is taking too
  // long to execute, depending on the number of active frames in the
  // SiteInstance.  May be null in tests.
  std::unique_ptr<TimeoutMonitor> swapout_event_monitor_timeout_;

  // GeolocationService which provides Geolocation.
  std::unique_ptr<GeolocationServiceImpl> geolocation_service_;

  // SensorProvider proxy which acts as a gatekeeper to the real SensorProvider.
  std::unique_ptr<SensorProviderProxyImpl> sensor_provider_proxy_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;
  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  std::list<std::unique_ptr<WebBluetoothServiceImpl>> web_bluetooth_services_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // The last AXContentTreeData for this frame received from the RenderFrame.
  AXContentTreeData ax_content_tree_data_;

  // The AX tree ID of the embedder, if this is a browser plugin guest.
  ui::AXTreeID browser_plugin_embedder_ax_tree_id_;

  // The mapping from callback id to corresponding callback for pending
  // accessibility tree snapshot calls created by RequestAXTreeSnapshot.
  std::map<int, AXTreeSnapshotCallback> ax_tree_snapshot_callbacks_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
#if defined(OS_ANDROID)
  base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>
      smart_clip_callbacks_;
#endif  // defined(OS_ANDROID)

  // Callback when an event is received, for testing.
  AccessibilityCallbackForTesting accessibility_testing_callback_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // Context shared for each mojom::PermissionService instance created for this
  // RFH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // Holder of Mojo connection with ImageDownloader service in Blink.
  blink::mojom::ImageDownloaderPtr mojo_image_downloader_;

  // Holder of Mojo connection with FindInPage service in Blink.
  mojo::AssociatedRemote<blink::mojom::FindInPage> find_in_page_;

  // Holds a NavigationRequest when it's about to commit, ie. after
  // OnCrossDocumentCommitProcessed has returned a positive answer for this
  // NavigationRequest but before receiving DidCommitProvisionalLoad. This
  // NavigationRequest is for a cross-document navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // Holds AppCacheNavigationHandle after navigation request has been committed,
  // which keeps corresponding AppCacheHost alive while renderer asks for it.
  // See AppCacheNavigationHandle comment for more details.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  // Holds the cross-document NavigationRequests that are waiting to commit,
  // indexed by IDs. These are navigations that have passed ReadyToCommit stage
  // and are waiting for the renderer to send back a matching
  // OnCrossDocumentCommitProcessed.

  // TODO(ahemery): We have this storage as a map because we actually want to
  // find navigations by id with PerNavigationMojoInterface disabled.
  // When the flag is always on, rework the structure to simply store an
  // unindexed bunch of ongoing navigations and modify
  // DidCommitNavigationInternal.
  std::map<NavigationRequest*, std::unique_ptr<NavigationRequest>>
      navigation_requests_;

  // Holds a same-document NavigationRequest while waiting for the navigation it
  // is tracking to commit.
  std::unique_ptr<NavigationRequest> same_document_navigation_request_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_;

  // The pending WebUIImpl and its type. These values will be used exclusively
  // for same-site navigations to keep a transition of a WebUI in a pending
  // state until the navigation commits.
  std::unique_ptr<WebUIImpl> pending_web_ui_;
  WebUI::TypeID pending_web_ui_type_;

  // If true the associated WebUI should be reused when CommitPendingWebUI is
  // called (no pending instance should be set).
  bool should_reuse_web_ui_;

  // If true, then the RenderFrame has selected text.
  bool has_selection_;

  // If true, then this RenderFrame has one or more audio streams with audible
  // signal. If false, all audio streams are currently silent (or there are no
  // audio streams).
  bool is_audible_;

  // Used for tracking the latest size of the RenderFrame.
  base::Optional<gfx::Size> frame_size_;

  // The Previews state of the last navigation. This is used during history
  // navigation of subframes to ensure that subframes navigate with the same
  // Previews status as the top-level frame.
  PreviewsState last_navigation_previews_state_;

  bool has_committed_any_navigation_ = false;
  mojo::AssociatedBinding<mojom::FrameHost> frame_host_associated_binding_;
  mojom::FramePtr frame_;
  mojom::FrameBindingsControlAssociatedPtr frame_bindings_control_;
  mojo::AssociatedRemote<mojom::FrameNavigationControl> navigation_control_;

  // If this is true then this object was created in response to a renderer
  // initiated request. Init() will be called, and until then navigation
  // requests should be queued.
  bool waiting_for_init_;

  // If true then this frame's document has a focused element which is editable.
  bool has_focused_editable_element_;

  std::unique_ptr<PendingNavigation> pending_navigate_;

  // A collection of non-network URLLoaderFactory implementations which are used
  // to service any supported non-network subresource requests for the currently
  // committed navigation.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  // Bitfield for renderer-side state that blocks fast shutdown of the frame.
  blink::WebSuddenTerminationDisablerType
      sudden_termination_disabler_types_enabled_ = 0;

  // We switch between |audio_service_audio_output_stream_factory_| and
  // |in_content_audio_output_stream_factory_| based on
  // features::kAudioServiceAudioStreams status.
  base::Optional<RenderFrameAudioOutputStreamFactory>
      audio_service_audio_output_stream_factory_;
  UniqueAudioOutputStreamFactoryPtr in_content_audio_output_stream_factory_;

  // We switch between |audio_service_audio_input_stream_factory_| and
  // |in_content_audio_input_stream_factory_| based on
  // features::kAudioServiceAudioStreams status.
  base::Optional<RenderFrameAudioInputStreamFactory>
      audio_service_audio_input_stream_factory_;
  UniqueAudioInputStreamFactoryPtr in_content_audio_input_stream_factory_;

  std::unique_ptr<MediaStreamDispatcherHost, BrowserThread::DeleteOnIOThread>
      media_stream_dispatcher_host_;

  // Hosts media::mojom::InterfaceFactory for the RenderFrame and forwards
  // media::mojom::InterfaceFactory calls to the remote "media" service.
  std::unique_ptr<MediaInterfaceProxy> media_interface_proxy_;

#if !defined(OS_ANDROID)
  // Hosts blink::mojom::SerialService for the RenderFrame.
  std::unique_ptr<SerialService> serial_service_;
#endif

  // Hosts blink::mojom::PresentationService for the RenderFrame.
  std::unique_ptr<PresentationServiceImpl> presentation_service_;

  // Hosts blink::mojom::FileSystemManager for the RenderFrame.
  std::unique_ptr<FileSystemManagerImpl, BrowserThread::DeleteOnIOThread>
      file_system_manager_;

  // Hosts blink::mojom::PushMessaging for the RenderFrame.
  std::unique_ptr<PushMessagingManager, BrowserThread::DeleteOnIOThread>
      push_messaging_manager_;

#if !defined(OS_ANDROID)
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
#endif

  std::unique_ptr<blink::AssociatedInterfaceProvider>
      remote_associated_interfaces_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // Tracks the feature policy which has been set on this frame.
  std::unique_ptr<blink::FeaturePolicy> feature_policy_;

  // Tracks the sandbox flags which are in effect on this frame. This includes
  // any flags which have been set by a Content-Security-Policy header, in
  // addition to those which are set by the embedding frame. This is initially a
  // copy of the active sandbox flags which are stored in the FrameTreeNode for
  // this RenderFrameHost, but may diverge if this RenderFrameHost is pending
  // deletion.
  blink::WebSandboxFlags active_sandbox_flags_;

#if defined(OS_ANDROID)
  // An InterfaceProvider for Java-implemented interfaces that are scoped to
  // this RenderFrameHost. This provides access to interfaces implemented in
  // Java in the browser process to C++ code in the browser process.
  std::unique_ptr<service_manager::InterfaceProvider> java_interfaces_;

  // An InterfaceRegistry that forwards interface requests from Java to the
  // RenderFrame. This provides access to interfaces implemented in the renderer
  // to Java code in the browser process.
  class JavaInterfaceProvider;
  std::unique_ptr<JavaInterfaceProvider> java_interface_registry_;
#endif

  // Binding for the InterfaceProvider through which this RenderFrameHostImpl
  // exposes frame-scoped Mojo services to the currently active document in the
  // corresponding RenderFrame.
  //
  // GetInterface messages dispatched through this binding are guaranteed to
  // originate from the document corresponding to the last committed navigation;
  // or the inital empty document if no real navigation has ever been committed.
  //
  // The InterfaceProvider interface connection is established as follows:
  //
  // 1) For the initial empty document, the call site that creates this
  //    RenderFrameHost is responsible for creating a message pipe, binding its
  //    request end to this instance by calling BindInterfaceProviderRequest(),
  //    and plumbing the client end to the renderer process, and ultimately
  //    supplying it to the RenderFrame synchronously at construction time.
  //
  //    The only exception to this rule are out-of-process child frames, whose
  //    RenderFrameHosts take care of this internally in CreateRenderFrame().
  //
  // 2) For subsequent documents, the RenderFrame creates a new message pipe
  //    every time a cross-document navigation is committed, and pushes its
  //    request end to the browser process as part of DidCommitProvisionalLoad.
  //    The client end will be used by the new document corresponding to the
  //    committed naviagation to access services exposed by the RenderFrameHost.
  //
  // This is required to prevent GetInterface messages racing with navigation
  // commit from being serviced in the security context corresponding to the
  // wrong document in the RenderFrame. The benefit of the approach taken is
  // that it does not necessitate using channel-associated InterfaceProvider
  // interfaces.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      document_scoped_interface_provider_binding_;

  // Bindings for the DocumentInterfaceBroker through which this
  // RenderFrameHostImpl exposes document-scoped Mojo services to the currently
  // active document in the corresponding RenderFrame. Because of the type
  // difference between content and blink, two separate pipes are used.
  mojo::Binding<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_content_binding_;
  mojo::Binding<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_blink_binding_;

  // Logs interface requests that arrive after the frame has already committed a
  // non-same-document navigation, and has already unbound
  // |document_scoped_interface_provider_binding_| from the interface connection
  // that had been used to service RenderFrame::GetRemoteInterface for the
  // previously active document in the frame.
  std::unique_ptr<DroppedInterfaceRequestLogger>
      dropped_interface_request_logger_;

  // IPC-friendly token that represents this host for AndroidOverlays, if we
  // have created one yet.
  base::Optional<base::UnguessableToken> overlay_routing_token_;

  viz::mojom::InputTargetClient* input_target_client_ = nullptr;
  mojo::Remote<mojom::FrameInputHandler> frame_input_handler_;

  std::unique_ptr<KeepAliveHandleFactory> keep_alive_handle_factory_;
  base::TimeDelta keep_alive_timeout_;

  // For observing Network Service connection errors only. Will trigger
  // |OnNetworkServiceConnectionError()| and push updated factories to
  // |RenderFrame|.
  network::mojom::URLLoaderFactoryPtr
      network_service_connection_error_handler_holder_;

  // Whether UpdateSubresourceLoaderFactories should recreate the default
  // URLLoaderFactory when handling a NetworkService crash.  In case the frame
  // is covered by AppCache, only initiator-specific factories need to be
  // refreshed, but the main, AppCache-specific factory shouldn't be refreshed.
  bool recreate_default_url_loader_factory_after_network_service_crash_ = false;

  // Set of request-initiator-origins that require a separate URLLoaderFactory
  // (e.g. for handling requests initiated by extension content scripts that
  // require relaxed CORS/CORB rules).
  base::flat_set<url::Origin> initiators_requiring_separate_url_loader_factory_;

  // Holds the renderer generated ID and global request ID for the main frame
  // request.
  std::pair<int, GlobalRequestID> main_frame_request_ids_;

  // If |ResourceLoadComplete()| is called for the main resource before
  // |DidCommitProvisionalLoad()|, the load info is saved here to call
  // |ResourceLoadComplete()| when |DidCommitProvisionalLoad()| is called. This
  // is necessary so the renderer ID can be mapped to the global ID in
  // |DidCommitProvisionalLoad()|. This situation should only happen when an
  // empty document is loaded.
  mojom::ResourceLoadInfoPtr deferred_main_frame_load_info_;

  enum class UnloadState {
    // The initial state. The frame is alive.
    NotRun,

    // An event such as a navigation happened causing the frame to start its
    // deletion. IPC are sent to execute the unload handlers and delete the
    // RenderFrame. The RenderFrameHost is waiting for an ACK. Either
    // FrameHostMsg_Swapout_ACK for the navigating frame, or FrameHostMsg_Detach
    // for its subframe.
    InProgress,

    // The unload handlers have run. Once all the descendant frames in other
    // processes are gone, this RenderFrameHost can delete itself too.
    Completed,
  };
  UnloadState unload_state_ = UnloadState::NotRun;

  // If a subframe failed to finish running its unload handler after
  // |subframe_unload_timeout_| the RenderFrameHost is deleted.
  base::TimeDelta subframe_unload_timeout_;

  // Call OnUnloadTimeout() when the unload timer expires.
  base::OneShotTimer subframe_unload_timer_;

  // BackForwardCache:
  bool is_in_back_forward_cache_ = false;

  blink::mojom::FrameVisibility visibility_ =
      blink::mojom::FrameVisibility::kRenderedInViewport;

  // Whether the currently committed document is MHTML or not. It is set at
  // commit time based on the MIME type of the NavigationRequest that resulted
  // in the navigation commit. Setting the value should be based only on
  // browser side state as this value is used in security checks.
  bool is_mhtml_document_ = false;

  // Used to intercept DidCommit* calls in tests.
  CommitCallbackInterceptor* commit_callback_interceptor_;

  // Mask of the active features tracked by the scheduler used by this frame.
  // This is used only for metrics.
  // See blink::SchedulingPolicy::Feature for the meaning.
  // These values should be cleared on document commit.
  // Both are needed as some features are tracked in the renderer process and
  // some in the browser process, depending on the design of each individual
  // feature. They are tracked separately, because when the renderer updates the
  // set of features, the browser ones should persist.
  uint64_t renderer_reported_scheduler_tracked_features_ = 0;
  uint64_t browser_reported_scheduler_tracked_features_ = 0;

  // Holds prefetched signed exchanges for SignedExchangeSubresourcePrefetch.
  // They will be passed to the next navigation.
  scoped_refptr<PrefetchedSignedExchangeCache>
      prefetched_signed_exchange_cache_;

  // Network isolation key to be used for subresources from the currently
  // committed navigation. This is specific to a document and should be reset on
  // every document commit.
  net::NetworkIsolationKey network_isolation_key_;

  // Hold onto hashes of the last |kMaxCookieSameSiteDeprecationUrls| cookie
  // URLs that we have seen since the last committed navigation, in order to
  // partially deduplicate the corresponding cookie SameSite deprecation
  // messages.
  // TODO(crbug.com/977040): Remove when no longer needed.
  base::circular_deque<size_t> cookie_no_samesite_deprecation_url_hashes_;
  base::circular_deque<size_t>
      cookie_samesite_none_insecure_deprecation_url_hashes_;

  // The lifecycle state of the frame.
  blink::mojom::FrameLifecycleState frame_lifecycle_state_ =
      blink::mojom::FrameLifecycleState::kRunning;

  // The factory to load resources from the BundledExchanges source bound to
  // this file.
  std::unique_ptr<BundledExchangesFactory> bundled_exchanges_factory_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
