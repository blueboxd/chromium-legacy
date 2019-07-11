// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  class GoogleAppsMetricsProxyImpl extends welcome.ModuleMetricsProxyImpl {
    constructor() {
      super(
          'FirstRun.NewUserExperience.GoogleAppsInteraction',
          welcome.NuxGoogleAppsInteractions);
    }
  }

  cr.addSingletonGetter(GoogleAppsMetricsProxyImpl);

  return {
    GoogleAppsMetricsProxyImpl: GoogleAppsMetricsProxyImpl,
  };
});
