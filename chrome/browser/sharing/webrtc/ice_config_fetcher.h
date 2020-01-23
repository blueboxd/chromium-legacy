// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_
#define CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/services/sharing/public/mojom/webrtc.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class IceConfigFetcher {
 public:
  using IceServerCallback =
      base::OnceCallback<void(std::vector<sharing::mojom::IceServerPtr>)>;

  explicit IceConfigFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IceConfigFetcher();

  IceConfigFetcher(const IceConfigFetcher& other) = delete;
  IceConfigFetcher& operator=(const IceConfigFetcher& other) = delete;

  // TODO(himanshujaju) - Cache configs fetched from server.
  void GetIceServers(IceServerCallback callback);

 private:
  void OnIceServersResponse(IceServerCallback callback,
                            std::unique_ptr<std::string> response_body);

  std::vector<sharing::mojom::IceServerPtr> ParseIceConfigJson(
      std::string json);

  // Returns public ice servers if API fails to respond.
  static std::vector<sharing::mojom::IceServerPtr> GetDefaultIceServers();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<IceConfigFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_
