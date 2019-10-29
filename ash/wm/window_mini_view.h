// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MINI_VIEW_H_
#define ASH_WM_WINDOW_MINI_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {
class RoundedRectView;
class WindowPreviewView;

// WindowMiniView is a view which contains a header and optionally a mirror of
// the given window. Displaying the mirror is chosen by the subclass by calling
// |SetShowPreview| in their constructors (or later on if they like).
class ASH_EXPORT WindowMiniView : public views::Button {
 public:
  ~WindowMiniView() override;

  // Sets the visiblity of |backdrop_view_|. Creates it if it is null.
  void SetBackdropVisibility(bool visible);

  // Set the title of the view, and also updates the accessibility name.
  void SetTitle(const base::string16& title);

  // Creates or deletes |preview_view_| as needed.
  void SetShowPreview(bool show);

  void UpdatePreviewRoundedCorners(bool show, float rounding);

  views::View* header_view() { return header_view_; }
  views::Label* title_label() { return title_label_; }
  RoundedRectView* backdrop_view() { return backdrop_view_; }
  WindowPreviewView* preview_view() { return preview_view_; }

 protected:
  explicit WindowMiniView(aura::Window* source_window);

  // views::View:
  void Layout() override;

 private:
  // The window this class is meant to be a header for. This class also may
  // optionally show a mirrored view of this window.
  aura::Window* source_window_;

  // Views for the icon and title.
  views::View* header_view_ = nullptr;
  views::Label* title_label_ = nullptr;
  views::ImageView* image_view_ = nullptr;

  // A view that covers the area except the header. It is null when the window
  // associated is not pillar or letter boxed.
  RoundedRectView* backdrop_view_ = nullptr;

  // Optionally shows a preview of |window_|.
  WindowPreviewView* preview_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowMiniView);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_H_
