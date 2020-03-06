// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/parental_controls.h"

#include <combaseapi.h>
#include <windows.h>
#include <winerror.h>
#include <wpcapi.h>
#include <wrl/client.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"

namespace {

// This singleton allows us to attempt to calculate the Platform Parental
// Controls enabled value on a worker thread before the UI thread needs the
// value. If the UI thread finishes sooner than we expect, that's no worse than
// today where we block.
class WinParentalControlsValue {
 public:
  static WinParentalControlsValue* GetInstance() {
    return base::Singleton<WinParentalControlsValue>::get();
  }

  const WinParentalControls& parental_controls() const {
    return parental_controls_;
  }

 private:
  friend struct base::DefaultSingletonTraits<WinParentalControlsValue>;

  // Histogram enum for tracking the thread that checked parental controls.
  enum class ThreadType {
    UI = 0,
    BLOCKING,
    COUNT,
  };

  WinParentalControlsValue() : parental_controls_(GetParentalControls()) {}

  ~WinParentalControlsValue() = default;

  WinParentalControlsValue(const WinParentalControlsValue&) = delete;
  WinParentalControlsValue& operator=(const WinParentalControlsValue&) = delete;

  // Returns the Windows Parental control enablements. This feature is available
  // on Windows 7 and beyond. This function should be called on a COM
  // Initialized thread and is potentially blocking.
  static WinParentalControls GetParentalControls() {
    // Since we can potentially block, make sure the thread is okay with this.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    Microsoft::WRL::ComPtr<IWindowsParentalControlsCore> parent_controls;
    HRESULT hr = ::CoCreateInstance(__uuidof(WindowsParentalControls), nullptr,
                                    CLSCTX_ALL, IID_PPV_ARGS(&parent_controls));
    if (FAILED(hr))
      return WinParentalControls();

    Microsoft::WRL::ComPtr<IWPCSettings> settings;
    hr = parent_controls->GetUserSettings(nullptr, &settings);
    if (FAILED(hr))
      return WinParentalControls();

    DWORD restrictions = 0;
    settings->GetRestrictions(&restrictions);

    WinParentalControls controls = {
        restrictions != WPCFLAG_NO_RESTRICTION /* any_restrictions */,
        (restrictions & WPCFLAG_LOGGING_REQUIRED) == WPCFLAG_LOGGING_REQUIRED
        /* logging_required */,
        (restrictions & WPCFLAG_WEB_FILTERED) == WPCFLAG_WEB_FILTERED
        /* web_filter */
    };
    return controls;
  }

  const WinParentalControls parental_controls_;
};

}  // namespace

void InitializeWinParentalControls() {
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, base::BindOnce(base::IgnoreResult(
                                &WinParentalControlsValue::GetInstance)));
}

const WinParentalControls& GetWinParentalControls() {
  return WinParentalControlsValue::GetInstance()->parental_controls();
}
