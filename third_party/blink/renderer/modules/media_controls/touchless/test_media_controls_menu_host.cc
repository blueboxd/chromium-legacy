// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/test_media_controls_menu_host.h"

namespace blink {

mojo::PendingRemote<mojom::blink::MediaControlsMenuHost>
TestMediaControlsMenuHost::CreateMediaControlsMenuHostRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void TestMediaControlsMenuHost::ShowMediaMenu(
    const WTF::Vector<mojom::MenuItem>& menu_items,
    mojo::PendingRemote<mojom::blink::VideoState> video_state,
    base::Optional<WTF::Vector<mojom::blink::TextTrackMetadataPtr>> text_tracks,
    ShowMediaMenuCallback callback) {
  arg_list_.menu_items = WTF::Vector<mojom::MenuItem>(menu_items);

  arg_list_.video_state = mojom::blink::VideoState::New();
  arg_list_.video_state->is_fullscreen = video_state->is_fullscreen;
  arg_list_.video_state->is_muted = video_state->is_muted;

  arg_list_.text_tracks = WTF::Vector<mojom::blink::TextTrackMetadataPtr>(
      std::move(text_tracks.value()));

  std::move(callback).Run(std::move(response_));
}

TestMenuHostArgList& TestMediaControlsMenuHost::GetMenuHostArgList() {
  return arg_list_;
}

void TestMediaControlsMenuHost::SetMenuResponse(
    mojom::blink::MenuItem menu_item,
    int track_index) {
  if (response_.is_null())
    response_ = mojom::blink::MenuResponse::New();

  response_->clicked = menu_item;
  response_->track_index = track_index;
}

}  // namespace blink
