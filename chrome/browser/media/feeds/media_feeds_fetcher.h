// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_FETCHER_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_FETCHER_H_

#include "base/threading/thread_checker.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/extractor.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace media_feeds {

// Fetcher object to retrieve a Media Feed schema.org object from a URL.
class MediaFeedsFetcher {
 public:
  enum class Status {
    kOk,
    kRequestFailed,
    kNotFound,
    kInvalidFeedData,
    kGone,
  };

  using MediaFeedCallback =
      base::OnceCallback<void(const schema_org::improved::mojom::EntityPtr&,
                              Status,
                              /*was_fetched_via_cache=*/bool)>;
  explicit MediaFeedsFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory);
  ~MediaFeedsFetcher();

  void FetchFeed(const GURL& url,
                 const bool bypass_cache,
                 MediaFeedCallback callback);

 private:
  // Called when fetch request completes. Parses the received media feed
  // data and dispatches them to callbacks stored in queue.
  void OnURLFetchComplete(const GURL& original_url,
                          MediaFeedCallback callback,
                          std::unique_ptr<std::string> feed_data);

  // Called when parsing and extraction of the schema.org JSON is complete.
  void OnParseComplete(MediaFeedCallback callback,
                       const bool was_fetched_via_cache,
                       schema_org::improved::mojom::EntityPtr parsed_entity);

  const scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory_;

  // Contains the current fetch request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete| or if cancelled.
  std::unique_ptr<::network::SimpleURLLoader> pending_request_;

  schema_org::Extractor extractor_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_FETCHER_H_
