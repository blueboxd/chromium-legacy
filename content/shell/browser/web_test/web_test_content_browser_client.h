// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "content/public/common/client_hints.mojom.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/web_test/fake_bluetooth_chooser.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_automation.mojom-forward.h"

namespace content {

class FakeBluetoothChooser;
class FakeBluetoothChooserFactory;
class MockClipboardHost;
class MockPlatformNotificationService;
class WebTestBrowserContext;

class WebTestContentBrowserClient : public ShellContentBrowserClient {
 public:
  // Gets the current instance.
  static WebTestContentBrowserClient* Get();

  WebTestContentBrowserClient();
  ~WebTestContentBrowserClient() override;

  WebTestBrowserContext* GetWebTestBrowserContext();
  void SetPopupBlockingEnabled(bool block_popups_);
  void ResetMockClipboardHost();

  // Retrieves the last created FakeBluetoothChooser instance.
  std::unique_ptr<FakeBluetoothChooser> GetNextFakeBluetoothChooser();

  // ContentBrowserClient overrides.
  void RenderProcessWillLaunch(RenderProcessHost* host) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) override;
  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           WebPreferences* prefs) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;
  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override;
  PlatformNotificationService* GetPlatformNotificationService(
      content::BrowserContext* browser_context) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      service_manager::BinderMapWithContext<content::RenderFrameHost*>* map)
      override;
  bool CanAcceptUntrustedExchangesIfNeeded() override;

  content::TtsControllerDelegate* GetTtsControllerDelegate() override;
  content::TtsPlatform* GetTtsPlatform() override;

  // ShellContentBrowserClient overrides.
  std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;

 private:
  // Creates and stores a FakeBluetoothChooserFactory instance.
  void CreateFakeBluetoothChooserFactory(
      mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver);
  void BindClipboardHost(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);

  void BindClientHintsControllerDelegate(
      mojo::PendingReceiver<client_hints::mojom::ClientHints> receiver);

  void BindPermissionAutomation(
      mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver);

  std::unique_ptr<MockPlatformNotificationService>
      mock_platform_notification_service_;
  bool block_popups_ = false;

  // Stores the FakeBluetoothChooserFactory that produces FakeBluetoothChoosers.
  std::unique_ptr<FakeBluetoothChooserFactory> fake_bluetooth_chooser_factory_;
  std::unique_ptr<MockClipboardHost> mock_clipboard_host_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CONTENT_BROWSER_CLIENT_H_
