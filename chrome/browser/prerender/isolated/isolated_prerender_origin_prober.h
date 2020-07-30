// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_

#include "base/callback.h"
#include "url/gurl.h"

class AvailabilityProber;
class Profile;

// This class handles all probing and canary checks for the isolated prerender
// feature. Calling code should use |ShouldProbeOrigins| to determine if a probe
// is needed before using prefetched resources and if so, call |Probe|. See
// http://crbug.com/1109992 for more details.
class IsolatedPrerenderOriginProber {
 public:
  // Allows the url passed to |Probe| to be changed. Only used in testing.
  class ProbeURLOverrideDelegate {
   public:
    virtual GURL OverrideProbeURL(const GURL& url) = 0;
  };

  explicit IsolatedPrerenderOriginProber(Profile* profile);
  ~IsolatedPrerenderOriginProber();

  // Returns true if a probe needs to be done before using prefetched resources.
  bool ShouldProbeOrigins();

  // Sets the probe url override delegate for testing.
  void SetProbeURLOverrideDelegateOverrideForTesting(
      ProbeURLOverrideDelegate* delegate);

  // Tells whether a canary check has completed, either in success or failure.
  // Used for testing.
  bool IsCanaryCheckCompleteForTesting() const;

  // Starts a probe to |url| and calls |callback| with a bool to indicate
  // success (when true) or failure (when false).
  using OnProbeResultCallback = base::OnceCallback<void(bool)>;
  void Probe(const GURL& url, OnProbeResultCallback callback);

 private:
  void DNSProbe(const GURL& url, OnProbeResultCallback callback);
  void HTTPProbe(const GURL& url, OnProbeResultCallback callback);

  // The current profile, not owned.
  Profile* profile_;

  // Used for testing to change the url passed to |Probe|. Must outlive |this|.
  ProbeURLOverrideDelegate* override_delegate_ = nullptr;

  // The canary url checker.
  std::unique_ptr<AvailabilityProber> canary_check_;
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_
