// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

#include <algorithm>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/text_track_list.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_orientation_lock_delegate.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_text_track_manager.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_bottom_container_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_overlay_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_volume_container_element.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_resource_loader.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// When specified as trackIndex, disable text tracks.
constexpr int kTrackIndexOffValue = -1;

// Number of seconds to jump when press left/right arrow.
constexpr int kNumberOfSecondsToJumpForTouchless = 10;

// Amount of volume to change when press up/down arrow.
constexpr double kVolumeToChangeForTouchless = 0.05;

const char kInlineCSSClass[] = "inline";

const char kNoSourceCSSClass[] = "state-no-source";

const char kUseDefaultPosterCSSClass[] = "use-default-poster";

}  // namespace

enum class MediaControlsTouchlessImpl::ArrowDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

enum class MediaControlsTouchlessImpl::ControlsState {
  kNoSource,
  kPreReady,
  kReady,
};

MediaControlsTouchlessImpl::MediaControlsTouchlessImpl(
    HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      overlay_(nullptr),
      bottom_container_(nullptr),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsTouchlessMediaEventListener>(
              media_element)),
      text_track_manager_(
          MakeGarbageCollected<MediaControlsTextTrackManager>(media_element)),
      orientation_lock_delegate_(nullptr) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-touchless"));
  media_event_listener_->AddObserver(this);
}

MediaControlsTouchlessImpl* MediaControlsTouchlessImpl::Create(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) {
  MediaControlsTouchlessImpl* controls =
      MakeGarbageCollected<MediaControlsTouchlessImpl>(media_element);
  controls->bottom_container_ =
      MakeGarbageCollected<MediaControlsTouchlessBottomContainerElement>(
          *controls);
  controls->overlay_ =
      MakeGarbageCollected<MediaControlsTouchlessOverlayElement>(*controls);
  controls->volume_container_ =
      MakeGarbageCollected<MediaControlsTouchlessVolumeContainerElement>(
          *controls);

  MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-info", controls);

  controls->ParserAppendChild(controls->bottom_container_);
  controls->ParserAppendChild(controls->overlay_);
  controls->ParserAppendChild(controls->volume_container_);

  // Controls start hidden.
  if (!media_element.paused())
    controls->bottom_container_->MakeTransparent();
  controls->overlay_->MakeTransparent();
  controls->volume_container_->MakeTransparent();

  if (RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled() &&
      media_element.IsHTMLVideoElement()) {
    // Initialize the orientation lock when going fullscreen feature.
    controls->orientation_lock_delegate_ =
        MakeGarbageCollected<MediaControlsOrientationLockDelegate>(
            ToHTMLVideoElement(media_element));
  }

  MediaControlsTouchlessResourceLoader::
      InjectMediaControlsTouchlessUAStyleSheet();

  if (!media_element.IsFullscreen())
    controls->classList().Add(kInlineCSSClass);

  controls->UpdateCSSFromState();

  shadow_root.ParserAppendChild(controls);
  return controls;
}

Node::InsertionNotificationRequest MediaControlsTouchlessImpl::InsertedInto(
    ContainerNode& root) {
  media_event_listener_->Attach();

  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Attach();

  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsTouchlessImpl::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);

  Hide();

  media_event_listener_->Detach();

  if (orientation_lock_delegate_)
    orientation_lock_delegate_->Detach();
}

void MediaControlsTouchlessImpl::MaybeShow() {
  RemoveInlineStyleProperty(CSSPropertyID::kDisplay);
}

void MediaControlsTouchlessImpl::Hide() {
  SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
}

void MediaControlsTouchlessImpl::NetworkStateChanged() {
  UpdateCSSFromState();
}

LayoutObject* MediaControlsTouchlessImpl::PanelLayoutObject() {
  return nullptr;
}

LayoutObject* MediaControlsTouchlessImpl::TimelineLayoutObject() {
  return bottom_container_->TimelineLayoutObject();
}

LayoutObject* MediaControlsTouchlessImpl::ButtonPanelLayoutObject() {
  return bottom_container_->TimeDisplayLayoutObject();
}

