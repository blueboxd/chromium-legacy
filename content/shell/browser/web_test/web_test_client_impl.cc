// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_client_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_content_browser_client.h"
#include "content/shell/browser/web_test/web_test_content_index_provider.h"
#include "content/shell/browser/web_test/web_test_permission_manager.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/test/mock_platform_notification_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

MockPlatformNotificationService* GetMockPlatformNotificationService() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = client->GetPlatformNotificationService(context);
  return static_cast<MockPlatformNotificationService*>(service);
}

WebTestContentIndexProvider* GetWebTestContentIndexProvider() {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  return static_cast<WebTestContentIndexProvider*>(
      context->GetContentIndexProvider());
}

ContentIndexContext* GetContentIndexContext(const url::Origin& origin) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
      context, origin.GetURL(), /* can_create= */ false);
  return storage_partition->GetContentIndexContext();
}

}  // namespace

// static
void WebTestClientImpl::Create(
    mojo::PendingReceiver<mojom::WebTestClient> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebTestClientImpl>(),
                              std::move(receiver));
}

void WebTestClientImpl::InspectSecondaryWindow() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnInspectSecondaryWindow();
}

void WebTestClientImpl::TestFinishedInSecondaryRenderer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OnTestFinishedInSecondaryRenderer();
}

void WebTestClientImpl::SimulateWebNotificationClose(const std::string& title,
                                                     bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetMockPlatformNotificationService()->SimulateClose(title, by_user);
}

void WebTestClientImpl::SimulateWebContentIndexDelete(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* provider = GetWebTestContentIndexProvider();

  std::pair<int64_t, url::Origin> registration_data =
      provider->GetRegistrationDataFromId(id);

  auto* context = GetContentIndexContext(registration_data.second);
  context->OnUserDeletedItem(registration_data.first, registration_data.second,
                             id);
}

void WebTestClientImpl::BlockThirdPartyCookies(bool block) {
  BlinkTestController::Get()->OnBlockThirdPartyCookies(block);
}

void WebTestClientImpl::ResetPermissions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->ResetPermissions();
}

void WebTestClientImpl::InitiateCaptureDump(bool capture_navigation_history,
                                            bool capture_pixels) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (BlinkTestController::Get()) {
    BlinkTestController::Get()->OnInitiateCaptureDump(
        capture_navigation_history, capture_pixels);
  }
}

}  // namespace content
