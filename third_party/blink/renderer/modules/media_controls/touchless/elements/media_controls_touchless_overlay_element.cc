// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_overlay_element.h"

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_play_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_seek_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_volume_button_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

MediaControlsTouchlessOverlayElement::MediaControlsTouchlessOverlayElement(
    MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-touchless-overlay"));

  MediaControlsTouchlessPlayButtonElement* play_button =
      MakeGarbageCollected<MediaControlsTouchlessPlayButtonElement>(
          media_controls);

  MediaControlsTouchlessVolumeButtonElement* volume_up_button =
      MakeGarbageCollected<MediaControlsTouchlessVolumeButtonElement>(
          media_controls, true);
  MediaControlsTouchlessVolumeButtonElement* volume_down_button =
      MakeGarbageCollected<MediaControlsTouchlessVolumeButtonElement>(
          media_controls, false);

  MediaControlsTouchlessSeekButtonElement* seek_forward_button =
      MakeGarbageCollected<MediaControlsTouchlessSeekButtonElement>(
          media_controls, true);
  MediaControlsTouchlessSeekButtonElement* seek_backward_button =
      MakeGarbageCollected<MediaControlsTouchlessSeekButtonElement>(
          media_controls, false);

  ParserAppendChild(volume_up_button);
  ParserAppendChild(seek_backward_button);
  ParserAppendChild(play_button);
  ParserAppendChild(seek_forward_button);
  ParserAppendChild(volume_down_button);

  StringBuilder aria_label;
  aria_label.Append(GetLocale().QueryString(
      WebLocalizedString::kAXMediaTouchLessPlayPauseAction));
  aria_label.Append(" ");
  aria_label.Append(
      GetLocale().QueryString(WebLocalizedString::kAXMediaTouchLessSeekAction));
  aria_label.Append(" ");
  aria_label.Append(GetLocale().QueryString(
      WebLocalizedString::kAXMediaTouchLessVolumeAction));
  setAttribute(html_names::kAriaLabelAttr, aria_label.ToAtomicString());
}

void MediaControlsTouchlessOverlayElement::Trace(blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