LayoutObject* MediaControlsTouchlessImpl::ContainerLayoutObject() {
  return GetLayoutObject();
}

MediaControlsTouchlessMediaEventListener&
MediaControlsTouchlessImpl::MediaEventListener() const {
  return *media_event_listener_;
}

void MediaControlsTouchlessImpl::OnFocusIn() {
  if (MediaElement().ShouldShowControls()) {
    bottom_container_->MakeOpaque(!MediaElement().paused());
    overlay_->MakeOpaque(true);
  }
}

void MediaControlsTouchlessImpl::OnPlay() {
  bottom_container_->MakeOpaque(true);
}

void MediaControlsTouchlessImpl::OnPause() {
  bottom_container_->MakeOpaque(false);
}

void MediaControlsTouchlessImpl::OnEnterFullscreen() {
  classList().Remove(kInlineCSSClass);
}

void MediaControlsTouchlessImpl::OnExitFullscreen() {
  classList().Add(kInlineCSSClass);
}

void MediaControlsTouchlessImpl::OnKeyDown(KeyboardEvent* event) {
  if (!MediaElement().ShouldShowControls())
    return;

  bool handled = true;
  switch (event->keyCode()) {
    case VKEY_RETURN:
      volume_container_->MakeTransparent(true);
      overlay_->MakeOpaque(true);
      MediaElement().TogglePlayState();
      break;
    case VKEY_LEFT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kLeft));
      break;
    case VKEY_RIGHT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kRight));
      break;
    case VKEY_UP:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kUp));
      break;
    case VKEY_DOWN:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kDown));
      break;
    default:
      handled = false;
      break;
  }

  if (handled)
    event->SetDefaultHandled();
}

void MediaControlsTouchlessImpl::EnsureMediaControlsMenuHost() {
  if (!media_controls_host_) {
    GetDocument().GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&media_controls_host_,
                          GetExecutionContext()->GetTaskRunner(
                              blink::TaskType::kMediaElementEvent)));
    media_controls_host_.set_connection_error_handler(WTF::Bind(
        &MediaControlsTouchlessImpl::OnMediaControlsMenuHostConnectionError,
        WrapWeakPersistent(this)));
  }
}

mojom::blink::VideoStatePtr MediaControlsTouchlessImpl::GetVideoState() {
  mojom::blink::VideoStatePtr video_state = mojom::blink::VideoState::New();
  video_state->is_muted = MediaElement().muted();
  video_state->is_fullscreen = MediaElement().IsFullscreen();
  return video_state;
}

WTF::Vector<mojom::blink::TextTrackMetadataPtr>
MediaControlsTouchlessImpl::GetTextTracks() {
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> text_tracks;
  TextTrackList* track_list = MediaElement().textTracks();
  for (unsigned i = 0; i < track_list->length(); i++) {
    TextTrack* track = track_list->AnonymousIndexedGetter(i);
    if (!track->CanBeRendered())
      continue;

    mojom::blink::TextTrackMetadataPtr text_track(
        mojom::blink::TextTrackMetadata::New());
    text_track->track_index = track->TrackIndex();
    text_track->label = text_track_manager_->GetTextTrackLabel(track);
    text_tracks.push_back(std::move(text_track));
  }

  if (!text_tracks.IsEmpty()) {
    mojom::blink::TextTrackMetadataPtr text_track(
        mojom::blink::TextTrackMetadata::New());
    text_track->track_index = kTrackIndexOffValue;
    text_track->label = text_track_manager_->GetTextTrackLabel(nullptr);
    text_tracks.push_front(std::move(text_track));
  }

  return text_tracks;
}

