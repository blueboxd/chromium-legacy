// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_export.h"
#include "content/public/browser/appcache_service.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace net {
class URLRequestContext;
}  // namespace net

namespace storage {
class SpecialStoragePolicy;
}  // namespace storage

namespace content {
FORWARD_DECLARE_TEST(AppCacheServiceImplTest, ScheduleReinitialize);
class AppCacheBackendImpl;
class AppCacheHost;
class AppCacheQuotaClient;
class AppCachePolicy;
class AppCacheServiceImplTest;
class AppCacheStorageImplTest;
class AppCacheStorage;

// Refcounted container to manage the lifetime of the old storage instance
// during Reinitialization.
class CONTENT_EXPORT AppCacheStorageReference
    : public base::RefCounted<AppCacheStorageReference> {
 public:
  explicit AppCacheStorageReference(std::unique_ptr<AppCacheStorage> storage);

  AppCacheStorage* storage() const { return storage_.get(); }

 private:
  friend class base::RefCounted<AppCacheStorageReference>;
  ~AppCacheStorageReference();

  std::unique_ptr<AppCacheStorage> storage_;
};

// Handles operations that apply to caches across multiple renderer processes
// for a user-profile. Each instance has exclusive access to its cache_directory
// on disk.
class CONTENT_EXPORT AppCacheServiceImpl : public AppCacheService {
 public:
  using OnceCompletionCallback = base::OnceCallback<void(int)>;

  class CONTENT_EXPORT Observer {
   public:
    // Called just prior to the instance being deleted.
    virtual void OnServiceDestructionImminent(AppCacheServiceImpl* service) {}

    // An observer method to inform consumers of reinitialzation. Managing
    // the lifetime of the old storage instance is a delicate process.
    // Consumers can keep the old disabled instance alive by hanging on to the
    // ref provided.
    virtual void OnServiceReinitialized(
        AppCacheStorageReference* old_storage_ref) {}
    virtual ~Observer() {}
  };

  // If not using quota management, the proxy may be NULL.
  explicit AppCacheServiceImpl(storage::QuotaManagerProxy* quota_manager_proxy);
  ~AppCacheServiceImpl() override;

  void Initialize(const base::FilePath& cache_directory);

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // For use in catastrophic failure modes to reboot the appcache system
  // without relaunching the browser.
  void ScheduleReinitialize();

  // AppCacheService
  void GetAllAppCacheInfo(AppCacheInfoCollection* collection,
                          OnceCompletionCallback callback) override;
  void DeleteAppCachesForOrigin(const url::Origin& origin,
                                net::CompletionOnceCallback callback) override;

  // Deletes the group identified by 'manifest_url', 'callback' is
  // invoked upon completion. Upon completion, the cache group and
  // any resources within the group are no longer loadable and all
  // subresource loads for pages associated with a deleted group
  // will fail. This method always completes asynchronously.
  void DeleteAppCacheGroup(const GURL& manifest_url,
                           net::CompletionOnceCallback callback);

  // Checks the integrity of 'response_id' by reading the headers and data.
  // If it cannot be read, the cache group for 'manifest_url' is deleted.
  void CheckAppCacheResponse(const GURL& manifest_url,
                             int64_t cache_id,
                             int64_t response_id);

  // Context for use during cache updates, should only be accessed
  // on the IO thread. We do NOT add a reference to the request context,
  // it is the callers responsibility to ensure that the pointer
  // remains valid while set.
  net::URLRequestContext* request_context() const { return request_context_; }
  void set_request_context(net::URLRequestContext* context) {
    request_context_ = context;
  }

  // The appcache policy, may be null, in which case access is always allowed.
  // The service does NOT assume ownership of the policy, it is the callers
  // responsibility to ensure that the pointer remains valid while set.
  AppCachePolicy* appcache_policy() const { return appcache_policy_; }
  void set_appcache_policy(AppCachePolicy* policy) {
    appcache_policy_ = policy;
  }

