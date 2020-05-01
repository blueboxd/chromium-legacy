// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/zx/channel.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/media_control/browser/media_blocker.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/event_filter.h"
#include "fuchsia/engine/browser/frame_permission_controller.h"
#include "fuchsia/engine/browser/navigation_controller_impl.h"
#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"
#include "fuchsia/engine/on_load_script_injector.mojom.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/focus_controller.h"
#include "url/gurl.h"

namespace content {
class FromRenderFrameHost;
}  // namespace content

class ContextImpl;
class FrameWindowTreeHost;
class FrameLayoutManager;
class MediaPlayerImpl;

// Implementation of fuchsia.web.Frame based on content::WebContents.
class FrameImpl : public fuchsia::web::Frame,
                  public content::WebContentsObserver,
                  public content::WebContentsDelegate {
 public:
  // Returns FrameImpl that owns the |render_frame_host| or nullptr if the
  // |render_frame_host| is not owned by a FrameImpl.
  static FrameImpl* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  FrameImpl(std::unique_ptr<content::WebContents> web_contents,
            ContextImpl* context,
            fidl::InterfaceRequest<fuchsia::web::Frame> frame_request);
  ~FrameImpl() override;

  FrameImpl(const FrameImpl&) = delete;
  FrameImpl& operator=(const FrameImpl&) = delete;

  uint64_t media_session_id() const { return media_session_id_; }

  FramePermissionController* permission_controller() {
    return &permission_controller_;
  }

  zx::unowned_channel GetBindingChannelForTest() const;
  content::WebContents* web_contents_for_test() const {
    return web_contents_.get();
  }
  bool has_view_for_test() const { return window_tree_host_ != nullptr; }
  void set_javascript_console_message_hook_for_test(
      base::RepeatingCallback<void(base::StringPiece)> hook) {
    console_log_message_hook_ = std::move(hook);
  }
  AccessibilityBridge* accessibility_bridge_for_test() const {
    return accessibility_bridge_.get();
  }
  void set_semantics_manager_for_test(
      fuchsia::accessibility::semantics::SemanticsManagerPtr
          semantics_manager) {
    semantics_manager_for_test_ = std::move(semantics_manager);
  }
  void set_handle_actions_for_test(bool handle) {
    accessibility_bridge_->set_handle_actions_for_test(handle);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, DelayedNavigationEventAck);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NavigationObserverDisconnected);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, NoNavigationObserverAttached);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, ReloadFrame);
  FRIEND_TEST_ALL_PREFIXES(FrameImplTest, Stop);

  class OriginScopedScript {
   public:
    OriginScopedScript();
    OriginScopedScript(std::vector<std::string> origins,
                       base::ReadOnlySharedMemoryRegion script);
    OriginScopedScript& operator=(OriginScopedScript&& other);
    ~OriginScopedScript();

    const std::vector<std::string>& origins() const { return origins_; }
    const base::ReadOnlySharedMemoryRegion& script() const { return script_; }

   private:
    std::vector<std::string> origins_;

    // A shared memory buffer containing the script, encoded as UTF16.
    base::ReadOnlySharedMemoryRegion script_;

    DISALLOW_COPY_AND_ASSIGN(OriginScopedScript);
  };

  aura::Window* root_window() const;

  // Shared implementation for the ExecuteJavaScript[NoResult]() APIs.
  void ExecuteJavaScriptInternal(std::vector<std::string> origins,
                                 fuchsia::mem::Buffer script,
                                 ExecuteJavaScriptCallback callback,
                                 bool need_result);

  // Sends the next entry in |pending_popups_| to |popup_listener_|.
  void MaybeSendPopup();

  void OnPopupListenerDisconnected(zx_status_t status);

  // Cleans up the MediaPlayerImpl on disconnect.
  void OnMediaPlayerDisconnect();

  // Initializes WindowTreeHost for the view with the specified |view_token|.
  // |view_token| may be uninitialized in headless mode.
  void InitWindowTreeHost(fuchsia::ui::views::ViewToken view_token);

  // Destroys the WindowTreeHost along with its view or other associated
  // resources.
  void DestroyWindowTreeHost();

  // Destroys |this| and sends the FIDL |error| to the client.
  void CloseAndDestroyFrame(zx_status_t error);

  // fuchsia::web::Frame implementation.
  void CreateView(fuchsia::ui::views::ViewToken view_token) override;
  void GetMediaPlayer(fidl::InterfaceRequest<fuchsia::media::sessions2::Player>
                          player) override;
  void GetNavigationController(
      fidl::InterfaceRequest<fuchsia::web::NavigationController> controller)
      override;
  void ExecuteJavaScript(std::vector<std::string> origins,
                         fuchsia::mem::Buffer script,
                         ExecuteJavaScriptCallback callback) override;
  void ExecuteJavaScriptNoResult(
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      ExecuteJavaScriptNoResultCallback callback) override;
  void AddBeforeLoadJavaScript(
      uint64_t id,
      std::vector<std::string> origins,
      fuchsia::mem::Buffer script,
      AddBeforeLoadJavaScriptCallback callback) override;
  void RemoveBeforeLoadJavaScript(uint64_t id) override;
  void PostMessage(std::string origin,
                   fuchsia::web::WebMessage message,
                   fuchsia::web::Frame::PostMessageCallback callback) override;
  void SetNavigationEventListener(
      fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener)
      override;
  void SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel level) override;
  void ConfigureInputTypes(fuchsia::web::InputTypes types,
                           fuchsia::web::AllowInputState allow) override;
  void SetPopupFrameCreationListener(
      fidl::InterfaceHandle<fuchsia::web::PopupFrameCreationListener> listener)
      override;
  void SetUrlRequestRewriteRules(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
      SetUrlRequestRewriteRulesCallback callback) override;
  void EnableHeadlessRendering() override;
  void DisableHeadlessRendering() override;
  void SetMediaSessionId(uint64_t session_id) override;
  void ForceContentDimensions(
      std::unique_ptr<fuchsia::ui::gfx::vec2> web_dips) override;
  void SetPermissionState(fuchsia::web::PermissionDescriptor permission,
                          std::string web_origin,
                          fuchsia::web::PermissionState state) override;
  void SetBlockMediaLoading(bool blocked) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;

  // content::WebContentsDelegate implementation.
  void CloseContents(content::WebContents* source) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // content::WebContentsObserver implementation.
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;

  const std::unique_ptr<content::WebContents> web_contents_;
  ContextImpl* const context_;

  std::unique_ptr<FrameWindowTreeHost> window_tree_host_;

  std::unique_ptr<wm::FocusController> focus_controller_;

  // Owned via |window_tree_host_|.
  FrameLayoutManager* layout_manager_ = nullptr;

  std::unique_ptr<AccessibilityBridge> accessibility_bridge_;
  fuchsia::accessibility::semantics::SemanticsManagerPtr
      semantics_manager_for_test_;

  EventFilter event_filter_;
  NavigationControllerImpl navigation_controller_;
  logging::LogSeverity log_level_;
  std::map<uint64_t, OriginScopedScript> before_load_scripts_;
  std::vector<uint64_t> before_load_scripts_order_;
  base::RepeatingCallback<void(base::StringPiece)> console_log_message_hook_;
  UrlRequestRewriteRulesManager url_request_rewrite_rules_manager_;
  FramePermissionController permission_controller_;

  // Session ID to use for fuchsia.media.AudioConsumer. Set with
  // SetMediaSessionId().
  uint64_t media_session_id_ = 0;

  // Used for receiving and dispatching popup created by this Frame.
  fuchsia::web::PopupFrameCreationListenerPtr popup_listener_;
  std::list<std::unique_ptr<content::WebContents>> pending_popups_;
  bool popup_ack_outstanding_ = false;
  gfx::Size render_size_override_;

  std::unique_ptr<MediaPlayerImpl> media_player_;

  fidl::Binding<fuchsia::web::Frame> binding_;
  media_control::MediaBlocker media_blocker_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_IMPL_H_
