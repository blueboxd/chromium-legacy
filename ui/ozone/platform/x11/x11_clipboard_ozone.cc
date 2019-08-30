// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_clipboard_ozone.h"

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/stl_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

using base::Contains;

namespace ui {

namespace {

const char kChromeSelection[] = "CHROME_SELECTION";
const char kClipboard[] = "CLIPBOARD";
const char kMimeTypeUtf8[] = "text/plain;charset=utf-8";
const char kString[] = "STRING";
const char kTargets[] = "TARGETS";
const char kTimestamp[] = "TIMESTAMP";
const char kUtf8String[] = "UTF8_STRING";

// Helps to allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
void ExpandTypes(std::vector<std::string>* list) {
  bool has_mime_type_text = Contains(*list, ui::kMimeTypeText);
  bool has_string = Contains(*list, kString);
  bool has_mime_type_utf8 = Contains(*list, kMimeTypeUtf8);
  bool has_utf8_string = Contains(*list, kUtf8String);
  if (has_mime_type_text && !has_string)
    list->push_back(kString);
  if (has_string && !has_mime_type_text)
    list->push_back(ui::kMimeTypeText);
  if (has_mime_type_utf8 && !has_utf8_string)
    list->push_back(kUtf8String);
  if (has_utf8_string && !has_mime_type_utf8)
    list->push_back(kMimeTypeUtf8);
}

XID FindXEventTarget(const XEvent& xev) {
  XID target = xev.xany.window;
  if (xev.type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xev.xcookie.data)->event;
  return target;
}

}  // namespace

X11ClipboardOzone::X11ClipboardOzone()
    : atom_clipboard_(gfx::GetAtom(kClipboard)),
      atom_targets_(gfx::GetAtom(kTargets)),
      atom_timestamp_(gfx::GetAtom(kTimestamp)),
      x_property_(gfx::GetAtom(kChromeSelection)),
      x_display_(gfx::GetXDisplay()),
      x_window_(XCreateSimpleWindow(x_display_,
                                    DefaultRootWindow(x_display_),
                                    /*x=*/-100,
                                    /*y=*/-100,
                                    /*width=*/10,
                                    /*height=*/10,
                                    /*border_width=*/0,
                                    /*border=*/0,
                                    /*background=*/0)) {
  int ignored;  // xfixes_error_base.
  if (!XFixesQueryExtension(x_display_, &xfixes_event_base_, &ignored)) {
    LOG(ERROR) << "X server does not support XFixes.";
    return;
  }
  using_xfixes_ = true;

  // Register to receive standard X11 events.
  X11EventSource::GetInstance()->AddXEventDispatcher(this);

  // Register to receive XFixes notification when selection owner changes.
  XFixesSelectSelectionInput(x_display_, x_window_, atom_clipboard_,
                             XFixesSetSelectionOwnerNotifyMask);

  // Prefetch the current remote clipboard contents.
  QueryTargets();
}

X11ClipboardOzone::~X11ClipboardOzone() {
  X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

bool X11ClipboardOzone::DispatchXEvent(XEvent* xev) {
  if (FindXEventTarget(*xev) != x_window_)
    return false;

  switch (xev->type) {
    case SelectionRequest:
      return OnSelectionRequest(xev);
    case SelectionNotify:
      return OnSelectionNotify(xev);
  }

  if (using_xfixes_ &&
      xev->type == xfixes_event_base_ + XFixesSetSelectionOwnerNotify) {
    return OnSetSelectionOwnerNotify(xev);
  }

  return false;
}

// We are the clipboard owner, and a remote peer has requested either:
// TARGETS: List of mime types that we support for the clipboard.
// TIMESTAMP: Time when we took ownership of the clipboard.
// <mime-type>: Mime type to receive clipboard as.
bool X11ClipboardOzone::OnSelectionRequest(XEvent* xev) {
  // We only support selection=CLIPBOARD, and property must be set.
  if (xev->xselectionrequest.selection != atom_clipboard_ ||
      xev->xselectionrequest.property == x11::None) {
    return false;
  }

  XSelectionEvent selection_event;
  selection_event.type = SelectionNotify;
  selection_event.display = xev->xselectionrequest.display;
  selection_event.requestor = xev->xselectionrequest.requestor;
  selection_event.selection = xev->xselectionrequest.selection;
  selection_event.target = xev->xselectionrequest.target;
  selection_event.property = xev->xselectionrequest.property;
  selection_event.time = xev->xselectionrequest.time;

  selection_event.property = xev->xselectionrequest.property;

  // target=TARGETS.
  if (selection_event.target == atom_targets_) {
    std::vector<std::string> targets;
    // Add TIMESTAMP.
    targets.push_back(kTimestamp);
    for (auto& entry : offer_data_map_) {
      targets.push_back(entry.first);
    }
    // Expand types, then convert from string to atom.
    ExpandTypes(&targets);
    std::vector<XAtom> atoms;
    for (auto& entry : targets) {
      atoms.push_back(gfx::GetAtom(entry.c_str()));
    }
    XChangeProperty(
        x_display_, selection_event.requestor, selection_event.property,
        XA_ATOM, /*format=*/32, PropModeReplace,
        reinterpret_cast<unsigned char*>(atoms.data()), atoms.size());

  } else if (selection_event.target == atom_timestamp_) {
    // target=TIMESTAMP.
    XChangeProperty(
        x_display_, selection_event.requestor, selection_event.property,
        XA_INTEGER, /*format=*/32, PropModeReplace,
        reinterpret_cast<unsigned char*>(&acquired_selection_timestamp_), 1);

  } else {
    // Send clipboard data.
    char* target_name = XGetAtomName(x_display_, selection_event.target);

    std::string key = target_name;
    // Allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
    if (key == kUtf8String && !Contains(offer_data_map_, kUtf8String)) {
      key = kMimeTypeUtf8;
    } else if (key == kString && !Contains(offer_data_map_, kString)) {
      key = kMimeTypeText;
    }
    auto it = offer_data_map_.find(key);
    if (it != offer_data_map_.end()) {
      XChangeProperty(
          x_display_, selection_event.requestor, selection_event.property,
          selection_event.target, /*format=*/8, PropModeReplace,
          const_cast<unsigned char*>(it->second.data()), it->second.size());
    }
    XFree(target_name);
  }

  // Notify remote peer that clipboard has been sent.
  XSendEvent(x_display_, selection_event.requestor, /*propagate=*/x11::False,
             /*event_mask=*/0, reinterpret_cast<XEvent*>(&selection_event));
  return true;
}

// A remote peer owns the clipboard.  This event is received in response to
// our request for TARGETS (GetAvailableMimeTypes), or a specific mime type
// (RequestClipboardData).
bool X11ClipboardOzone::OnSelectionNotify(XEvent* xev) {
  // GetAvailableMimeTypes.
  if (xev->xselection.target == atom_targets_) {
    XAtom type;
    int format;
    unsigned long item_count, after;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(x_display_, x_window_, x_property_,
                           /*long_offset=*/0,
                           /*long_length=*/256 * sizeof(XAtom),
                           /*delete=*/x11::False, XA_ATOM, &type, &format,
                           &item_count, &after, &data) != x11::Success) {
      return false;
    }

    mime_types_.clear();
    base::span<XAtom> targets(reinterpret_cast<XAtom*>(data), item_count);
    for (auto target : targets) {
      char* atom_name = XGetAtomName(x_display_, target);
      if (atom_name) {
        mime_types_.push_back(atom_name);
        XFree(atom_name);
      }
    }
    XFree(data);

    // If we have a saved callback, invoke it now with expanded types, otherwise
    // guess that we will want 'text/plain' and fetch it now.
    if (get_available_mime_types_callback_) {
      std::vector<std::string> result(mime_types_);
      ExpandTypes(&result);
      std::move(get_available_mime_types_callback_).Run(std::move(result));
    } else {
      data_mime_type_ = kMimeTypeText;
      ReadRemoteClipboard();
    }

    return true;
  }

  // RequestClipboardData.
  if (xev->xselection.property == x_property_) {
    XAtom type;
    int format;
    unsigned long item_count, after;
    unsigned char* data;
    XGetWindowProperty(x_display_, x_window_, x_property_,
                       /*long_offset=*/0, /*long_length=*/~0L,
                       /*delete=*/x11::True, AnyPropertyType, &type, &format,
                       &item_count, &after, &data);
    if (type != x11::None && format == 8) {
      std::vector<unsigned char> tmp(data, data + item_count);
      data_ = tmp;
    }
    XFree(data);

    // If we have a saved callback, invoke it now, otherwise this was a prefetch
    // and we have already saved |data_| for the next call to
    // |RequestClipboardData|.
    if (request_clipboard_data_callback_) {
      request_data_map_->emplace(data_mime_type_, data_);
      std::move(request_clipboard_data_callback_).Run(data_);
    }
    return true;
  }

  return false;
}

bool X11ClipboardOzone::OnSetSelectionOwnerNotify(XEvent* xev) {
  // Reset state and fetch remote clipboard if there is a new remote owner.
  if (!IsSelectionOwner()) {
    mime_types_.clear();
    data_mime_type_.clear();
    data_.clear();
    QueryTargets();
  }
  return true;
}

void X11ClipboardOzone::QueryTargets() {
  mime_types_.clear();
  XConvertSelection(x_display_, atom_clipboard_, atom_targets_, x_property_,
                    x_window_, x11::CurrentTime);
}

void X11ClipboardOzone::ReadRemoteClipboard() {
  data_.clear();
  // Allow conversions for text/plain[;charset=utf-8] <=> [UTF8_]STRING.
  std::string target = data_mime_type_;
  if (!Contains(mime_types_, target)) {
    if (target == kMimeTypeText) {
      target = kString;
    } else if (target == kMimeTypeUtf8) {
      target = kUtf8String;
    }
  }

  XConvertSelection(x_display_, atom_clipboard_, gfx::GetAtom(target.c_str()),
                    x_property_, x_window_, x11::CurrentTime);
}

void X11ClipboardOzone::OfferClipboardData(
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  acquired_selection_timestamp_ = X11EventSource::GetInstance()->GetTimestamp();
  offer_data_map_ = data_map;
  // Only take ownership if we are using xfixes.
  // TODO(joelhockey): Make clipboard work without xfixes.
  if (using_xfixes_) {
    XSetSelectionOwner(x_display_, atom_clipboard_, x_window_,
                       acquired_selection_timestamp_);
  }
  std::move(callback).Run();
}

void X11ClipboardOzone::RequestClipboardData(
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  // If we are not using xfixes, return empty data.
  // TODO(joelhockey): Make clipboard work without xfixes.
  // If we have already prefetched the clipboard for the correct mime type,
  // then send it right away, otherwise save the callback and attempt to get the
  // requested mime type from the remote clipboard.
  if (!using_xfixes_ || (data_mime_type_ == mime_type && !data_.empty())) {
    data_map->emplace(mime_type, data_);
    std::move(callback).Run(data_);
    return;
  }
  data_mime_type_ = mime_type;
  request_data_map_ = data_map;
  DCHECK(request_clipboard_data_callback_.is_null());
  request_clipboard_data_callback_ = std::move(callback);
  ReadRemoteClipboard();
}

void X11ClipboardOzone::GetAvailableMimeTypes(
    PlatformClipboard::GetMimeTypesClosure callback) {
  // If we are not using xfixes, return empty data.
  // TODO(joelhockey): Make clipboard work without xfixes.
  // If we already have the list of supported mime types, send the expanded list
  // of types right away, otherwise save the callback and get the list of
  // TARGETS from the remote clipboard.
  if (!using_xfixes_ || !mime_types_.empty()) {
    std::vector<std::string> result(mime_types_);
    ExpandTypes(&result);
    std::move(callback).Run(std::move(result));
    return;
  }
  DCHECK(get_available_mime_types_callback_.is_null());
  get_available_mime_types_callback_ = std::move(callback);
  QueryTargets();
}

bool X11ClipboardOzone::IsSelectionOwner() {
  // If we are not using xfixes, then we are always the owner.
  // TODO(joelhockey): Make clipboard work without xfixes.
  return !using_xfixes_ ||
         XGetSelectionOwner(x_display_, atom_clipboard_) == x_window_;
}

void X11ClipboardOzone::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  update_sequence_cb_ = std::move(cb);
}

}  // namespace ui
