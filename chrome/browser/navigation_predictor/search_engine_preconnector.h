// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_

#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {
class BrowserContext;
}  // namespace content

namespace features {
extern const base::Feature kPreconnectToSearch;
}  // namespace features

// Class to preconnect to the user's default search engine at regular intervals.
class SearchEnginePreconnector {
 public:
  explicit SearchEnginePreconnector(content::BrowserContext* browser_context);
  ~SearchEnginePreconnector();

  SearchEnginePreconnector(const SearchEnginePreconnector&) = delete;
  SearchEnginePreconnector& operator=(const SearchEnginePreconnector&) = delete;

  // Start the process of preconnecting to the default search engine.
  // |with_startup_delay| adds a delay to the preconnect, and should be true
  // only during app start up.
  void StartPreconnecting(bool with_startup_delay);

  void OnAppStateChangedForTesting(bool in_foreground);

 private:
  // Preconnects to the default search engine synchronously. Preconnects in
  // credentialed and uncredentialed mode.
  void PreconnectDSE();

  // Queries template service for the current DSE URL.
  GURL GetDefaultSearchEngineOriginURL() const;

  // Called on Android when the app moves to or from the background/foreground.
  // |in_foreground| is whether it is moving into the foreground or not.
  void OnAppStateChanged(bool in_foreground);

#if defined(OS_ANDROID)
  // Called when application state changes. e.g., application is brought to the
  // background or the foreground.
  void OnApplicationStateChange(
      base::android::ApplicationState application_state);

  // Used to listen to the changes in the application state changes. e.g., when
  // the application is brought to the background or the foreground.
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
#endif  // defined(OS_ANDROID)

  // Used to get keyed services.
  content::BrowserContext* const browser_context_;

  // Used to preconnect regularly.
  base::OneShotTimer timer_;

  // Always true on desktop, on Android only true when the app is the foreground
  // app.
  bool currently_in_foreground_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_SEARCH_ENGINE_PRECONNECTOR_H_
