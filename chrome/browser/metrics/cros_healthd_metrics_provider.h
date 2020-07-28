// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CROS_HEALTHD_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CROS_HEALTHD_METRICS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/metrics/metrics_provider.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class CrosHealthdMetricsProvider : public metrics::MetricsProvider {
 public:
  CrosHealthdMetricsProvider();
  CrosHealthdMetricsProvider(const CrosHealthdMetricsProvider&) = delete;
  CrosHealthdMetricsProvider& operator=(const CrosHealthdMetricsProvider&) =
      delete;
  ~CrosHealthdMetricsProvider() override;

  void AsyncInit(base::OnceClosure done_callback) override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  chromeos::cros_healthd::mojom::CrosHealthdProbeService* GetService();
  void OnDisconnect();
  void OnProbeDone(base::OnceClosure done_callback,
                   chromeos::cros_healthd::mojom::TelemetryInfoPtr ptr);

  mojo::Remote<chromeos::cros_healthd::mojom::CrosHealthdProbeService> service_;
  std::vector<metrics::SystemProfileProto::Hardware::InternalStorageDevice>
      devices_;

  base::WeakPtrFactory<CrosHealthdMetricsProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_CROS_HEALTHD_METRICS_PROVIDER_H_
