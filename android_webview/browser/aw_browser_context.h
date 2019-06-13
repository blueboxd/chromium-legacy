// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <memory>
#include <vector>

#include "android_webview/browser/aw_ssl_host_state_delegate.h"
#include "android_webview/browser/net/aw_proxy_config_monitor.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"

class GURL;
class PrefService;
class PrefRegistrySimple;

namespace autofill {
class AutocompleteHistoryManager;
}

namespace content {
class PermissionControllerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
class WebContents;
}

namespace download {
class InProgressDownloadManager;
}

namespace net {
class NetLog;
}

namespace policy {
class BrowserPolicyConnectorBase;
}

namespace visitedlink {
class VisitedLinkMaster;
}

namespace safe_browsing {
class TriggerManager;
}  // namespace safe_browsing

namespace android_webview {

class AwFormDatabaseService;
class AwQuotaManagerBridge;
class AwSafeBrowsingWhitelistManager;
class AwURLRequestContextGetter;

namespace prefs {

// Used for Kerberos authentication.
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kAuthServerWhitelist[];

}  // namespace prefs

class AwBrowserContext : public content::BrowserContext,
                         public visitedlink::VisitedLinkDelegate {
 public:
  AwBrowserContext(
      const base::FilePath path,
      std::unique_ptr<PrefService> pref_service,
      std::unique_ptr<policy::BrowserPolicyConnectorBase> policy_connector);
  ~AwBrowserContext() override;

  // Currently only one instance per process is supported.
  static AwBrowserContext* GetDefault();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  // TODO(ntfschr): consider moving these into our PathService in
  // common/aw_paths.h (http://crbug.com/934184).
  static base::FilePath GetCacheDir();
  static base::FilePath GetCookieStorePath();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Get the list of authentication schemes to support.
  static std::vector<std::string> GetAuthSchemes();

  // Maps to BrowserMainParts::PreMainMessageLoopRun.
  void PreMainMessageLoopRun(net::NetLog* net_log);

  // These methods map to Add methods in visitedlink::VisitedLinkMaster.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  AwQuotaManagerBridge* GetQuotaManagerBridge();
  AwFormDatabaseService* GetFormDatabaseService();
  AwURLRequestContextGetter* GetAwURLRequestContext();
  autofill::AutocompleteHistoryManager* GetAutocompleteHistoryManager();

  // content::BrowserContext implementation.
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  download::InProgressDownloadManager* RetriveInProgressDownloadManager()
      override;

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

  AwSafeBrowsingUIManager* GetSafeBrowsingUIManager() const;
  safe_browsing::RemoteSafeBrowsingDatabaseManager* GetSafeBrowsingDBManager();
  safe_browsing::TriggerManager* GetSafeBrowsingTriggerManager() const;
  AwSafeBrowsingWhitelistManager* GetSafeBrowsingWhitelistManager() const;

  // Constructs HttpAuthDynamicParams based on |user_pref_service_|.
  network::mojom::HttpAuthDynamicParamsPtr CreateHttpAuthDynamicParams();

  PrefService* GetPrefService() const { return user_pref_service_.get(); }

 private:
  void OnAuthPrefsChanged();

  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;

  scoped_refptr<AwURLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<AwQuotaManagerBridge> quota_manager_bridge_;
  std::unique_ptr<AwFormDatabaseService> form_database_service_;
  std::unique_ptr<autofill::AutocompleteHistoryManager>
      autocomplete_history_manager_;

  std::unique_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  std::unique_ptr<content::ResourceContext> resource_context_;

  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<policy::BrowserPolicyConnectorBase> browser_policy_connector_;
  std::unique_ptr<AwSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<content::PermissionControllerDelegate> permission_manager_;
  PrefChangeRegistrar pref_change_registrar_;

  scoped_refptr<AwSafeBrowsingUIManager> safe_browsing_ui_manager_;
  std::unique_ptr<safe_browsing::TriggerManager> safe_browsing_trigger_manager_;
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;
  bool safe_browsing_db_manager_started_ = false;

  std::unique_ptr<AwSafeBrowsingWhitelistManager>
      safe_browsing_whitelist_manager_;
  SimpleFactoryKey simple_factory_key_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserContext);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
