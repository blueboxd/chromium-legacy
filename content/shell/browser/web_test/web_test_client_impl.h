// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "content/shell/common/web_test/web_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace storage {
class DatabaseTracker;
class QuotaManager;
}  // namespace storage

namespace content {

// WebTestClientImpl is an implementation of WebTestClient mojo interface that
// handles the communication from the renderer process to the browser process
// using the legacy IPC. This class is bound to a RenderProcessHost when it's
// initialized and is managed by the registry of the RenderProcessHost.
class WebTestClientImpl : public mojom::WebTestClient {
 public:
  static void Create(
      int render_process_id,
      storage::QuotaManager* quota_manager,
      storage::DatabaseTracker* database_tracker,
      network::mojom::NetworkContext* network_context,
      mojo::PendingAssociatedReceiver<mojom::WebTestClient> receiver);

  WebTestClientImpl(int render_process_id,
                    storage::QuotaManager* quota_manager,
                    storage::DatabaseTracker* database_tracker,
                    network::mojom::NetworkContext* network_context);
  ~WebTestClientImpl() override;

  WebTestClientImpl(const WebTestClientImpl&) = delete;
  WebTestClientImpl& operator=(const WebTestClientImpl&) = delete;

 private:
  // WebTestClient implementation.
  void SimulateWebNotificationClick(
      const std::string& title,
      int32_t action_index,
      const base::Optional<base::string16>& reply) override;
  void SimulateWebNotificationClose(const std::string& title,
                                    bool by_user) override;
  void SimulateWebContentIndexDelete(const std::string& id) override;
  void ResetPermissions() override;
  void SetPermission(const std::string& name,
                     blink::mojom::PermissionStatus status,
                     const GURL& origin,
                     const GURL& embedding_origin) override;
  void WebTestRuntimeFlagsChanged(
      base::Value changed_web_test_runtime_flags) override;
  void DeleteAllCookies() override;
  void ClearAllDatabases() override;
  void SetDatabaseQuota(int32_t quota) override;
  void RegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& absolute_filenames,
      RegisterIsolatedFileSystemCallback callback) override;
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   base::OnceClosure callback) override;
  void ClearTrustTokenState(base::OnceClosure callback) override;

  int render_process_id_;

  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  network::mojom::NetworkContext* const network_context_;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_CLIENT_IMPL_H_
