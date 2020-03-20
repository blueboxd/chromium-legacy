// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/inspector_media_event_handler.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"

namespace content {

namespace {

blink::WebString ToString(const base::Value& value) {
  if (value.is_string()) {
    return blink::WebString::FromUTF8(value.GetString());
  }
  std::string output_str;
  base::JSONWriter::Write(value, &output_str);
  return blink::WebString::FromUTF8(output_str);
}

}  // namespace

InspectorMediaEventHandler::InspectorMediaEventHandler(
    blink::MediaInspectorContext* inspector_context)
    : inspector_context_(inspector_context),
      player_id_(inspector_context_->CreatePlayer()) {}

// TODO(tmathmeyer) It would be wonderful if the definition for MediaLogRecord
// and InspectorPlayerEvent / InspectorPlayerProperty could be unified so that
// this method is no longer needed. Refactor MediaLogRecord at some point.
void InspectorMediaEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogRecord> events_to_send) {
  // If the video player is gone, the whole frame
  if (video_player_destroyed_)
    return;

  blink::InspectorPlayerEvents events;
  blink::InspectorPlayerProperties properties;

  for (media::MediaLogRecord event : events_to_send) {
    switch (event.type) {
      case media::MediaLogRecord::Type::kMessage: {
        for (auto&& itr : event.params.DictItems()) {
          blink::InspectorPlayerEvent ev = {
              blink::InspectorPlayerEvent::MESSAGE_EVENT, event.time,
              blink::WebString::FromUTF8(itr.first), ToString(itr.second)};
          events.emplace_back(std::move(ev));
        }
        break;
      }
      case media::MediaLogRecord::Type::kMediaPropertyChange: {
        for (auto&& itr : event.params.DictItems()) {
          blink::InspectorPlayerProperty prop = {
              blink::WebString::FromUTF8(itr.first), ToString(itr.second)};
          properties.emplace_back(std::move(prop));
        }
        break;
      }
      case media::MediaLogRecord::Type::kMediaEventTriggered: {
        blink::InspectorPlayerEvent ev = {
            blink::InspectorPlayerEvent::TRIGGERED_EVENT, event.time,
            blink::WebString::FromUTF8("event"), ToString(event.params)};
        events.emplace_back(std::move(ev));
        break;
      }
      case media::MediaLogRecord::Type::kMediaStatus: {
        // TODO(tmathmeyer) Make a new type in the browser protocol instead
        // of overloading InspectorPlayerEvent.
        blink::InspectorPlayerEvent ev = {
            blink::InspectorPlayerEvent::ERROR_EVENT, event.time,
            blink::WebString::FromUTF8("error"), ToString(event.params)};
        events.emplace_back(std::move(ev));
        break;
      }
    }
  }
  if (!events.empty())
    inspector_context_->NotifyPlayerEvents(player_id_, events);

  if (!properties.empty())
    inspector_context_->SetPlayerProperties(player_id_, properties);
}

void InspectorMediaEventHandler::OnWebMediaPlayerDestroyed() {
  video_player_destroyed_ = true;
}

}  // namespace content
