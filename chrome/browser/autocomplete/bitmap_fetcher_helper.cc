// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/bitmap_fetcher_helper.h"

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("omnibox_result_change", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Chromium provides answers in the suggestion list for "
            "certain queries that user types in the omnibox. This request "
            "retrieves a small image (for example, an icon illustrating "
            "the current weather conditions) when this can add information "
            "to an answer."
          trigger:
            "Change of results for the query typed by the user in the "
            "omnibox."
          data:
            "The only data sent is the path to an image. No user data is "
            "included, although some might be inferrable (e.g. whether the "
            "weather is sunny or rainy in the user's current location) "
            "from the name of the image in the path."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "You can enable or disable this feature via 'Use a prediction "
            "service to help complete searches and URLs typed in the "
            "address bar.' in Chromium's settings under Advanced. The "
            "feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

// Calls the provided callback when the requested image is downloaded. This
// is a separate class instead of BitmapFetcherHelper implementing the observer
// because BitmapFetcherService takes ownership of its observers.
// TODO(crbug.com/1035981): Make BitmapFetcherService use the more typical
// non-owning ObserverList pattern and include its own network traffic
// annonations therefore elimninating the need for BitmapFetcherServiceObserver
// as well as BitmapFetcherHelper.
class BitmapFetcherServiceObserver : public BitmapFetcherService::Observer {
 public:
  explicit BitmapFetcherServiceObserver(
      const BitmapFetcherHelper::BitmapFetchedCallback& callback)
      : callback_(callback) {}

  // BitmapFetcherService::Observer
  void OnImageChanged(BitmapFetcherService::RequestId request_id,
                      const SkBitmap& image) override;

 private:
  const BitmapFetcherHelper::BitmapFetchedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherServiceObserver);
};

void BitmapFetcherServiceObserver::OnImageChanged(
    BitmapFetcherService::RequestId request_id,
    const SkBitmap& image) {
  DCHECK(!image.empty());
  callback_.Run(image);
}

}  // namespace

BitmapFetcherHelper::BitmapFetcherHelper(content::BrowserContext* context)
    : bitmap_fetcher_service_(
          BitmapFetcherServiceFactory::GetForBrowserContext(context)) {}

BitmapFetcherHelper::~BitmapFetcherHelper() = default;

BitmapFetcherService::RequestId BitmapFetcherHelper::RequestImage(
    const GURL& image_url,
    BitmapFetchedCallback callback) {
  if (!bitmap_fetcher_service_)
    return BitmapFetcherService::REQUEST_ID_INVALID;

  return bitmap_fetcher_service_->RequestImage(
      image_url, new BitmapFetcherServiceObserver(callback),
      traffic_annotation);
}

void BitmapFetcherHelper::CancelRequest(
    BitmapFetcherService::RequestId request) {
  if (!bitmap_fetcher_service_)
    return;

  bitmap_fetcher_service_->CancelRequest(request);
}

void BitmapFetcherHelper::PrefetchImage(const GURL& image_url) {
  if (!bitmap_fetcher_service_)
    return;

  bitmap_fetcher_service_->Prefetch(image_url, traffic_annotation);
}
