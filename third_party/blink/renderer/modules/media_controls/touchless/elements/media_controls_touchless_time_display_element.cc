// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_time_display_element.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

MediaControlsTouchlessTimeDisplayElement::
    MediaControlsTouchlessTimeDisplayElement(
        MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls),
      current_time_(0.0),
      duration_(0.0) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-time-display"));
  UpdateTimeDisplay();
}

void MediaControlsTouchlessTimeDisplayElement::OnTimeUpdate() {
  current_time_ = MediaElement().currentTime();
  UpdateTimeDisplay();
}

void MediaControlsTouchlessTimeDisplayElement::OnSeeking() {
  current_time_ = MediaElement().currentTime();
  UpdateTimeDisplay();
}

void MediaControlsTouchlessTimeDisplayElement::OnDurationChange() {
  duration_ = MediaElement().duration();
  UpdateTimeDisplay();
}

void MediaControlsTouchlessTimeDisplayElement::Trace(blink::Visitor* visitor) {
  MediaControlsTouchlessElement::Trace(visitor);
}

void MediaControlsTouchlessTimeDisplayElement::UpdateTimeDisplay() {
  StringBuilder builder;
  builder.Append(MediaControlsSharedHelpers::FormatTime(current_time_));
  builder.Append(" / ");
  builder.Append(MediaControlsSharedHelpers::FormatTime(duration_));
  setInnerText(builder.ToAtomicString(), ASSERT_NO_EXCEPTION);

  StringBuilder aria_label;
  aria_label.Append(GetLocale().QueryString(
      IDS_AX_MEDIA_CURRENT_TIME_DISPLAY,
      MediaControlsSharedHelpers::FormatTime(current_time_)));
  aria_label.Append(" ");
  aria_label.Append(GetLocale().QueryString(
      IDS_AX_MEDIA_TIME_REMAINING_DISPLAY,
      MediaControlsSharedHelpers::FormatTime(duration_)));
  setAttribute(html_names::kAriaLabelAttr, aria_label.ToAtomicString());
}

}  // namespace blink
