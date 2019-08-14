// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/common/media_router/mojom/media_controller.mojom.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "components/cast_channel/cast_message_util.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class Value;
}

namespace media_router {

class ActivityRecord;
class CastSession;

// Per-session object for sending media control commands to a Cast receiver, and
// notifying an observer of updates on the session's media status.
class CastMediaController : public mojom::MediaController {
 public:
  CastMediaController(ActivityRecord* activity,
                      mojom::MediaControllerRequest request,
                      mojom::MediaStatusObserverPtr observer);
  ~CastMediaController() override;

  // mojom::MediaController overrides:
  void Play() override;
  void Pause() override;
  void SetMute(bool mute) override;
  void SetVolume(float volume) override;
  void Seek(base::TimeDelta time) override;

  // These methods may notify the MediaStatusObserver that the status has been
  // updated.
  void SetSession(const CastSession& session);
  void SetMediaStatus(const base::Value& media_status);

  const std::string& sender_id() const { return sender_id_; }

 private:
  base::Value CreateMediaRequest(cast_channel::V2MessageType type);
  base::Value CreateVolumeRequest();

  void UpdateMediaStatus(const base::Value& message_value);

  const std::string sender_id_;
  ActivityRecord* const activity_;
  mojom::MediaStatus media_status_;
  std::string session_id_;
  int media_session_id_;

  mojo::Binding<mojom::MediaController> binding_;
  mojom::MediaStatusObserverPtr observer_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_CONTROLLER_H_
