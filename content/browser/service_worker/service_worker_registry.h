// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/common/content_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

// This class manages in-memory representation of service worker registrations
// (i.e., ServiceWorkerRegistration) including installing and uninstalling
// registrations. The instance of this class is owned by
// ServiceWorkerContextCore and has the same lifetime of the owner.
// The instance owns ServiceworkerStorage and uses it to store/retrieve
// registrations to/from persistent storage.
// The instance lives on the core thread.
// TODO(crbug.com/1039200): Move ServiceWorkerStorage's method and fields which
// depend on ServiceWorkerRegistration into this class.
class CONTENT_EXPORT ServiceWorkerRegistry {
 public:
  using ResourceList = ServiceWorkerStorage::ResourceList;
  using RegistrationList = ServiceWorkerStorage::RegistrationList;
  using FindRegistrationCallback =
      ServiceWorkerStorage::FindRegistrationCallback;
  using GetRegistrationsCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations)>;
  using GetRegistrationsInfosCallback = base::OnceCallback<void(
      blink::ServiceWorkerStatusCode status,
      const std::vector<ServiceWorkerRegistrationInfo>& registrations)>;
  using StatusCallback = ServiceWorkerStorage::StatusCallback;

  ServiceWorkerRegistry(
      const base::FilePath& user_data_directory,
      ServiceWorkerContextCore* context,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      storage::QuotaManagerProxy* quota_manager_proxy,
      storage::SpecialStoragePolicy* special_storage_policy);

  // For re-creating the registry from the old one. This is called when
  // something went wrong during storage access.
  ServiceWorkerRegistry(ServiceWorkerContextCore* context,
                        ServiceWorkerRegistry* old_registry);

  ~ServiceWorkerRegistry();

  ServiceWorkerStorage* storage() const { return storage_.get(); }

  // TODO(crbug.com/1039200): Move corresponding comments from
  // ServiceWorkerStorage.
  void FindRegistrationForClientUrl(const GURL& client_url,
                                    FindRegistrationCallback callback);
  void FindRegistrationForScope(const GURL& scope,
                                FindRegistrationCallback callback);
  void FindRegistrationForId(int64_t registration_id,
                             const GURL& origin,
                             FindRegistrationCallback callback);
  void FindRegistrationForIdOnly(int64_t registration_id,
                                 FindRegistrationCallback callback);

  // Returns all stored and installing registrations for a given origin.
  void GetRegistrationsForOrigin(const GURL& origin,
                                 GetRegistrationsCallback callback);

  // Returns info about all stored and initially installing registrations.
  void GetAllRegistrationsInfos(GetRegistrationsInfosCallback callback);

  ServiceWorkerRegistration* GetUninstallingRegistration(const GURL& scope);

  // Commits |registration| with the installed but not activated |version|
  // to storage, overwriting any pre-existing registration data for the scope.
  // A pre-existing version's script resources remain available if that version
  // is live. ServiceWorkerStorage::PurgeResources() should be called when it's
  // OK to delete them.
  void StoreRegistration(ServiceWorkerRegistration* registration,
                         ServiceWorkerVersion* version,
                         StatusCallback callback);

  // Deletes the registration data for |registration|. The live registration is
  // still findable via GetUninstallingRegistration(), and versions are usable
  // because their script resources have not been deleted. After calling this,
  // the caller should later:
  // - Call NotifyDoneUninstallingRegistration() to let registry know the
  //   uninstalling operation is done.
  // - If it no longer wants versions to be usable, call
  //   ServiceWorkerStorage::PurgeResources() to delete their script resources.
  // If these aren't called, on the next profile session the cleanup occurs.
  void DeleteRegistration(scoped_refptr<ServiceWorkerRegistration> registration,
                          const GURL& origin,
                          StatusCallback callback);

  // Intended for use only by ServiceWorkerRegisterJob and
  // ServiceWorkerRegistration.
  void NotifyInstallingRegistration(ServiceWorkerRegistration* registration);
  void NotifyDoneInstallingRegistration(ServiceWorkerRegistration* registration,
                                        ServiceWorkerVersion* version,
                                        blink::ServiceWorkerStatusCode status);
  void NotifyDoneUninstallingRegistration(
      ServiceWorkerRegistration* registration,
      ServiceWorkerRegistration::Status new_status);

  // TODO(crbug.com/1039200): Make this private once methods/fields related to
  // ServiceWorkerRegistration in ServiceWorkerStorage are moved into this
  // class.
  scoped_refptr<ServiceWorkerRegistration> GetOrCreateRegistration(
      const ServiceWorkerDatabase::RegistrationData& data,
      const ResourceList& resources);

  using RegistrationRefsById =
      std::map<int64_t, scoped_refptr<ServiceWorkerRegistration>>;
  // TODO(crbug.com/1039200): Remove this accessor. This is tentatively
  // exposed for ServiceWorkerStorage. Code that relies on these should be
  // moved into this class.
  RegistrationRefsById& installing_registrations() {
    return installing_registrations_;
  }

 private:
  ServiceWorkerRegistration* FindInstallingRegistrationForClientUrl(
      const GURL& client_url);
  ServiceWorkerRegistration* FindInstallingRegistrationForScope(
      const GURL& scope);
  ServiceWorkerRegistration* FindInstallingRegistrationForId(
      int64_t registration_id);

  void DidFindRegistrationForClientUrl(
      const GURL& client_url,
      int64_t trace_event_id,
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindRegistrationForScope(
      const GURL& scope,
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void DidFindRegistrationForId(
      int64_t registration_id,
      FindRegistrationCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void DidGetRegistrationsForOrigin(
      GetRegistrationsCallback callback,
      const GURL& origin_filter,
      blink::ServiceWorkerStatusCode status,
      std::unique_ptr<RegistrationList> registration_data_list,
      std::unique_ptr<std::vector<ResourceList>> resources_list);
  void DidGetAllRegistrations(
      GetRegistrationsInfosCallback callback,
      blink::ServiceWorkerStatusCode status,
      std::unique_ptr<RegistrationList> registration_data_list);

  void DidStoreRegistration(const ServiceWorkerDatabase::RegistrationData& data,
                            StatusCallback callback,
                            blink::ServiceWorkerStatusCode status);
  void DidDeleteRegistration(StatusCallback callback,
                             blink::ServiceWorkerStatusCode status);

  // The ServiceWorkerContextCore object must outlive this.
  ServiceWorkerContextCore* const context_;

  std::unique_ptr<ServiceWorkerStorage> storage_;

  // For finding registrations being installed or uninstalled.
  RegistrationRefsById installing_registrations_;
  RegistrationRefsById uninstalling_registrations_;

  base::WeakPtrFactory<ServiceWorkerRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRY_H_
