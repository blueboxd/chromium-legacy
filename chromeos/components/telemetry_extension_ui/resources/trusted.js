// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verifies that we don't crash.
let probeService = chromeos.health.mojom.ProbeService.getRemote();

probeService
    .probeTelemetryInfo([chromeos.health.mojom.ProbeCategoryEnum.kBattery])
    .then(e => console.log(e))
    .catch(e => console.log(e));
