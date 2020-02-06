// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/media/audio_log_factory.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/public/mojom/log_factory_manager.mojom.h"

namespace content {

AudioServiceListener::Metrics::Metrics(const base::TickClock* clock)
    : clock_(clock), initial_downtime_start_(clock_->NowTicks()) {}

AudioServiceListener::Metrics::~Metrics() = default;

void AudioServiceListener::Metrics::ServiceAlreadyRunning() {
  LogServiceStartStatus(ServiceStartStatus::kAlreadyStarted);
  initial_downtime_start_ = base::TimeTicks();
  started_ = clock_->NowTicks();
}

void AudioServiceListener::Metrics::ServiceCreated() {
  DCHECK(created_.is_null());
  created_ = clock_->NowTicks();
}

void AudioServiceListener::Metrics::ServiceStarted() {
  started_ = clock_->NowTicks();

  // |created_| is uninitialized if OnServiceCreated() was called before the
  // listener is initialized with OnInit() call.
  if (!created_.is_null()) {
    LogServiceStartStatus(ServiceStartStatus::kSuccess);
    UMA_HISTOGRAM_TIMES("Media.AudioService.ObservedStartupTime",
                        started_ - created_);
    created_ = base::TimeTicks();
  }

  if (!initial_downtime_start_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedInitialDowntime",
                               started_ - initial_downtime_start_,
                               base::TimeDelta(), base::TimeDelta::FromDays(7),
                               50);
    initial_downtime_start_ = base::TimeTicks();
  }

  if (!stopped_.is_null()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedDowntime2",
                               started_ - stopped_, base::TimeDelta(),
                               base::TimeDelta::FromDays(7), 50);
    stopped_ = base::TimeTicks();
  }
}

void AudioServiceListener::Metrics::ServiceStopped() {
  stopped_ = clock_->NowTicks();

  DCHECK(!started_.is_null());
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioService.ObservedUptime",
                             stopped_ - started_, base::TimeDelta(),
                             base::TimeDelta::FromDays(7), 50);

  created_ = base::TimeTicks();
  started_ = base::TimeTicks();
}

void AudioServiceListener::Metrics::LogServiceStartStatus(
    Metrics::ServiceStartStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioService.ObservedStartStatus", status);
}

AudioServiceListener::AudioServiceListener()
    : metrics_(base::DefaultTickClock::GetInstance()) {
  ServiceProcessHost::AddObserver(this);
  Init(ServiceProcessHost::GetRunningProcessInfo());
}

AudioServiceListener::~AudioServiceListener() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  ServiceProcessHost::RemoveObserver(this);
}

base::ProcessId AudioServiceListener::GetProcessId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  return process_id_;
}

void AudioServiceListener::Init(
    std::vector<ServiceProcessInfo> running_service_processes) {
  for (const auto& info : running_service_processes) {
    if (info.IsService<audio::mojom::AudioService>()) {
      process_id_ = info.pid;
      metrics_.ServiceAlreadyRunning();
      MaybeSetLogFactory();
      break;
    }
  }
}

void AudioServiceListener::OnServiceProcessLaunched(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  process_id_ = info.pid;
  metrics_.ServiceCreated();
  metrics_.ServiceStarted();
  MaybeSetLogFactory();
}

void AudioServiceListener::OnServiceProcessTerminatedNormally(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  metrics_.ServiceStopped();
  process_id_ = base::kNullProcessId;
  log_factory_is_set_ = false;
}

void AudioServiceListener::OnServiceProcessCrashed(
    const ServiceProcessInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!info.IsService<audio::mojom::AudioService>())
    return;

  metrics_.ServiceStopped();
  process_id_ = base::kNullProcessId;
  log_factory_is_set_ = false;
}

void AudioServiceListener::MaybeSetLogFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) ||
      log_factory_is_set_)
    return;

  mojo::PendingRemote<media::mojom::AudioLogFactory> audio_log_factory;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AudioLogFactory>(),
      audio_log_factory.InitWithNewPipeAndPassReceiver());
  mojo::Remote<audio::mojom::LogFactoryManager> log_factory_manager;
  GetAudioService().BindLogFactoryManager(
      log_factory_manager.BindNewPipeAndPassReceiver());
  log_factory_manager->SetLogFactory(std::move(audio_log_factory));
  log_factory_is_set_ = true;
}

}  // namespace content
