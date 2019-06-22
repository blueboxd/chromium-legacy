// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {

// A class to query output protection status and/or enable output protection.
// All methods except constructor should be invoked in UI thread.
class OutputProtectionDelegate : public aura::WindowObserver,
                                 public display::DisplayObserver {
 public:
  class Controller {
   public:
    using QueryStatusCallback = base::OnceCallback<
        void(bool success, uint32_t connection_mask, uint32_t protection_mask)>;
    using SetProtectionCallback = base::OnceCallback<void(bool success)>;

    Controller();
    virtual ~Controller();
    virtual void QueryStatus(int64_t display_id,
                             QueryStatusCallback callback) = 0;
    virtual void SetProtection(int64_t display_id,
                               uint32_t protection_mask,
                               SetProtectionCallback callback) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Controller);
  };

  OutputProtectionDelegate(int render_process_id, int render_frame_id);
  ~OutputProtectionDelegate() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  void QueryStatus(Controller::QueryStatusCallback callback);
  void SetProtection(uint32_t protection_mask,
                     Controller::SetProtectionCallback callback);

 private:
  void OnWindowMayHaveMovedToAnotherDisplay();

  bool InitializeControllerIfNecessary();

  // Used to lookup the WebContents associated with the render frame.
  const int render_process_id_;
  const int render_frame_id_;

  // Native window being observed.
  aura::Window* window_ = nullptr;

  // Display ID of the observed window.
  int64_t display_id_ = display::kInvalidDisplayId;

  // Last requested ContentProtectionMethod bitmask, applied when the observed
  // window moves to another display.
  uint32_t protection_mask_ = display::CONTENT_PROTECTION_METHOD_NONE;

  // The display content protection controller.
  std::unique_ptr<Controller> controller_;

  base::WeakPtrFactory<OutputProtectionDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OutputProtectionDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_OUTPUT_PROTECTION_DELEGATE_H_
