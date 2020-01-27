// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"

#include "skia/public/mojom/skcolor.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"

namespace blink {

void FakeLocalFrameHost::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::LocalFrameHost::Name_,
      base::BindRepeating(&FakeLocalFrameHost::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrameHost::EnterFullscreen(
    mojom::blink::FullscreenOptionsPtr options) {}

void FakeLocalFrameHost::ExitFullscreen() {}

void FakeLocalFrameHost::FullscreenStateChanged(bool is_fullscreen) {}

void FakeLocalFrameHost::RegisterProtocolHandler(const WTF::String& scheme,
                                                 const ::blink::KURL& url,
                                                 const ::WTF::String& title,
                                                 bool user_gesture) {}

void FakeLocalFrameHost::UnregisterProtocolHandler(const WTF::String& scheme,
                                                   const ::blink::KURL& url,
                                                   bool user_gesture) {}

void FakeLocalFrameHost::DidDisplayInsecureContent() {}

void FakeLocalFrameHost::DidContainInsecureFormAction() {}

void FakeLocalFrameHost::SetNeedsOcclusionTracking(bool needs_tracking) {}

void FakeLocalFrameHost::LifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {}

void FakeLocalFrameHost::EvictFromBackForwardCache() {}

void FakeLocalFrameHost::VisibilityChanged(
    mojom::blink::FrameVisibility visibility) {}

void FakeLocalFrameHost::DidChangeThemeColor(
    const base::Optional<::SkColor>& theme_color) {}

void FakeLocalFrameHost::DidFocusFrame() {}

void FakeLocalFrameHost::EnforceInsecureRequestPolicy(uint8_t policy_bitmap) {}

void FakeLocalFrameHost::EnforceInsecureNavigationsSet(
    const WTF::Vector<uint32_t>& set) {}

void FakeLocalFrameHost::DidChangeActiveSchedulerTrackedFeatures(
    uint64_t features_mask) {}

void FakeLocalFrameHost::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {}

void FakeLocalFrameHost::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {}

void FakeLocalFrameHost::ScrollRectToVisibleInParentFrame(
    const WebRect& rect_to_scroll,
    blink::mojom::blink::ScrollIntoViewParamsPtr params) {}

void FakeLocalFrameHost::BubbleLogicalScrollInParentFrame(
    blink::mojom::blink::ScrollDirection direction,
    ui::input_types::ScrollGranularity granularity) {}

void FakeLocalFrameHost::DidAccessInitialDocument() {}

void FakeLocalFrameHost::DidBlockNavigation(
    const KURL& blocked_url,
    const KURL& initiator_url,
    mojom::NavigationBlockedReason reason) {}

void FakeLocalFrameHost::DispatchLoad() {}

void FakeLocalFrameHost::RenderFallbackContentInParentProcess() {}

void FakeLocalFrameHost::ForwardResourceTimingToParent(
    mojom::blink::ResourceTimingInfoPtr timing) {}

void FakeLocalFrameHost::DidFinishDocumentLoad() {}

void FakeLocalFrameHost::RunModalAlertDialog(
    const WTF::String& alert_message,
    RunModalAlertDialogCallback callback) {
  std::move(callback).Run();
}

void FakeLocalFrameHost::RunModalConfirmDialog(
    const WTF::String& alert_message,
    RunModalConfirmDialogCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::RunModalPromptDialog(
    const WTF::String& alert_message,
    const WTF::String& default_value,
    RunModalPromptDialogCallback callback) {
  std::move(callback).Run(true, g_empty_string);
}

void FakeLocalFrameHost::RunBeforeUnloadConfirm(
    bool is_reload,
    RunBeforeUnloadConfirmCallback callback) {
  std::move(callback).Run(true);
}

void FakeLocalFrameHost::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::blink::LocalFrameHost>(
      std::move(handle)));
}

void FakeLocalFrameHost::GoToEntryAtOffset(int32_t offset,
                                           bool has_user_gesture) {}
void FakeLocalFrameHost::HandleAccessibilityFindInPageResult(
    mojom::blink::FindInPageResultAXParamsPtr params) {}

void FakeLocalFrameHost::HandleAccessibilityFindInPageTermination() {}

void FakeLocalFrameHost::DocumentOnLoadCompleted() {}

}  // namespace blink
