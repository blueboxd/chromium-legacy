// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('welcome', function() {
  class NtpBackgroundMetricsProxyImpl extends welcome.ModuleMetricsProxyImpl {
    constructor() {
      super(
          'FirstRun.NewUserExperience.NtpBackgroundInteraction',
          welcome.NuxNtpBackgroundInteractions);
    }

    getInteractions() {
      return this.interactions_;
    }
  }

  cr.addSingletonGetter(NtpBackgroundMetricsProxyImpl);

  return {
    NtpBackgroundMetricsProxyImpl: NtpBackgroundMetricsProxyImpl,
  };
});
