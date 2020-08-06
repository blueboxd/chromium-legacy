// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_manager_helper.h"

#include <utility>

#include "base/check.h"
#include "base/single_thread_task_runner.h"

namespace chromecast {
namespace media {

CastAudioManagerHelper::CastAudioManagerHelper(
    ::media::AudioManagerBase* audio_manager,
    base::RepeatingCallback<CmaBackendFactory*()> backend_factory_getter,
    GetSessionIdCallback get_session_id_callback,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    mojo::PendingRemote<chromecast::mojom::ServiceConnector> connector)
    : audio_manager_(audio_manager),
      backend_factory_getter_(std::move(backend_factory_getter)),
      get_session_id_callback_(std::move(get_session_id_callback)),
      media_task_runner_(std::move(media_task_runner)),
      pending_connector_(std::move(connector)) {
  DCHECK(audio_manager_);
  DCHECK(backend_factory_getter_);
  DCHECK(get_session_id_callback_);
  DCHECK(pending_connector_);
}

CastAudioManagerHelper::~CastAudioManagerHelper() = default;

CmaBackendFactory* CastAudioManagerHelper::GetCmaBackendFactory() {
  if (!cma_backend_factory_)
    cma_backend_factory_ = backend_factory_getter_.Run();
  return cma_backend_factory_;
}

std::string CastAudioManagerHelper::GetSessionId(
    const std::string& audio_group_id) {
  return get_session_id_callback_.Run(audio_group_id);
}

chromecast::mojom::ServiceConnector* CastAudioManagerHelper::GetConnector() {
  if (!connector_) {
    DCHECK(pending_connector_);
    connector_.Bind(std::move(pending_connector_));
  }
  return connector_.get();
}

}  // namespace media
}  // namespace chromecast