void MediaControlsTouchlessImpl::ShowContextMenu() {
  EnsureMediaControlsMenuHost();

  mojom::blink::VideoStatePtr video_state = GetVideoState();
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> text_tracks = GetTextTracks();

  WTF::Vector<mojom::blink::MenuItem> menu_items;

  if (MediaControlsSharedHelpers::ShouldShowFullscreenButton(MediaElement()))
    menu_items.push_back(mojom::blink::MenuItem::FULLSCREEN);

  if (MediaElement().HasAudio())
    menu_items.push_back(mojom::blink::MenuItem::MUTE);

  if (MediaElement().SupportsSave())
    menu_items.push_back(mojom::blink::MenuItem::DOWNLOAD);

  if (!text_tracks.IsEmpty())
    menu_items.push_back(mojom::blink::MenuItem::CAPTIONS);

  media_controls_host_->ShowMediaMenu(
      std::move(menu_items), std::move(video_state), std::move(text_tracks),
      WTF::Bind(&MediaControlsTouchlessImpl::OnMediaMenuResult,
                WrapWeakPersistent(this)));
}

void MediaControlsTouchlessImpl::OnMediaMenuResult(
    mojom::blink::MenuResponsePtr response) {
  if (response.is_null())
    return;

  switch (response->clicked) {
    case mojom::blink::MenuItem::FULLSCREEN:
      if (MediaElement().IsFullscreen())
        Fullscreen::ExitFullscreen(GetDocument());
      else
        Fullscreen::RequestFullscreen(MediaElement());
      break;
    case mojom::blink::MenuItem::MUTE:
      MediaElement().setMuted(!MediaElement().muted());
      break;
    case mojom::blink::MenuItem::DOWNLOAD:
      Download();
      break;
    case mojom::blink::MenuItem::CAPTIONS:
      text_track_manager_->DisableShowingTextTracks();
      if (response->track_index >= 0)
        text_track_manager_->ShowTextTrackAtIndex(response->track_index);
      break;
  }
}

void MediaControlsTouchlessImpl::Download() {
  const KURL& url = MediaElement().currentSrc();
  if (url.IsNull() || url.IsEmpty())
    return;
  ResourceRequest request(url);
  request.SetSuggestedFilename(MediaElement().title());
  request.SetRequestContext(mojom::RequestContextType::DOWNLOAD);
  request.SetRequestorOrigin(SecurityOrigin::Create(GetDocument().Url()));
  GetDocument().GetFrame()->Client()->DownloadURL(
      request, DownloadCrossOriginRedirects::kFollow);
}

void MediaControlsTouchlessImpl::OnMediaControlsMenuHostConnectionError() {
  media_controls_host_.reset();
}

MediaControlsTouchlessImpl::ArrowDirection
MediaControlsTouchlessImpl::OrientArrowPress(ArrowDirection direction) {
  switch (GetOrientation()) {
    case kWebScreenOrientationUndefined:
    case kWebScreenOrientationPortraitPrimary:
      return direction;
    case kWebScreenOrientationPortraitSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kDown;
        case ArrowDirection::kDown:
          return ArrowDirection::kUp;
        case ArrowDirection::kLeft:
          return ArrowDirection::kRight;
        case ArrowDirection::kRight:
          return ArrowDirection::kLeft;
      }
    case kWebScreenOrientationLandscapePrimary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kLeft;
        case ArrowDirection::kDown:
          return ArrowDirection::kRight;
        case ArrowDirection::kLeft:
          return ArrowDirection::kDown;
        case ArrowDirection::kRight:
          return ArrowDirection::kUp;
      }
    case kWebScreenOrientationLandscapeSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kRight;
        case ArrowDirection::kDown:
          return ArrowDirection::kLeft;
        case ArrowDirection::kLeft:
          return ArrowDirection::kUp;
        case ArrowDirection::kRight:
          return ArrowDirection::kDown;
      }
  }
}

void MediaControlsTouchlessImpl::HandleOrientedArrowPress(
    ArrowDirection direction) {
  switch (direction) {
    case ArrowDirection::kUp:
      HandleTopButtonPress();
      break;
    case ArrowDirection::kDown:
      HandleBottomButtonPress();
      break;
    case ArrowDirection::kLeft:
      HandleLeftButtonPress();
      break;
    case ArrowDirection::kRight:
      HandleRightButtonPress();
      break;
  }
}

