// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

// Handles clipboard operations for X11.
// Registers to receive standard X11 events, as well as
// XFixesSetSelectionOwnerNotify.  When the remote owner changes, TARGETS and
// text/plain are preemptively fetched.  They can then be provided immediately
// to GetAvailableMimeTypes, and RequestClipboardData when mime_type is
// text/plain.  Otherwise GetAvailableMimeTypes and RequestClipboardData call
// the appropriate X11 functions and invoke callbacks when the associated events
// are received.
class X11ClipboardOzone : public PlatformClipboard, public XEventDispatcher {
 public:
  X11ClipboardOzone();
  ~X11ClipboardOzone() override;

  // PlatformClipboard:
  void OfferClipboardData(
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override;
  void RequestClipboardData(
      const std::string& mime_type,
      PlatformClipboard::DataMap* data_map,
      PlatformClipboard::RequestDataClosure callback) override;
  void GetAvailableMimeTypes(
      PlatformClipboard::GetMimeTypesClosure callback) override;
  bool IsSelectionOwner() override;
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override;

 private:
  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

  bool OnSelectionRequest(XEvent* xev);
  bool OnSelectionNotify(XEvent* xev);
  bool OnSetSelectionOwnerNotify(XEvent* xev);

  // Queries the current clipboard owner for what mime types are available by
  // sending XConvertSelection with target=TARGETS.  After sending this, we
  // will receive a SelectionNotify event with xselection.target=TARGETS which
  // is processed in |OnSelectionNotify|.
  void QueryTargets();

  // Reads the contents of the remote clipboard by sending XConvertSelection
  // with target=<mime-type>.  After sending this, we will receive a
  // SelectionNotify event with xselection.target=<mime-type> which is processed
  // in |OnSelectionNotify|.
  void ReadRemoteClipboard();

  // Notifies whenever clipboard sequence number is changed.
  PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;

  // DataMap we keep from |OfferClipboardData| to service remote requests for
  // the clipboard.
  PlatformClipboard::DataMap offer_data_map_;

  // DataMap from |RequestClipboardData| that we write remote clipboard contents
  // to before calling the completion callback.
  PlatformClipboard::DataMap* request_data_map_ = nullptr;

  // Mime types supported by remote clipboard.
  std::vector<std::string> mime_types_;

  // Data most recently read from remote clipboard.
  std::vector<unsigned char> data_;

  // Mime type of most recently read data from remote clipboard.
  std::string data_mime_type_;

  // If XFixes is unavailable, this clipboard window will not register to
  // receive events and no processing will take place.
  // TODO(joelhockey): Make clipboard work without xfixes.
  bool using_xfixes_ = false;

  // The event base returned by XFixesQueryExtension().
  int xfixes_event_base_;

  // Callbacks are stored when we haven't already prefetched the remote
  // clipboard.
  PlatformClipboard::GetMimeTypesClosure get_available_mime_types_callback_;
  PlatformClipboard::RequestDataClosure request_clipboard_data_callback_;

  // Local cache of atoms.
  const XAtom atom_clipboard_;
  const XAtom atom_targets_;
  const XAtom atom_timestamp_;

  // The property on |x_window_| which will receive remote clipboard contents.
  const XAtom x_property_;

  // Our X11 state.
  Display* const x_display_;

  // Input-only window used as a selection owner.
  const XID x_window_;

  // The time that this instance took ownership of the clipboard.
  Time acquired_selection_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(X11ClipboardOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_CLIPBOARD_OZONE_H_
