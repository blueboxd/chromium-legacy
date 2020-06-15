// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"

AccessContextAuditService::AccessContextAuditService() = default;
AccessContextAuditService::~AccessContextAuditService() = default;

bool AccessContextAuditService::Init(
    const base::FilePath& database_dir,
    network::mojom::CookieManager* cookie_manager) {
  database_ = base::MakeRefCounted<AccessContextAuditDatabase>(database_dir);

  // Tests may have provided a task runner already.
  if (!database_task_runner_) {
    database_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::WithBaseSyncPrimitives(),
         base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }

  if (!database_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AccessContextAuditDatabase::Init, database_)))
    return false;

  cookie_manager->AddGlobalChangeListener(
      cookie_listener_receiver_.BindNewPipeAndPassRemote());

  return true;
}

void AccessContextAuditService::RecordAccess(
    const browsing_data::LocalSharedObjectsContainer& container,
    const GURL& top_frame_origin) {
  std::vector<AccessContextAuditDatabase::AccessRecord> access_records;

  auto cookies_list = container.cookies()->GetCookieList();
  for (const auto& cookie : cookies_list) {
    access_records.emplace_back(top_frame_origin, cookie.Name(),
                                cookie.Domain(), cookie.Path(),
                                cookie.LastAccessDate());
  }

  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::AddRecords,
                                database_, std::move(access_records)));
}

void AccessContextAuditService::GetAllAccessRecords(
    AccessContextRecordsCallback callback) {
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::GetAllRecords, database_),
      std::move(callback));
}

void AccessContextAuditService::OnCookieChange(
    const net::CookieChangeInfo& change) {
  switch (change.cause) {
    case net::CookieChangeCause::INSERTED:
    case net::CookieChangeCause::OVERWRITE:
      // Ignore change causes that do not represent deletion.
      return;
    case net::CookieChangeCause::EXPLICIT:
    case net::CookieChangeCause::UNKNOWN_DELETION:
    case net::CookieChangeCause::EXPIRED:
    case net::CookieChangeCause::EVICTED:
    case net::CookieChangeCause::EXPIRED_OVERWRITE: {
      // Remove records of deleted cookie from database.
      database_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AccessContextAuditDatabase::RemoveAllRecordsForCookie,
                         database_, change.cookie.Name(),
                         change.cookie.Domain(), change.cookie.Path()));
    }
  }
}

void AccessContextAuditService::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!database_task_runner_);
  database_task_runner_ = std::move(task_runner);
}