  storage::SpecialStoragePolicy* special_storage_policy() const {
    return special_storage_policy_.get();
  }
  void set_special_storage_policy(storage::SpecialStoragePolicy* policy);

  storage::QuotaManagerProxy* quota_manager_proxy() const {
    return quota_manager_proxy_.get();
  }

  AppCacheQuotaClient* quota_client() const {
    return quota_client_;
  }

  // Each child process in chrome uses a distinct backend instance.
  // See chrome/browser/AppCacheDispatcherHost.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  virtual void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) const {
    auto it = backends_.find(id);
    return (it != backends_.end()) ? it->second : nullptr;
  }

  AppCacheStorage* storage() const { return storage_.get(); }

  base::WeakPtr<AppCacheServiceImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Disables the exit-time deletion of session-only data.
  void set_force_keep_session_state() { force_keep_session_state_ = true; }
  bool force_keep_session_state() const { return force_keep_session_state_; }

  // The following two functions are invoked in the network service world to
  // set/get a pointer to the URLLoaderFactoryGetter instance which is used to
  // get to the network URL loader factory.
  void set_url_loader_factory_getter(
      URLLoaderFactoryGetter* loader_factory_getter) {
    url_loader_factory_getter_ = loader_factory_getter;
  }

  URLLoaderFactoryGetter* url_loader_factory_getter() const {
    return url_loader_factory_getter_.get();
  }

  // Returns a pointer to a registered host. It retains ownership.
  AppCacheHost* GetHost(const base::UnguessableToken& host_id);
  bool EraseHost(const base::UnguessableToken& host_id);
  void RegisterHostForFrame(
      blink::mojom::AppCacheHostRequest host_request,
      blink::mojom::AppCacheFrontendPtrInfo frontend,
      const base::UnguessableToken& host_id,
      int32_t render_frame_id,
      int process_id,
      mojo::ReportBadMessageCallback bad_message_callback);

 protected:
  friend class content::AppCacheServiceImplTest;
  friend class content::AppCacheStorageImplTest;
  FRIEND_TEST_ALL_PREFIXES(content::AppCacheServiceImplTest,
      ScheduleReinitialize);

  class AsyncHelper;
  class DeleteHelper;
  class DeleteOriginHelper;
  class GetInfoHelper;
  class CheckResponseHelper;

  void Reinitialize();

  base::FilePath cache_directory_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  AppCachePolicy* appcache_policy_;
  AppCacheQuotaClient* quota_client_;
  std::unique_ptr<AppCacheStorage> storage_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  std::map<AsyncHelper*, std::unique_ptr<AsyncHelper>> pending_helpers_;
  // One 'backend' per child process.
  std::map<int, AppCacheBackendImpl*> backends_;
  // Context for use during cache updates.
  net::URLRequestContext* request_context_;
  // If true, nothing (not even session-only data) should be deleted on exit.
  bool force_keep_session_state_;
  base::Time last_reinit_time_;
  base::TimeDelta next_reinit_delay_;
  base::OneShotTimer reinit_timer_;
  base::ObserverList<Observer>::Unchecked observers_;

  // In the network service world contains the pointer to the
  // URLLoaderFactoryGetter instance which is used to get to the network
  // URL loader factory.
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;

 private:
  // TODO: Once we remove 'blink::mojom::AppCacheBackend', remove this together.
  friend class content::AppCacheBackendImpl;

  void RegisterHostInternal(
      blink::mojom::AppCacheHostRequest host_request,
      blink::mojom::AppCacheFrontendPtr frontend,
      const base::UnguessableToken& host_id,
      int32_t render_frame_id,
      int process_id,
      mojo::ReportBadMessageCallback bad_message_callback);
  // The (process id, host id) pair that identifies one AppCacheHost.
  using AppCacheHostProcessMap =
      std::map<base::UnguessableToken, std::unique_ptr<AppCacheHost>>;
  AppCacheHostProcessMap hosts_;

  base::WeakPtrFactory<AppCacheServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_SERVICE_IMPL_H_
