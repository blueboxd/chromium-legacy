// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/parent_access_view.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace views {
class Widget;
}

namespace ash {

class WindowDimmer;

enum class ParentAccessRequestReason;
enum class ParentAccessRequestViewState;

// Widget to display the Parent Access View in a standalone container.
// This widget is modal and only one instance can be created at a time. It will
// be destroyed when dismissed.
class ASH_EXPORT ParentAccessWidget {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ParentAccessWidget* widget);
    ~TestApi();

    ParentAccessView* parent_access_view();

    // Simulates that parent access code validation finished with the result
    // specified in |access_granted|, which dismisses the widget.
    void SimulateValidationFinished(bool access_granted);

   private:
    ParentAccessWidget* const parent_access_widget_;
  };

  // Creates and shows the instance of ParentAccessWidget.
  // This widget is modal and only one instance can be created at a time. It
  // will be destroyed when dismissed.
  static void Show(ParentAccessRequest request,
                   ParentAccessView::Delegate* delegate);

  // Returns the instance of ParentAccessWidget or nullptr if it does not exits.
  static ParentAccessWidget* Get();

  // Toggles showing an error state and updates displayed strings.
  void UpdateState(ParentAccessRequestViewState state,
                   const base::string16& title,
                   const base::string16& description);

  // Closes the widget.
  // |success| describes whether the validation was successful and is passed to
  // |on_parent_access_done_|.
  void Close(bool success);

 private:
  ParentAccessWidget(ParentAccessRequest request,
                     ParentAccessView::Delegate* delegate);
  ~ParentAccessWidget();

  // Shows the |widget_| and |dimmer_| if applicable.
  void Show();

  // Callback invoked when closing the widget.
  ParentAccessRequest::OnParentAccessDone on_parent_access_done_;

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<WindowDimmer> dimmer_;

  base::WeakPtrFactory<ParentAccessWidget> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessWidget);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_WIDGET_H_
