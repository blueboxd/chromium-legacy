// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_contents_observer.h"

#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"

MediaHistoryContentsObserver::MediaHistoryContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  service_ = media_history::MediaHistoryKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(service_);
}

MediaHistoryContentsObserver::~MediaHistoryContentsObserver() = default;

void MediaHistoryContentsObserver::MediaWatchTimeChanged(
    const content::MediaPlayerWatchTime& watch_time) {
  service_->GetMediaHistoryStore()->SavePlayback(watch_time);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaHistoryContentsObserver)
