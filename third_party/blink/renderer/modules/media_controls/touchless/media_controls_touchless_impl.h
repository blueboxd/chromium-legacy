// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_IMPL_H_

#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/mojom/media_controls/touchless/media_controls.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_media_event_listener_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MediaControlsOrientationLockDelegate;
class MediaControlsTouchlessBottomContainerElement;
class MediaControlsTouchlessOverlayElement;
class MediaControlsTouchlessMediaEventListener;
class MediaControlsTouchlessVolumeContainerElement;
class MediaControlsTextTrackManager;

class MODULES_EXPORT MediaControlsTouchlessImpl final
    : public HTMLDivElement,
      public MediaControls,
      public MediaControlsTouchlessMediaEventListenerObserver {
  USING_GARBAGE_COLLECTED_MIXIN(MediaControlsTouchlessImpl);

 public:
  static MediaControlsTouchlessImpl* Create(HTMLMediaElement&, ShadowRoot&);

  explicit MediaControlsTouchlessImpl(HTMLMediaElement&);

  // Node override.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  // MediaControls implementation.
  void MaybeShow() override;
  void Hide() override;
  void Reset() override {}
  void OnControlsListUpdated() override {}
  void OnTrackElementFailedToLoad() override {}
  void NetworkStateChanged() override;
  LayoutObject* PanelLayoutObject() override;
  LayoutObject* TimelineLayoutObject() override;
  LayoutObject* ButtonPanelLayoutObject() override;
  LayoutObject* ContainerLayoutObject() override;
  void ShowContextMenu() override;
  void SetTestMode(bool) override {}
  HTMLDivElement* PanelElement() override { return nullptr; }
  void OnMediaControlsEnabledChange() override {}

  // MediaControlsTouchlessMediaEventListenerObserver implementation.
  void OnFocusIn() override;
  void OnTimeUpdate() override {}
  void OnDurationChange() override {}
  void OnSeeking() override {}
  void OnLoadingProgress() override {}
  void OnPlay() override;
  void OnPause() override;
  void OnEnterFullscreen() override;
  void OnExitFullscreen() override;
  void OnError() override;
  void OnLoadedMetadata() override;
  void OnKeyPress(KeyboardEvent* event) override {}
  void OnKeyDown(KeyboardEvent* event) override;
  void OnKeyUp(KeyboardEvent* event) override {}

  MediaControlsTouchlessMediaEventListener& MediaEventListener() const;

  // Test functions
  void SetMediaControlsMenuHostForTesting(
      mojom::blink::MediaControlsMenuHostPtr);
  void MenuHostFlushForTesting();

  void Trace(blink::Visitor*) override;

 private:
  friend class MediaControlsTouchlessImplTest;

  enum class ArrowDirection;
  enum class ControlsState;
  ArrowDirection OrientArrowPress(ArrowDirection direction);
  void HandleOrientedArrowPress(ArrowDirection direction);

  WebScreenOrientationType GetOrientation();

  ControlsState State();
  void UpdateCSSFromState();

  void HandleTopButtonPress();
  void HandleBottomButtonPress();
  void HandleLeftButtonPress();
  void HandleRightButtonPress();

  void MaybeJump(int);
  void MaybeChangeVolume(double);

  void Download();

  HTMLVideoElement& VideoElement();

  // Node
  bool IsMediaControls() const override { return true; }

  void EnsureMediaControlsMenuHost();
  mojom::blink::VideoStatePtr GetVideoState();
  WTF::Vector<mojom::blink::TextTrackMetadataPtr> GetTextTracks();
  void OnMediaMenuResult(mojom::blink::MenuResponsePtr);
  void OnMediaControlsMenuHostConnectionError();

  Member<MediaControlsTouchlessOverlayElement> overlay_;
  Member<MediaControlsTouchlessBottomContainerElement> bottom_container_;
  Member<MediaControlsTouchlessVolumeContainerElement> volume_container_;

  Member<MediaControlsTouchlessMediaEventListener> media_event_listener_;
  Member<MediaControlsTextTrackManager> text_track_manager_;
  Member<MediaControlsOrientationLockDelegate> orientation_lock_delegate_;

  mojom::blink::MediaControlsMenuHostPtr media_controls_host_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsTouchlessImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_TOUCHLESS_MEDIA_CONTROLS_TOUCHLESS_IMPL_H_
