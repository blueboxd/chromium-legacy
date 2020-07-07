// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/access_context_audit_service.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/access_context_audit_database.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/storage_partition.h"

AccessContextAuditService::AccessContextAuditService(Profile* profile)
    : profile_(profile) {}
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
          base::BindOnce(&AccessContextAuditDatabase::Init, database_,
                         profile_->ShouldRestoreOldSessionCookies()))) {
    return false;
  }

  cookie_manager->AddGlobalChangeListener(
      cookie_listener_receiver_.BindNewPipeAndPassRemote());

  return true;
}

void AccessContextAuditService::RecordCookieAccess(
    const net::CookieList& accessed_cookies,
    const url::Origin& top_frame_origin) {
  auto now = base::Time::Now();
  std::vector<AccessContextAuditDatabase::AccessRecord> access_records;
  for (const auto& cookie : accessed_cookies) {
    // Do not record accesses to already expired cookies. This service is
    // informed of deletion via OnCookieChange.
    if (cookie.ExpiryDate() < now && cookie.IsPersistent())
      continue;

    access_records.emplace_back(top_frame_origin, cookie.Name(),
                                cookie.Domain(), cookie.Path(),
                                cookie.LastAccessDate(), cookie.IsPersistent());
  }
  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::AddRecords,
                                database_, std::move(access_records)));
}

void AccessContextAuditService::RecordStorageAPIAccess(
    const url::Origin& storage_origin,
    AccessContextAuditDatabase::StorageAPIType type,
    const url::Origin& top_frame_origin) {
  std::vector<AccessContextAuditDatabase::AccessRecord> access_record = {
      AccessContextAuditDatabase::AccessRecord(
          top_frame_origin, type, storage_origin, base::Time::Now())};
  database_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AccessContextAuditDatabase::AddRecords,
                                database_, std::move(access_record)));
}

void AccessContextAuditService::GetAllAccessRecords(
    AccessContextRecordsCallback callback) {
  database_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::GetAllRecords, database_),
      std::move(callback));
}

void AccessContextAuditService::Shutdown() {
  ClearSessionOnlyRecords();
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

void AccessContextAuditService::ClearSessionOnlyRecords() {
  ContentSettingsForOneType settings;
  HostContentSettingsMapFactory::GetForProfile(profile_)->GetSettingsForOneType(
      ContentSettingsType::COOKIES, std::string(), &settings);

  database_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AccessContextAuditDatabase::RemoveSessionOnlyRecords,
                     database_, CookieSettingsFactory::GetForProfile(profile_),
                     std::move(settings)));
}
