// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONTENTS_OBSERVER_H_

#include "base/callback_forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace media_history {
class MediaHistoryKeyedService;
}  // namespace media_history

namespace url {
class Origin;
}  // namespace url

class MediaFeedsContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MediaFeedsContentsObserver> {
 public:
  ~MediaFeedsContentsObserver() override;
  MediaFeedsContentsObserver(const MediaFeedsContentsObserver&) = delete;
  MediaFeedsContentsObserver& operator=(const MediaFeedsContentsObserver&) =
      delete;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

  void SetClosureForTest(base::RepeatingClosure callback) {
    test_closure_ = std::move(callback);
  }

 private:
  friend class content::WebContentsUserData<MediaFeedsContentsObserver>;

  explicit MediaFeedsContentsObserver(content::WebContents* web_contents);

  void DidFindMediaFeed(const url::Origin& origin,
                        const base::Optional<GURL>& url);

  media_history::MediaHistoryKeyedService* GetService();

  void ResetFeed();

  // The last origin that the web contents navigated too. Used for resetting
  // feeds based on user navigation.
  base::Optional<url::Origin> last_origin_;

  // The test closure will be called once we have checked the page for a media
  // feed.
  base::RepeatingClosure test_closure_;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> render_frame_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONTENTS_OBSERVER_H_
