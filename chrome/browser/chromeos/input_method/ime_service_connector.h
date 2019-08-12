// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/token.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace chromeos {

namespace input_method {

// The connector of an ImeService which runs in its own process.
class ImeServiceConnector : public ime::mojom::PlatformAccessProvider {
 public:
  explicit ImeServiceConnector(Profile* profile);
  ~ImeServiceConnector() override;

  // chromeos::ime::mojom::PlatformAccessProvider overrides:
  void DownloadImeFileTo(const GURL& url,
                         const base::FilePath& file_path,
                         DownloadImeFileToCallback callback) override;

  // Launch an out-of-process IME service and grant necessary Platform access.
  void SetupImeService(
      mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver);

  void OnFileDownloadComplete(DownloadImeFileToCallback client_callback,
                              base::FilePath path);

 private:
  void OnPlatformAccessConnectionLost();

  Profile* profile_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current request in progress, or NULL.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // There is 1:1 mapping from the instance IDs to IME services running out of
  // process.
  const base::Token instance_id_;

  mojo::Receiver<chromeos::ime::mojom::PlatformAccessProvider> access_;

  mojo::Remote<chromeos::ime::mojom::PlatformAccessClient> access_client_;

  DISALLOW_COPY_AND_ASSIGN(ImeServiceConnector);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IME_SERVICE_CONNECTOR_H_
