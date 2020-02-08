// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_MATURE_EXTENSION_CHECKER_H_
#define CHROME_BROWSER_SUPERVISED_USER_MATURE_EXTENSION_CHECKER_H_

#include <map>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"

class Profile;

namespace extensions {

class WebstoreDataFetcher;

// This class queries the Webstore Item JSON Data API and keeps track of which
// extensions are marked mature and inappropriate for child users.
// Information about mature extensions is intentionally not preserved between
// sessions. We also intentionally do not retry failed requests.
class MatureExtensionChecker : public WebstoreDataFetcherDelegate {
 public:
  explicit MatureExtensionChecker(Profile* profile);
  ~MatureExtensionChecker() final;
  MatureExtensionChecker(const MatureExtensionChecker& other) = delete;
  MatureExtensionChecker& operator=(const MatureExtensionChecker& other) =
      delete;

  // Returns true if we have an outgoing request about this extension's maturity
  // rating.
  bool HasRequestForExtension(const std::string& extension_id) const;

  // Returns true if the RPC request completed. Either we received data about
  // this extension's maturity rating, or the request failed.
  bool HasDataForExtension(const std::string& extension_id) const;

  // Sends out async RPC web request for this extension's maturity rating.
  void CheckMatureDataForExtension(const std::string& extension_id);

  // Checks if this extension is mature or not. Returns false by default if the
  // request failed or no data is available.
  bool IsExtensionMature(const std::string& extension_id) const;

  // Submit a fake call to OnWebstoreResponseParseSuccess() to mark the
  // specified extension with the desired maturity rating.
  void MarkExtensionMatureForTesting(const std::string& extension_id,
                                     bool is_mature);

 private:
  friend class MatureExtensionCheckerTest;
  FRIEND_TEST_ALL_PREFIXES(MatureExtensionCheckerTest, RequestFailure);
  FRIEND_TEST_ALL_PREFIXES(MatureExtensionCheckerTest, ResponseParseFailure);

  using ExtensionRequestMap =
      std::map<std::string, std::unique_ptr<WebstoreDataFetcher>>;

  // Enables/Disables extensions upon change in maturity rating information
  // retrieved from the Webstore Item Json Data API. This function is
  // idempotent.
  void ChangeExtensionStateIfNecessary(const std::string& extension_id);

  // WebstoreDataFetcherDelegate implementation:
  void OnWebstoreRequestFailure(const std::string& extension_id) override;
  void OnWebstoreResponseParseSuccess(
      const std::string& extension_id,
      std::unique_ptr<base::DictionaryValue> webstore_data) override;
  void OnWebstoreResponseParseFailure(const std::string& extension_id,
                                      const std::string& error) override;

  // This map stores completed WebstoreDataFetcher results. Maps extension_id ->
  // boolean indicating whether the extension is rated mature or not. Failed
  // queries are recorded as false. This information is used to block mature
  // extensions for child users.
  std::map<std::string, bool> mature_extensions_map_;

  // This map stores pending WebstoreDataFetcher requests. Entries are deleted
  // after the requests complete, whether success or failure. Maps extension_id
  // -> WebstoreDataFetcher for fetching webstore JSON data. This map is useful
  // for preventing duplicate requests. Also, the WebstoreDataFetcher pointers
  // need to be stored somewhere or else the RPC requests get cancelled if the
  // pointers are deleted.
  ExtensionRequestMap pending_webstore_fetches_;

  Profile* const profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_MATURE_EXTENSION_CHECKER_H_
