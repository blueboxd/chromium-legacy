// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_

#include "ui/base/x/x11_os_exchange_data_provider.h"
#include "ui/events/platform/x11/x11_event_source.h"

namespace ui {

// OSExchangeData::Provider implementation for aura on linux.
class UI_BASE_EXPORT OSExchangeDataProviderAuraX11
    : public XOSExchangeDataProvider,
      public XEventDispatcher {
 public:
  // |x_window| is the window the cursor is over, and |selection| is the set of
  // data being offered.
  OSExchangeDataProviderAuraX11(XID x_window,
                                const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  OSExchangeDataProviderAuraX11();

  ~OSExchangeDataProviderAuraX11() override;

  OSExchangeDataProviderAuraX11(const OSExchangeDataProviderAuraX11&) = delete;
  OSExchangeDataProviderAuraX11& operator=(
      const OSExchangeDataProviderAuraX11&) = delete;

  // OSExchangeData::Provider:
  std::unique_ptr<Provider> Clone() const override;
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents) override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

 private:
  friend class OSExchangeDataProviderAuraX11Test;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_AURAX11_H_
