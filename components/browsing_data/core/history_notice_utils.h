// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_
#define COMPONENTS_BROWSING_DATA_CORE_HISTORY_NOTICE_UTILS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"

namespace history {
class WebHistoryService;
}

namespace syncer {
class SyncService;
}

namespace version_info {
enum class Channel;
}

namespace browsing_data {

// Whether history recording is enabled and can be used to provide personalized
// experience. The prior is true if all the following is true:
// - User is syncing and sync history is on.
// - Data is not encrypted with custom passphrase.
// - User has enabled 'Include Chrome browsing history and activity from
//   websites and apps that use Google services' in the |Web and App Activity|
//   of their Google Account.
// The response is returned in |callback|. The response is base::nullopt if the
// sWAA bit could not be retrieved (i.e. the history service has not been
// created yet).
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
