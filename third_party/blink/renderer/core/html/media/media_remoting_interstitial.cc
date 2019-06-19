// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_remoting_interstitial.h"

#include "third_party/blink/public/platform/web_localized_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace {

constexpr base::TimeDelta kStyleChangeTransitionDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kHiddenAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kShowToastDuration = base::TimeDelta::FromSeconds(5);

}  // namespace

namespace blink {

MediaRemotingInterstitial::MediaRemotingInterstitial(
    HTMLVideoElement& videoElement)
    : HTMLDivElement(videoElement.GetDocument()),
      toggle_interstitial_timer_(
          videoElement.GetDocument().GetTaskRunner(TaskType::kInternalMedia),
          this,
          &MediaRemotingInterstitial::ToggleInterstitialTimerFired),
      video_element_(&videoElement) {
  SetShadowPseudoId(AtomicString("-internal-media-interstitial"));
  background_image_ = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  background_image_->SetShadowPseudoId(
      AtomicString("-internal-media-interstitial-background-image"));
  background_image_->setAttribute(
      html_names::kSrcAttr, videoElement.getAttribute(html_names::kPosterAttr));
  AppendChild(background_image_);

  cast_icon_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  cast_icon_->SetShadowPseudoId(
      AtomicString("-internal-media-remoting-cast-icon"));
  AppendChild(cast_icon_);

  cast_text_message_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  cast_text_message_->SetShadowPseudoId(
      AtomicString("-internal-media-interstitial-message"));
  AppendChild(cast_text_message_);

  toast_message_ = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  toast_message_->SetShadowPseudoId(
      AtomicString("-internal-media-remoting-toast-message"));
  AppendChild(toast_message_);
}

void MediaRemotingInterstitial::Show(
    const WebString& remote_device_friendly_name) {
  if (IsVisible())
    return;
  if (remote_device_friendly_name.IsEmpty()) {
    cast_text_message_->setInnerText(
        GetVideoElement().GetLocale().QueryString(
            WebLocalizedString::kMediaRemotingCastToUnknownDeviceText),
        ASSERT_NO_EXCEPTION);
  } else {
    cast_text_message_->setInnerText(
        GetVideoElement().GetLocale().QueryString(
            WebLocalizedString::kMediaRemotingCastText,
            remote_device_friendly_name),
        ASSERT_NO_EXCEPTION);
  }
  if (toggle_interstitial_timer_.IsActive())
    toggle_interstitial_timer_.Stop();
  state_ = VISIBLE;
  RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
  SetInlineStyleProperty(CSSPropertyID::kOpacity, 0,
                         CSSPrimitiveValue::UnitType::kNumber);
  toggle_interstitial_timer_.StartOneShot(kStyleChangeTransitionDuration,
                                          FROM_HERE);
}

void MediaRemotingInterstitial::Hide(WebLocalizedString::Name error_msg) {
  if (!IsVisible())
    return;
  if (toggle_interstitial_timer_.IsActive())
    toggle_interstitial_timer_.Stop();
  if (error_msg == WebLocalizedString::kMediaRemotingStopNoText) {
    state_ = HIDDEN;
  } else {
    String stop_text = GetVideoElement().GetLocale().QueryString(
        WebLocalizedString::kMediaRemotingStopText);
    if (error_msg != WebLocalizedString::kMediaRemotingStopText) {
      stop_text = GetVideoElement().GetLocale().QueryString(error_msg) + ", " +
                  stop_text;
    }
    toast_message_->setInnerText(stop_text, ASSERT_NO_EXCEPTION);
    state_ = TOAST;
  }
  SetInlineStyleProperty(CSSPropertyID::kOpacity, 0,
                         CSSPrimitiveValue::UnitType::kNumber);
  toggle_interstitial_timer_.StartOneShot(kHiddenAnimationDuration, FROM_HERE);
}

void MediaRemotingInterstitial::ToggleInterstitialTimerFired(TimerBase*) {
  toggle_interstitial_timer_.Stop();
  if (IsVisible()) {
    // Show interstitial except the |toast_message_|.
    background_image_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    cast_icon_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    cast_text_message_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    toast_message_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                           CSSValueID::kNone);
    SetInlineStyleProperty(CSSPropertyID::kBackgroundColor, CSSValueID::kBlack);
    SetInlineStyleProperty(CSSPropertyID::kOpacity, 1,
                           CSSPrimitiveValue::UnitType::kNumber);
  } else if (state_ == HIDDEN) {
    SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
    toast_message_->setInnerText(WebString(), ASSERT_NO_EXCEPTION);
  } else {
    // Show |toast_message_| only.
    toast_message_->RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
    SetInlineStyleProperty(CSSPropertyID::kBackgroundColor,
                           CSSValueID::kTransparent);
    SetInlineStyleProperty(CSSPropertyID::kOpacity, 1,
                           CSSPrimitiveValue::UnitType::kNumber);
    background_image_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                              CSSValueID::kNone);
    cast_icon_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                       CSSValueID::kNone);
    cast_text_message_->SetInlineStyleProperty(CSSPropertyID::kDisplay,
                                               CSSValueID::kNone);
    toast_message_->SetInlineStyleProperty(
        CSSPropertyID::kOpacity, 1, CSSPrimitiveValue::UnitType::kNumber);
    state_ = HIDDEN;
    toggle_interstitial_timer_.StartOneShot(kShowToastDuration, FROM_HERE);
  }
}

void MediaRemotingInterstitial::DidMoveToNewDocument(Document& old_document) {
  toggle_interstitial_timer_.MoveToNewTaskRunner(
      GetDocument().GetTaskRunner(TaskType::kInternalMedia));

  HTMLDivElement::DidMoveToNewDocument(old_document);
}

void MediaRemotingInterstitial::OnPosterImageChanged() {
  background_image_->setAttribute(
      html_names::kSrcAttr,
      GetVideoElement().getAttribute(html_names::kPosterAttr));
}

void MediaRemotingInterstitial::Trace(Visitor* visitor) {
  visitor->Trace(video_element_);
  visitor->Trace(background_image_);
  visitor->Trace(cast_icon_);
  visitor->Trace(cast_text_message_);
  visitor->Trace(toast_message_);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
