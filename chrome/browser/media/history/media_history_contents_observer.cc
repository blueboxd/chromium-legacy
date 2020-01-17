// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_contents_observer.h"

#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"

MediaHistoryContentsObserver::MediaHistoryContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), service_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->IsOffTheRecord()) {
    service_ =
        media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile);
    DCHECK(service_);
  }

  content::MediaSession::Get(web_contents)
      ->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
}

MediaHistoryContentsObserver::~MediaHistoryContentsObserver() = default;

void MediaHistoryContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  frozen_ = true;
}

void MediaHistoryContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  MaybeCommitMediaSession();

  cached_position_.reset();
  cached_metadata_.reset();
  has_been_active_ = false;
  frozen_ = false;
  current_url_ = navigation_handle->GetURL();
}

void MediaHistoryContentsObserver::WebContentsDestroyed() {
  // The web contents is being destroyed so we might want to commit the media
  // session to the database.
  MaybeCommitMediaSession();
}

void MediaHistoryContentsObserver::MediaWatchTimeChanged(
    const content::MediaPlayerWatchTime& watch_time) {
  if (service_)
    service_->GetMediaHistoryStore()->SavePlayback(watch_time);
}

void MediaHistoryContentsObserver::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (session_info->state ==
      media_session::mojom::MediaSessionInfo::SessionState::kActive) {
    has_been_active_ = true;
  }
}

void MediaHistoryContentsObserver::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  if (!metadata.has_value() || frozen_)
    return;

  cached_metadata_ = metadata;
}

void MediaHistoryContentsObserver::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  if (!position.has_value() || frozen_)
    return;

  cached_position_ = position;
}

void MediaHistoryContentsObserver::MaybeCommitMediaSession() {
  // If the media session has never played anything or does not have any
  // metadata then we should not commit the media session.
  if (!has_been_active_ || !cached_metadata_ || cached_metadata_->IsEmpty() ||
      !service_) {
    return;
  }

  service_->GetMediaHistoryStore()->SavePlaybackSession(
      current_url_, *cached_metadata_, cached_position_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaHistoryContentsObserver)
