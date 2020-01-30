// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {
class SyncService;
}

namespace version_info {
enum class Channel;
}

namespace browsing_data {

// Returns a request that can be used to query |Web and App Activity|. It can be
// made independently from the history sync state and its lifetime needs to be
// managed by the caller.
//
// Once the request is completed, |callback| is called with the following
// arguments:
//   * a pointer to the request associated with the response.
//   * a base::Optional<bool> that indicated whether the user has enabled
//     'Include Chrome browsing history and activity from websites and apps that
//     use Google services' in the |Web and App Activity| for their Google
//     Account. This argument is base::nullopt if the request to fetch the
//     |Web and App Activity| information failed.
std::unique_ptr<history::WebHistoryService::Request>
CreateQueryWebAndAppActivityRequest(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceCallback<void(history::WebHistoryService::Request*,
                            const base::Optional<bool>&)> callback);

// The response is returned in the |callback|. It can be:
// * nullopt: If we fail to query the |Web And App Activity| or history sync is
//   not fully active yet.
// * True: If the user has enabled 'Include Chrome browsing history and activity
//   from websites and apps that use Google services' in the
//   |Web and App Activity| of their Google Account, data is not encrypted with
//   custom passphrase and history sync is active.
// * False: Otherwise.
void IsHistoryRecordingEnabledAndCanBeUsed(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::OnceCallback<void(const base::Optional<bool>&)> callback);

// Whether the Clear Browsing Data UI should show a notice about the existence
// of other forms of browsing history stored in user's account. The response
// is returned in a |callback|.
void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::OnceCallback<void(bool)> callback);

// Whether the Clear Browsing Data UI should popup a dialog with information
// about the existence of other forms of browsing history stored in user's
// account when the user deletes their browsing history for the first time.
// The response is returned in a |callback|. The |channel| parameter
// must be provided for successful communication with the Sync server, but
// the result does not depend on it.
void ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    version_info::Channel channel,
    base::OnceCallback<void(bool)> callback);

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_
