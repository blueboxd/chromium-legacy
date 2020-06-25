// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_

#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/local_shared_objects_container.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

typedef base::OnceCallback<void(
    std::vector<AccessContextAuditDatabase::AccessRecord>)>
    AccessContextRecordsCallback;

class AccessContextAuditService
    : public KeyedService,
      public ::network::mojom::CookieChangeListener {
 public:
  explicit AccessContextAuditService(Profile* profile);
  ~AccessContextAuditService() override;

  // Initialises the Access Context Audit database in |database_dir|, and
  // attaches listeners to |cookie_manager|.
  bool Init(const base::FilePath& database_dir,
            network::mojom::CookieManager* cookie_manager);

  // Records accesses for all cookies in |details| against |top_frame_origin|.
  void RecordCookieAccess(const net::CookieList& accessed_cookies,
                          const GURL& top_frame_origin);

  // Records access for |storage_origin|'s storage of |type| against
  // |top_frame_origin|.
  void RecordStorageAPIAccess(const GURL& storage_origin,
                              AccessContextAuditDatabase::StorageAPIType type,
                              const GURL& top_frame_origin);

  // Queries database for all access context records, which are provided via
  // |callback|.
  void GetAllAccessRecords(AccessContextRecordsCallback callback);

  // KeyedService:
  void Shutdown() override;

  // ::network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  // Override internal task runner with provided task runner. Must be called
  // before Init().
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  friend class AccessContextAuditServiceTest;
  FRIEND_TEST_ALL_PREFIXES(AccessContextAuditServiceTest, SessionOnlyRecords);

  // Removes any records which are session only from the database.
  void ClearSessionOnlyRecords();

  scoped_refptr<AccessContextAuditDatabase> database_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  Profile* profile_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessContextAuditService);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
