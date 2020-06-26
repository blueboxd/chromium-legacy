// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_id_provider_impl.h"

#include <unordered_set>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace federated_learning {

namespace {

constexpr size_t kMinHistoryDomainSizeToReportFlocId = 1;
constexpr base::TimeDelta kFlocSessionRenewInterval =
    base::TimeDelta::FromDays(1);
constexpr int kQueryHistoryWindowInDays = 7;

}  // namespace

FlocIdProviderImpl::FlocIdProviderImpl(
    syncer::SyncService* sync_service,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    FlocRemotePermissionService* floc_remote_permission_service,
    history::HistoryService* history_service,
    syncer::UserEventService* user_event_service)
    : sync_service_(sync_service),
      cookie_settings_(std::move(cookie_settings)),
      floc_remote_permission_service_(floc_remote_permission_service),
      history_service_(history_service),
      user_event_service_(user_event_service) {
  sync_service_->AddObserver(this);
  OnStateChanged(sync_service);
}

FlocIdProviderImpl::~FlocIdProviderImpl() = default;

void FlocIdProviderImpl::NotifyFlocIdUpdated(
    EventLoggingAction event_logging_action) {
  DCHECK(floc_session_count_ > 0);

  if (event_logging_action != EventLoggingAction::kAllow ||
      !base::FeatureList::IsEnabled(features::kFlocIdComputedEventLogging)) {
    return;
  }

  auto specifics = std::make_unique<sync_pb::UserEventSpecifics>();

  sync_pb::UserEventSpecifics_FlocIdComputed* const floc_id_computed_event =
      specifics->mutable_floc_id_computed_event();

  sync_pb::UserEventSpecifics_FlocIdComputed_EventTrigger event_trigger =
      (floc_session_count_ == 1u)
          ? sync_pb::UserEventSpecifics::FlocIdComputed::NEW
          : sync_pb::UserEventSpecifics::FlocIdComputed::REFRESHED;

  floc_id_computed_event->set_event_trigger(event_trigger);
  floc_id_computed_event->set_floc_id(floc_id_.ToUint64());

  user_event_service_->RecordUserEvent(std::move(specifics));
}

bool FlocIdProviderImpl::IsSyncHistoryEnabled() {
  syncer::SyncUserSettings* setting = sync_service_->GetUserSettings();
  DCHECK(setting);

  return sync_service_->IsSyncFeatureActive() &&
         sync_service_->GetActiveDataTypes().Has(
             syncer::HISTORY_DELETE_DIRECTIVES);
}

bool FlocIdProviderImpl::AreThirdPartyCookiesAllowed() {
  return !cookie_settings_->ShouldBlockThirdPartyCookies();
}

void FlocIdProviderImpl::IsSwaaNacAccountEnabled(
    CanComputeFlocIdCallback callback) {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "floc_id_provider_impl", "floc_remote_permission_service",
          R"(
        semantics {
          description:
            "Queries google to find out if user has enabled 'web and app "
            "activity' and 'ad personalization', and if the account type is "
            "NOT a child account. Those permission bits will be checked before "
            "computing the FLoC (Federated Learning of Cohorts) ID - an "
            "anonymous similarity hash value of user’s navigation history. "
            "This ensures that the FLoC ID is derived from data that Google "
            "already owns and the user has explicitly granted permission on "
            "what they will be used for."
          trigger:
            "This request is sent at each time a FLoC (Federated Learning of "
            "Cohorts) ID is to be computed. A FLoC ID is an anonymous "
            "similarity hash value of user’s navigation history. It'll be "
            "computed at the start of each browser profile session and will be "
            "refreshed every 24 hours during that session."
          data:
            "Google credentials if user is signed in."
        }
        policy {
            setting:
              "This feature cannot be disabled in settings, but disabling sync "
              "or third-party cookies will prevent it."
        })");

  floc_remote_permission_service_->QueryFlocPermission(
      std::move(callback), partial_traffic_annotation);
}

void FlocIdProviderImpl::Shutdown() {
  if (sync_service_ && sync_service_->HasObserver(this))
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void FlocIdProviderImpl::OnStateChanged(syncer::SyncService* sync_service) {
  if (floc_session_count_ > 0)
    return;

  if (!IsSyncHistoryEnabled())
    return;

  CalculateFloc();

  floc_session_start_timer_.Start(
      FROM_HERE, kFlocSessionRenewInterval,
      base::BindRepeating(&FlocIdProviderImpl::CalculateFloc,
                          weak_ptr_factory_.GetWeakPtr()));
}

void FlocIdProviderImpl::CalculateFloc() {
  floc_session_count_ += 1;
  CheckCanComputeFlocId(
      base::BindOnce(&FlocIdProviderImpl::OnCheckCanComputeFlocIdCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FlocIdProviderImpl::CheckCanComputeFlocId(
    CanComputeFlocIdCallback callback) {
  if (!IsSyncHistoryEnabled() || !AreThirdPartyCookiesAllowed()) {
    std::move(callback).Run(false);
    return;
  }

  IsSwaaNacAccountEnabled(std::move(callback));
}

void FlocIdProviderImpl::OnCheckCanComputeFlocIdCompleted(
    bool can_compute_floc) {
  if (!can_compute_floc) {
    if (floc_id_.IsValid()) {
      floc_id_ = FlocId();
      NotifyFlocIdUpdated(EventLoggingAction::kDisallow);
    }
    return;
  }

  GetRecentlyVisitedURLs(
      base::BindOnce(&FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted,
                     weak_ptr_factory_.GetWeakPtr(), floc_session_count_));
}

void FlocIdProviderImpl::GetRecentlyVisitedURLs(
    GetRecentlyVisitedURLsCallback callback) {
  history::QueryOptions options;
  options.SetRecentDayRange(kQueryHistoryWindowInDays);
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  history_service_->QueryHistory(base::string16(), options, std::move(callback),
                                 &history_task_tracker_);
}

void FlocIdProviderImpl::OnGetRecentlyVisitedURLsCompleted(
    size_t floc_session_count,
    history::QueryResults results) {
  DCHECK_LE(floc_session_count, floc_session_count_);
  if (floc_session_count < floc_session_count_)
    return;

  std::unordered_set<std::string> domains;
  for (const history::URLResult& url_result : results) {
    if (!url_result.publicly_routable())
      continue;

    domains.insert(net::registry_controlled_domains::GetDomainAndRegistry(
        url_result.url(),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  if (domains.size() < kMinHistoryDomainSizeToReportFlocId) {
    if (floc_id_.IsValid()) {
      floc_id_ = FlocId();
      NotifyFlocIdUpdated(EventLoggingAction::kDisallow);
    }
    return;
  }

  floc_id_ = FlocId::CreateFromHistory(domains);
  NotifyFlocIdUpdated(EventLoggingAction::kAllow);
}

}  // namespace federated_learning
