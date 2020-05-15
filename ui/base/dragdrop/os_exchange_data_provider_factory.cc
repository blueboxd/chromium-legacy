// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"

#if defined(USE_X11)
#include "ui/base/dragdrop/os_exchange_data_provider_x11.h"
#elif defined(OS_LINUX)
#if defined(USE_OZONE)
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#endif
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#elif defined(OS_MACOSX)
#include "ui/base/dragdrop/os_exchange_data_provider_builder_mac.h"
#elif defined(OS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace ui {

//static
std::unique_ptr<OSExchangeDataProvider>
OSExchangeDataProviderFactory::CreateProvider() {
#if defined(USE_X11)
  return std::make_unique<OSExchangeDataProviderX11>();
#elif defined(OS_LINUX)
#if defined(USE_OZONE)
  // The instance can be nullptr in tests that do not instantiate the platform,
  // or on platforms that do not implement specific drag'n'drop.  For them,
  // falling back to the Aura provider should be fine.
  if (OSExchangeDataProviderFactoryOzone::Instance()) {
    auto provider =
        OSExchangeDataProviderFactoryOzone::Instance()->CreateProvider();
    if (provider)
      return provider;
  }
#endif
  return std::make_unique<OSExchangeDataProviderNonBacked>();
#elif defined(OS_MACOSX)
  return BuildOSExchangeDataProviderMac();
#elif defined(OS_WIN)
  return std::make_unique<OSExchangeDataProviderWin>();
#elif defined(OS_FUCHSIA)
  // TODO(crbug.com/980371): Implement OSExchangeDataProvider for Fuchsia.
  NOTIMPLEMENTED();
  return nullptr;
#else
#error "Unknown operating system"
#endif
}

}  // namespace ui
