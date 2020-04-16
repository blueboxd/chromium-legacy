// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class CookiesTreeModel;
class GURL;
class Profile;

namespace browsing_data {
class CannedAppCacheHelper;
class CannedCacheStorageHelper;
class CannedCookieHelper;
class CannedDatabaseHelper;
class CannedFileSystemHelper;
class CannedIndexedDBHelper;
class CannedServiceWorkerHelper;
class CannedSharedWorkerHelper;
class CannedLocalStorageHelper;
}

class LocalSharedObjectsContainer {
 public:
  explicit LocalSharedObjectsContainer(Profile* profile);
  ~LocalSharedObjectsContainer();

  // Returns the number of objects stored in the container.
  size_t GetObjectCount() const;

  // Returns the number of objects for the given |origin|.
  size_t GetObjectCountForDomain(const GURL& origin) const;

  // Get number of unique registrable domains in the container.
  size_t GetDomainCount() const;

  // Empties the container.
  void Reset();

  // Creates a new CookiesTreeModel for all objects in the container,
  // copying each of them.
  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel() const;

  browsing_data::CannedAppCacheHelper* appcaches() const {
    return appcaches_.get();
  }
  browsing_data::CannedCookieHelper* cookies() const { return cookies_.get(); }
  browsing_data::CannedDatabaseHelper* databases() const {
    return databases_.get();
  }
  browsing_data::CannedFileSystemHelper* file_systems() const {
    return file_systems_.get();
  }
  browsing_data::CannedIndexedDBHelper* indexed_dbs() const {
    return indexed_dbs_.get();
  }
  browsing_data::CannedLocalStorageHelper* local_storages() const {
    return local_storages_.get();
  }
  browsing_data::CannedServiceWorkerHelper* service_workers() const {
    return service_workers_.get();
  }
  browsing_data::CannedSharedWorkerHelper* shared_workers() const {
    return shared_workers_.get();
  }
  browsing_data::CannedCacheStorageHelper* cache_storages() const {
    return cache_storages_.get();
  }
  browsing_data::CannedLocalStorageHelper* session_storages() const {
    return session_storages_.get();
  }

 private:
  scoped_refptr<browsing_data::CannedAppCacheHelper> appcaches_;
  scoped_refptr<browsing_data::CannedCookieHelper> cookies_;
  scoped_refptr<browsing_data::CannedDatabaseHelper> databases_;
  scoped_refptr<browsing_data::CannedFileSystemHelper> file_systems_;
  scoped_refptr<browsing_data::CannedIndexedDBHelper> indexed_dbs_;
  scoped_refptr<browsing_data::CannedLocalStorageHelper> local_storages_;
  scoped_refptr<browsing_data::CannedServiceWorkerHelper> service_workers_;
  scoped_refptr<browsing_data::CannedSharedWorkerHelper> shared_workers_;
  scoped_refptr<browsing_data::CannedCacheStorageHelper> cache_storages_;
  scoped_refptr<browsing_data::CannedLocalStorageHelper> session_storages_;

  DISALLOW_COPY_AND_ASSIGN(LocalSharedObjectsContainer);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_LOCAL_SHARED_OBJECTS_CONTAINER_H_
