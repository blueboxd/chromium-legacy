// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
#define CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_

#include "chrome/browser/browsing_data/access_context_audit_database.h"
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
  AccessContextAuditService();
  ~AccessContextAuditService() override;

  // Initialises the Access Context Audit database in |database_dir|, and
  // attaches listeners to |cookie_manager|.
  bool Init(const base::FilePath& database_dir,
            network::mojom::CookieManager* cookie_manager);

  // Records accesses for all storage API uses in |container| against
  // |top_frame_origin|.
  void RecordAccess(const browsing_data::LocalSharedObjectsContainer& container,
                    const GURL& top_frame_origin);

  // Queries database for all access context records, which are provided via
  // |callback|.
  void GetAllAccessRecords(AccessContextRecordsCallback callback);

  // ::network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  // Override internal task runner with provided task runner. Must be called
  // before Init().
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  scoped_refptr<AccessContextAuditDatabase> database_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessContextAuditService);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ACCESS_CONTEXT_AUDIT_SERVICE_H_
