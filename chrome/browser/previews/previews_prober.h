// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// This class is a utility to probe a given URL with a given set of behaviors.
// This can be used for determining whether a specific network resource is
// available or accessible by Chrome.
class PreviewsProber {
 public:
  enum class HttpMethod {
    kGet,
    kHead,
  };

  PreviewsProber(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& name,
      const GURL& url,
      HttpMethod http_method);
  ~PreviewsProber();

  // Sends a probe now if the prober is currently inactive. If the probe is
  // active (i.e.: there are probes in flight), this is a no-op.
  void SendNowIfInactive();

  // Returns the successfulness of the last probe, if there was one.
  base::Optional<bool> LastProbeWasSuccessful() const;

 private:
  void CreateAndStartURLLoader();
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The name given to this prober instance, used in metrics, prefs, and traffic
  // annotations.
  const std::string name_;

  // The URL that will be probed.
  const GURL url_;

  // The HTTP method used for probing.
  const HttpMethod http_method_;

  // Whether the prober is currently sending probes.
  bool is_active_;

  // The status of the last completed probe, if any.
  base::Optional<bool> last_probe_status_;

  // Used for setting up the |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URLLoader used for the probe. Expected to be non-null iff |is_active_|.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsProber);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_
