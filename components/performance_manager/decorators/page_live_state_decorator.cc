// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#include "components/performance_manager/decorators/decorators_utils.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {

// Private implementation of the node attached data. This keeps the complexity
// out of the header file.
class PageLiveStateDataImpl
    : public PageLiveStateDecorator::Data,
      public NodeAttachedDataImpl<PageLiveStateDataImpl> {
 public:
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};
  ~PageLiveStateDataImpl() override = default;
  PageLiveStateDataImpl(const PageLiveStateDataImpl& other) = delete;
  PageLiveStateDataImpl& operator=(const PageLiveStateDataImpl&) = delete;

  // PageLiveStateDecorator::Data:
  bool IsConnectedToUSBDevice() const override {
    return is_connected_to_usb_device_;
  }
  bool IsConnectedToBluetoothDevice() const override {
    return is_connected_to_bluetooth_device_;
  }
  bool IsCapturingVideo() const override { return is_capturing_video_; }
  bool IsCapturingAudio() const override { return is_capturing_audio_; }
  bool IsBeingMirrored() const override { return is_being_mirrored_; }
  bool IsCapturingDesktop() const override { return is_capturing_desktop_; }
  bool IsAutoDiscardable() const override { return is_auto_discardable_; }
  bool WasDiscarded() const override { return was_discarded_; }

  void set_is_connected_to_usb_device(bool is_connected_to_usb_device) {
    is_connected_to_usb_device_ = is_connected_to_usb_device;
  }
  void set_is_connected_to_bluetooth_device(
      bool is_connected_to_bluetooth_device) {
    is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
  }
  void set_is_capturing_video(bool is_capturing_video) {
    is_capturing_video_ = is_capturing_video;
  }
  void set_is_capturing_audio(bool is_capturing_audio) {
    is_capturing_audio_ = is_capturing_audio;
  }
  void set_is_being_mirrored(bool is_being_mirrored) {
    is_being_mirrored_ = is_being_mirrored;
  }
  void set_is_capturing_desktop(bool is_capturing_desktop) {
    is_capturing_desktop_ = is_capturing_desktop;
  }
  void set_is_auto_discardable(bool is_auto_discardable) {
    is_auto_discardable_ = is_auto_discardable;
  }
  void set_was_discarded(bool was_discarded) { was_discarded_ = was_discarded; }

 private:
  // Make the impl our friend so it can access the constructor and any
  // storage providers.
  friend class ::performance_manager::NodeAttachedDataImpl<
      PageLiveStateDataImpl>;

  explicit PageLiveStateDataImpl(const PageNodeImpl* page_node) {}

  bool is_connected_to_usb_device_ = false;
  bool is_connected_to_bluetooth_device_ = false;
  bool is_capturing_video_ = false;
  bool is_capturing_audio_ = false;
  bool is_being_mirrored_ = false;
  bool is_capturing_desktop_ = false;
  bool is_auto_discardable_ = true;
  bool was_discarded_ = false;
};

}  // namespace

// static
void PageLiveStateDecorator::OnIsConnectedToUSBDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_usb_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_usb_device,
      is_connected_to_usb_device);
}

// static
void PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged(
    content::WebContents* contents,
    bool is_connected_to_bluetooth_device) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_connected_to_bluetooth_device,
      is_connected_to_bluetooth_device);
}

// static
void PageLiveStateDecorator::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_video,
      is_capturing_video);
}

// static
void PageLiveStateDecorator::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_audio,
      is_capturing_audio);
}

// static
void PageLiveStateDecorator::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_being_mirrored,
      is_being_mirrored);
}

// static
void PageLiveStateDecorator::OnIsCapturingDesktopChanged(
    content::WebContents* contents,
    bool is_capturing_desktop) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_capturing_desktop,
      is_capturing_desktop);
}

// static
void PageLiveStateDecorator::SetIsAutoDiscardable(
    content::WebContents* contents,
    bool is_auto_discardable) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_is_auto_discardable,
      is_auto_discardable);
}

// static
void PageLiveStateDecorator::SetWasDiscarded(content::WebContents* contents,
                                             bool was_discarded) {
  SetPropertyForWebContentsPageNode(
      contents, &PageLiveStateDataImpl::set_was_discarded, was_discarded);
}

PageLiveStateDecorator::Data::Data() = default;
PageLiveStateDecorator::Data::~Data() = default;

const PageLiveStateDecorator::Data* PageLiveStateDecorator::Data::FromPageNode(
    const PageNode* page_node) {
  return PageLiveStateDataImpl::Get(PageNodeImpl::FromNode(page_node));
}

PageLiveStateDecorator::Data*
PageLiveStateDecorator::Data::GetOrCreateForTesting(PageNode* page_node) {
  return PageLiveStateDataImpl::GetOrCreate(PageNodeImpl::FromNode(page_node));
}

}  // namespace performance_manager