WebScreenOrientationType MediaControlsTouchlessImpl::GetOrientation() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationUndefined;

  return frame->GetChromeClient().GetScreenInfo().orientation_type;
}

void MediaControlsTouchlessImpl::HandleTopButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForTouchless);
  volume_container_->UpdateVolume();
  overlay_->MakeTransparent(true);
  volume_container_->MakeOpaque(true);
}

void MediaControlsTouchlessImpl::HandleBottomButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForTouchless * -1);
  volume_container_->UpdateVolume();
  overlay_->MakeTransparent(true);
  volume_container_->MakeOpaque(true);
}

void MediaControlsTouchlessImpl::HandleLeftButtonPress() {
  if (!MediaElement().paused())
    bottom_container_->MakeOpaque(true);
  MaybeJump(kNumberOfSecondsToJumpForTouchless * -1);
}

void MediaControlsTouchlessImpl::HandleRightButtonPress() {
  if (!MediaElement().paused())
    bottom_container_->MakeOpaque(true);
  MaybeJump(kNumberOfSecondsToJumpForTouchless);
}

void MediaControlsTouchlessImpl::MaybeChangeVolume(double volume_to_change) {
  double new_volume = std::max(0.0, MediaElement().volume() + volume_to_change);
  new_volume = std::min(new_volume, 1.0);
  MediaElement().setVolume(new_volume);
}

void MediaControlsTouchlessImpl::MaybeJump(int seconds) {
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);
}

MediaControlsTouchlessImpl::ControlsState MediaControlsTouchlessImpl::State() {
  HTMLMediaElement::NetworkState network_state =
      MediaElement().getNetworkState();
  HTMLMediaElement::ReadyState ready_state = MediaElement().getReadyState();

  switch (network_state) {
    case HTMLMediaElement::kNetworkEmpty:
    case HTMLMediaElement::kNetworkNoSource:
      return ControlsState::kNoSource;
    case HTMLMediaElement::kNetworkLoading:
      if (ready_state == HTMLMediaElement::kHaveNothing)
        return ControlsState::kPreReady;
      else
        return ControlsState::kReady;
    case HTMLMediaElement::kNetworkIdle:
      if (ready_state == HTMLMediaElement::kHaveNothing)
        return ControlsState::kPreReady;
      break;
  }

  return ControlsState::kReady;
}

void MediaControlsTouchlessImpl::UpdateCSSFromState() {
  ControlsState state = State();

  if (state == ControlsState::kNoSource)
    classList().Add(kNoSourceCSSClass);
  else
    classList().Remove(kNoSourceCSSClass);

  if (!MediaElement().IsHTMLVideoElement())
    return;

  if (MediaElement().ShouldShowControls() &&
      !VideoElement().HasAvailableVideoFrame() &&
      VideoElement().PosterImageURL().IsEmpty() &&
      state <= ControlsState::kPreReady) {
    classList().Add(kUseDefaultPosterCSSClass);
  } else {
    classList().Remove(kUseDefaultPosterCSSClass);
  }
}

HTMLVideoElement& MediaControlsTouchlessImpl::VideoElement() {
  DCHECK(MediaElement().IsHTMLVideoElement());
  return *ToHTMLVideoElement(&MediaElement());
}

void MediaControlsTouchlessImpl::OnError() {
  UpdateCSSFromState();
}

void MediaControlsTouchlessImpl::OnLoadedMetadata() {
  UpdateCSSFromState();
}

void MediaControlsTouchlessImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(bottom_container_);
  visitor->Trace(overlay_);
  visitor->Trace(media_event_listener_);
  visitor->Trace(text_track_manager_);
  visitor->Trace(orientation_lock_delegate_);
  visitor->Trace(volume_container_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

void MediaControlsTouchlessImpl::SetMediaControlsMenuHostForTesting(
    mojom::blink::MediaControlsMenuHostPtr menu_host) {
  media_controls_host_ = std::move(menu_host);
}

void MediaControlsTouchlessImpl::MenuHostFlushForTesting() {
  media_controls_host_.FlushForTesting();
}

}  // namespace blink
