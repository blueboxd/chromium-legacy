// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_FEEDS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_FEEDS_UI_H_

#include "chrome/browser/media/feeds/media_feeds_fetcher.h"
#include "chrome/browser/media/feeds/media_feeds_service.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace media_history {
class MediaHistoryKeyedService;
}  // namespace media_history

// The UI for chrome://media-feeds.
class MediaFeedsUI : public ui::MojoWebUIController,
                     public media_feeds::mojom::MediaFeedsStore {
 public:
  explicit MediaFeedsUI(content::WebUI* web_ui);
  MediaFeedsUI(const MediaFeedsUI&) = delete;
  MediaFeedsUI& operator=(const MediaFeedsUI&) = delete;
  ~MediaFeedsUI() override;

  // Instantiates the implementor of the MediaFeedsStore mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<media_feeds::mojom::MediaFeedsStore> pending);

  // media_history::mojom::MediaHistoryStore:
  void GetMediaFeeds(GetMediaFeedsCallback callback) override;
  void GetItemsForMediaFeed(int64_t feed_id,
                            GetItemsForMediaFeedCallback callback) override;
  void FetchMediaFeed(int64_t feed_id,
                      FetchMediaFeedCallback callback) override;
  void GetDebugInformation(GetDebugInformationCallback callback) override;
  void SetSafeSearchEnabledPref(
      bool value,
      SetSafeSearchEnabledPrefCallback callback) override;

 private:
  media_history::MediaHistoryKeyedService* GetMediaHistoryService();

  media_feeds::MediaFeedsService* GetMediaFeedsService();

  Profile* GetProfile();

  mojo::ReceiverSet<media_feeds::mojom::MediaFeedsStore> receiver_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_FEEDS_UI_H_
