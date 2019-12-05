// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace content {

class ServiceWorkerContextCoreObserver;
class ServiceWorkerContextWrapper;
class ServiceWorkerJobCoordinator;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class URLLoaderFactoryGetter;

// This class manages data associated with service workers.
// The class is single threaded and should only be used on the IO thread.
// In chromium, there is one instance per storagepartition. This class
// is the root of the containment hierarchy for service worker data
// associated with a particular partition.
class CONTENT_EXPORT ServiceWorkerContextCore
    : public ServiceWorkerVersion::Observer {
 public:
  using BoolCallback = base::OnceCallback<void(bool)>;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;
  using RegistrationCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t registration_id)>;
  using UpdateCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t registration_id)>;
  using UnregistrationCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;
  using ProviderByIdMap =
      std::map<int, std::unique_ptr<ServiceWorkerProviderHost>>;
  using ContainerHostByClientUUIDMap =
      std::map<std::string, ServiceWorkerContainerHost*>;

  // Directory for ServiceWorkerStorage and ServiceWorkerCacheManager.
  static const base::FilePath::CharType kServiceWorkerDirectory[];

  // Iterates over ServiceWorkerContainerHost objects in the
  // ContainerHostByClientUUIDMap.
  // Note: As ContainerHostIterator is operating on a member of
  // ServiceWorkerContextCore, users must ensure the ServiceWorkerContextCore
  // instance always outlives the ContainerHostIterator one.
  class CONTENT_EXPORT ContainerHostIterator {
   public:
    ~ContainerHostIterator();
    ServiceWorkerContainerHost* GetContainerHost();
    void Advance();
    bool IsAtEnd();

   private:
    friend class ServiceWorkerContextCore;
    using ContainerHostPredicate =
        base::RepeatingCallback<bool(ServiceWorkerContainerHost*)>;
    ContainerHostIterator(ContainerHostByClientUUIDMap* map,
                          ContainerHostPredicate predicate);
    void ForwardUntilMatchingContainerHost();

    ContainerHostByClientUUIDMap* const map_;
    ContainerHostPredicate predicate_;
    ContainerHostByClientUUIDMap::iterator container_host_iterator_;

    DISALLOW_COPY_AND_ASSIGN(ContainerHostIterator);
  };

  // This is owned by the StoragePartition, which will supply it with
  // the local path on disk. Given an empty |user_data_directory|,
  // nothing will be stored on disk. |observer_list| is created in
  // ServiceWorkerContextWrapper. When Notify() of |observer_list| is called in
  // ServiceWorkerContextCore, the methods of ServiceWorkerContextCoreObserver
  // will be called on the thread which called AddObserver() of |observer_list|.
  ServiceWorkerContextCore(
      const base::FilePath& user_data_directory,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy,
      URLLoaderFactoryGetter* url_loader_factory_getter,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          non_network_pending_loader_factory_bundle_for_update_check,
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>*
          observer_list,
      ServiceWorkerContextWrapper* wrapper);
  // TODO(https://crbug.com/877356): Remove this copy mechanism.
  ServiceWorkerContextCore(
      ServiceWorkerContextCore* old_context,
      ServiceWorkerContextWrapper* wrapper);
  ~ServiceWorkerContextCore() override;

  void OnStorageWiped();

  // ServiceWorkerVersion::Observer overrides.
  void OnRunningStateChanged(ServiceWorkerVersion* version) override;
  void OnVersionStateChanged(ServiceWorkerVersion* version) override;
  void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) override;
  void OnMainScriptHttpResponseInfoSet(ServiceWorkerVersion* version) override;
  void OnErrorReported(ServiceWorkerVersion* version,
                       const base::string16& error_message,
                       int line_number,
                       int column_number,
                       const GURL& source_url) override;
  void OnReportConsoleMessage(ServiceWorkerVersion* version,
                              blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;
  void OnControlleeAdded(ServiceWorkerVersion* version,
                         const std::string& client_uuid,
                         const ServiceWorkerClientInfo& client_info) override;
  void OnControlleeRemoved(ServiceWorkerVersion* version,
                           const std::string& client_uuid) override;
  void OnNoControllees(ServiceWorkerVersion* version) override;

  ServiceWorkerContextWrapper* wrapper() const { return wrapper_; }
  ServiceWorkerStorage* storage() { return storage_.get(); }
  ServiceWorkerProcessManager* process_manager();
  ServiceWorkerJobCoordinator* job_coordinator() {
    return job_coordinator_.get();
  }

  // The context class owns the set of ProviderHosts.
  void AddProviderHost(
      std::unique_ptr<ServiceWorkerProviderHost> provider_host);
  void RemoveProviderHost(int provider_id);

  // Returns a ContainerHost iterator for all service worker clients for the
  // |origin|. If |include_reserved_clients| is false, this only returns clients
  // that are execution ready (i.e., for windows, the document has been
  // created and for workers, the final response after redirects has been
  // delivered).
  std::unique_ptr<ContainerHostIterator> GetClientContainerHostIterator(
      const GURL& origin,
      bool include_reserved_clients);

  // Returns a ContainerHost iterator for service worker window clients for the
  // |origin|. If |include_reserved_clients| is false, this only returns clients
  // that are execution ready.
  std::unique_ptr<ContainerHostIterator> GetWindowClientContainerHostIterator(
      const GURL& origin,
      bool include_reserved_clients);

  // Runs the callback with true if there is a ContainerHost for |origin| of
  // type blink::mojom::ServiceWorkerProviderType::kForWindow which is a main
  // (top-level) frame. Reserved clients are ignored.
  // TODO(crbug.com/824858): Make this synchronously return bool when the core
  // thread is UI.
  void HasMainFrameWindowClient(const GURL& origin, BoolCallback callback);

  // Maintains a map from Client UUID to ServiceWorkerContainerHost for service
  // worker clients. |container_host| should not be for a service worker
  // execution context.
  // (Note: instead of maintaining 2 maps we might be able to uniformly use
  // UUID instead of process_id+provider_id elsewhere. For now I'm leaving
  // these as provider_id is deeply wired everywhere)
  void RegisterContainerHostByClientID(
      const std::string& client_uuid,
      ServiceWorkerContainerHost* container_host);
  void UnregisterContainerHostByClientID(const std::string& client_uuid);
  ServiceWorkerContainerHost* GetContainerHostByClientID(
      const std::string& client_uuid);

  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      RegistrationCallback callback);
  void UnregisterServiceWorker(const GURL& scope,
                               UnregistrationCallback callback);

  // Callback is called after all deletions occured. The status code is
  // blink::ServiceWorkerStatusCode::kOk if all succeed, or
  // SERVICE_WORKER_FAILED if any did not succeed.
  void DeleteForOrigin(const GURL& origin, StatusCallback callback);

  // Performs internal storage cleanup. Operations to the storage in the past
  // (e.g. deletion) are usually recorded in disk for a certain period until
  // compaction happens. This method wipes them out to ensure that the deleted
  // entries and other traces like log files are removed.
  void PerformStorageCleanup(base::OnceClosure callback);

  // Updates the service worker. If |force_bypass_cache| is true or 24 hours
  // have passed since the last update, bypasses the browser cache.
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache);
  // |callback| is called when the promise for
  // ServiceWorkerRegistration.update() would be resolved.
  void UpdateServiceWorker(ServiceWorkerRegistration* registration,
                           bool force_bypass_cache,
                           bool skip_script_comparison,
                           blink::mojom::FetchClientSettingsObjectPtr
                               outside_fetch_client_settings_object,
                           UpdateCallback callback);

  // Used in DevTools to update the service worker registrations without
  // consulting the browser cache while loading the controlled page. The
  // loading is delayed until the update completes and the new worker is
  // activated. The new worker skips the waiting state and immediately
  // becomes active after installed.
  bool force_update_on_page_load() { return force_update_on_page_load_; }
  void set_force_update_on_page_load(bool force_update_on_page_load) {
    force_update_on_page_load_ = force_update_on_page_load;
  }

  // This class maintains collections of live instances, this class
  // does not own these object or influence their lifetime.
  ServiceWorkerRegistration* GetLiveRegistration(int64_t registration_id);
  void AddLiveRegistration(ServiceWorkerRegistration* registration);
  void RemoveLiveRegistration(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerRegistration*>& GetLiveRegistrations()
      const {
    return live_registrations_;
  }
  ServiceWorkerVersion* GetLiveVersion(int64_t version_id);
  void AddLiveVersion(ServiceWorkerVersion* version);
  void RemoveLiveVersion(int64_t registration_id);
  const std::map<int64_t, ServiceWorkerVersion*>& GetLiveVersions() const {
    return live_versions_;
  }

  std::vector<ServiceWorkerRegistrationInfo> GetAllLiveRegistrationInfo();
  std::vector<ServiceWorkerVersionInfo> GetAllLiveVersionInfo();

  // ProtectVersion holds a reference to |version| until UnprotectVersion is
  // called.
  void ProtectVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void UnprotectVersion(int64_t version_id);

  void ScheduleDeleteAndStartOver() const;

  // Deletes all files on disk and restarts the system. This leaves the system
  // in a disabled state until it's done.
  void DeleteAndStartOver(StatusCallback callback);

  void ClearAllServiceWorkersForTest(base::OnceClosure callback);

  // Determines if there is a ServiceWorker registration that matches
  // |url|. See ServiceWorkerContext::CheckHasServiceWorker for more
  // details.
  void CheckHasServiceWorker(
      const GURL& url,
      const ServiceWorkerContext::CheckHasServiceWorkerCallback callback);

  void UpdateVersionFailureCount(int64_t version_id,
                                 blink::ServiceWorkerStatusCode status);
  // Returns the count of consecutive start worker failures for the given
  // version. The count resets to zero when the worker successfully starts.
  int GetVersionFailureCount(int64_t version_id);

  // Called by ServiceWorkerStorage when StoreRegistration() succeeds.
  void NotifyRegistrationStored(int64_t registration_id, const GURL& scope);

  URLLoaderFactoryGetter* loader_factory_getter() {
    return loader_factory_getter_.get();
  }

  const scoped_refptr<blink::URLLoaderFactoryBundle>&
  loader_factory_bundle_for_update_check() {
    return loader_factory_bundle_for_update_check_;
  }

  base::WeakPtr<ServiceWorkerContextCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  int GetNextEmbeddedWorkerId();

 private:
  friend class ServiceWorkerContextCoreTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextCoreTest, FailureInfo);

  typedef std::map<int64_t, ServiceWorkerRegistration*> RegistrationsMap;
  typedef std::map<int64_t, ServiceWorkerVersion*> VersionMap;

  struct FailureInfo {
    int count;
    blink::ServiceWorkerStatusCode last_failure;
  };

  void RegistrationComplete(const GURL& scope,
                            RegistrationCallback callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            ServiceWorkerRegistration* registration);

  void UpdateComplete(UpdateCallback callback,
                      blink::ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void UnregistrationComplete(const GURL& scope,
                              UnregistrationCallback callback,
                              int64_t registration_id,
                              blink::ServiceWorkerStatusCode status);

  bool IsValidRegisterRequest(const GURL& script_url,
                              const GURL& scope_url,
                              std::string* out_error) const;

  void DidGetRegistrationsForDeleteForOrigin(
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)> callback,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  void DidFindRegistrationForCheckHasServiceWorker(
      ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void OnRegistrationFinishedForCheckHasServiceWorker(
      ServiceWorkerContext::CheckHasServiceWorkerCallback callback,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // It's safe to store a raw pointer instead of a scoped_refptr to |wrapper_|
  // because the Wrapper::Shutdown call that hops threads to destroy |this| uses
  // Bind() to hold a reference to |wrapper_| until |this| is fully destroyed.
  ServiceWorkerContextWrapper* wrapper_;

  // |providers_| owns the provider hosts.
  std::unique_ptr<ProviderByIdMap> providers_;

  // |container_host_by_uuid_| contains raw pointers to container hosts for
  // service worker clients. This doesn't contain container hosts for service
  // worker execution contexts.
  std::unique_ptr<ContainerHostByClientUUIDMap> container_host_by_uuid_;

  std::unique_ptr<ServiceWorkerStorage> storage_;
  std::unique_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
  std::map<int64_t, ServiceWorkerRegistration*> live_registrations_;
  std::map<int64_t, ServiceWorkerVersion*> live_versions_;
  std::map<int64_t, scoped_refptr<ServiceWorkerVersion>> protected_versions_;

  std::map<int64_t /* version_id */, FailureInfo> failure_counts_;

  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;

  scoped_refptr<blink::URLLoaderFactoryBundle>
      loader_factory_bundle_for_update_check_;

  bool force_update_on_page_load_;
  // Set in RegisterServiceWorker(), cleared in ClearAllServiceWorkersForTest().
  // This is used to avoid unnecessary disk read operation in tests. This value
  // is false if Chrome was relaunched after service workers were registered.
  bool was_service_worker_registered_;
  using ServiceWorkerContextObserverList =
      base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>;
  const scoped_refptr<ServiceWorkerContextObserverList> observer_list_;

  int next_embedded_worker_id_ = 0;

  base::WeakPtrFactory<ServiceWorkerContextCore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CORE_H_
