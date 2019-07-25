// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_view.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "cc/paint/render_surface_filters.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// A view that controls the child view's layer so that the layer always has the
// same size as the display's original, un-scaled size in DIP. The layer is then
// transformed to fit to the virtual screen size when laid-out. This is to avoid
// scaling the image at painting time, then scaling it back to the screen size
// in the compositor.
class LayerControlView : public views::View {
 public:
  explicit LayerControlView(views::View* view) {
    AddChildView(view);
    view->SetPaintToLayer();
  }

  // Overrides views::View.
  void Layout() override {
    aura::Window* window = GetWidget()->GetNativeWindow();
    // Keep |this| at the bottom since there may be other windows on top of the
    // wallpaper view such as an overview mode shield.
    window->parent()->StackChildAtBottom(window);
    display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window);

    display::ManagedDisplayInfo info =
        Shell::Get()->display_manager()->GetDisplayInfo(display.id());

    DCHECK_EQ(1u, children().size());
    views::View* child = children().front();
    child->SetBounds(0, 0, display.size().width(), display.size().height());
    gfx::Transform transform;
    // Apply RTL transform explicitly becacuse Views layer code
    // doesn't handle RTL.  crbug.com/458753.
    transform.Translate(-child->GetMirroredX(), 0);
    child->SetTransform(transform);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerControlView);
};

}  // namespace

// This event handler receives events in the pre-target phase and takes care of
// the following:
//   - Disabling overview mode on touch release.
//   - Disabling overview mode on mouse release.
class PreEventDispatchHandler : public ui::EventHandler {
 public:
  PreEventDispatchHandler() = default;
  ~PreEventDispatchHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_RELEASED)
      HandleClickOrTap(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP)
      HandleClickOrTap(event);
  }

  void HandleClickOrTap(ui::Event* event) {
    CHECK_EQ(ui::EP_PRETARGET, event->phase());
    OverviewController* controller = Shell::Get()->overview_controller();
    if (!controller->InOverviewSession())
      return;
    // Events that happen while app list is sliding out during overview should
    // be ignored to prevent overview from disappearing out from under the user.
    if (!IsSlidingOutOverviewFromShelf())
      controller->EndOverview();
    event->StopPropagation();
  }

  DISALLOW_COPY_AND_ASSIGN(PreEventDispatchHandler);
};

////////////////////////////////////////////////////////////////////////////////
// WallpaperView, public:

WallpaperView::WallpaperView(int blur, float opacity)
    : repaint_blur_(blur),
      repaint_opacity_(opacity),
      pre_dispatch_handler_(std::make_unique<PreEventDispatchHandler>()) {
  set_context_menu_controller(this);
  AddPreTargetHandler(pre_dispatch_handler_.get());
}

WallpaperView::~WallpaperView() {
  RemovePreTargetHandler(pre_dispatch_handler_.get());
}

void WallpaperView::RepaintBlurAndOpacity(int repaint_blur,
                                          float repaint_opacity) {
  if (repaint_blur_ == repaint_blur && repaint_opacity_ == repaint_opacity)
    return;

  repaint_blur_ = repaint_blur;
  repaint_opacity_ = repaint_opacity;
  SchedulePaint();
}

const char* WallpaperView::GetClassName() const {
  return "WallpaperView";
}

bool WallpaperView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void WallpaperView::ShowContextMenuForViewImpl(views::View* source,
                                               const gfx::Point& point,
                                               ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(point, source_type);
}

void WallpaperView::DrawWallpaper(const gfx::ImageSkia& wallpaper,
                                  const gfx::Rect& src,
                                  const gfx::Rect& dst,
                                  const cc::PaintFlags& flags,
                                  gfx::Canvas* canvas) {
  // The amount we downsample the original image by before applying filters to
  // improve performance.
  constexpr float quality = 0.3f;
  gfx::Rect quality_adjusted_rect = gfx::ScaleToEnclosingRect(dst, quality);
  // Draw the wallpaper to a cached image the first time it is drawn or if the
  // size has changed.
  if (!small_image_ || small_image_->size() != quality_adjusted_rect.size()) {
    gfx::Canvas small_canvas(quality_adjusted_rect.size(),
                             /*image_scale=*/1.f,
                             /*is_opaque=*/false);
    small_canvas.DrawImageInt(wallpaper, src.x(), src.y(), src.width(),
                              src.height(), 0, 0, quality_adjusted_rect.width(),
                              quality_adjusted_rect.height(), true);
    small_image_ = base::make_optional(
        gfx::ImageSkia::CreateFrom1xBitmap(small_canvas.GetBitmap()));
  }

  if (repaint_blur_ == 0 && repaint_opacity_ == 1.f) {
    canvas->DrawImageInt(wallpaper, src.x(), src.y(), src.width(), src.height(),
                         dst.x(), dst.y(), dst.width(), dst.height(),
                         /*filter=*/true, flags);
    return;
  }

  float blur = repaint_blur_ * quality;
  // Create the blur and brightness filter to apply to the downsampled image.
  cc::PaintFlags filter_flags;
  cc::FilterOperations operations;
  operations.Append(
      cc::FilterOperation::CreateBrightnessFilter(repaint_opacity_));
  operations.Append(cc::FilterOperation::CreateBlurFilter(
      blur, SkBlurImageFilter::kClamp_TileMode));
  sk_sp<cc::PaintFilter> filter = cc::RenderSurfaceFilters::BuildImageFilter(
      operations, gfx::SizeF(dst.size()), gfx::Vector2dF());
  filter_flags.setImageFilter(filter);

  gfx::Canvas filtered_canvas(small_image_->size(),
                              /*image_scale=*/1.f,
                              /*is_opaque=*/false);
  filtered_canvas.sk_canvas()->saveLayer(nullptr, &filter_flags);
  filtered_canvas.DrawImageInt(
      *small_image_, 0, 0, small_image_->width(), small_image_->height(), 0, 0,
      small_image_->width(), small_image_->height(), true);
  filtered_canvas.sk_canvas()->restore();

  // Draw the downsampled and filtered image onto |canvas|. Draw a inseted
  // version of the image to avoid drawing a blackish border caused by the blur
  // filter. This is what we do on the login screen as well.
  gfx::ImageSkia filtered_wallpaper =
      gfx::ImageSkia::CreateFrom1xBitmap(filtered_canvas.GetBitmap());
  canvas->DrawImageInt(filtered_wallpaper, blur, blur,
                       small_image_->width() - 2 * blur,
                       small_image_->height() - 2 * blur, dst.x(), dst.y(),
                       dst.width(), dst.height(),
                       /*filter=*/true, flags);
}

views::Widget* CreateWallpaperWidget(aura::Window* root_window,
                                     int container_id,
                                     int blur,
                                     float opacity,
                                     WallpaperView** out_wallpaper_view) {
  auto* controller = Shell::Get()->wallpaper_controller();

  views::Widget* wallpaper_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "WallpaperView";
  if (controller->GetWallpaper().isNull())
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = root_window->GetChildById(container_id);
  wallpaper_widget->Init(params);
  // Owned by views.
  WallpaperView* wallpaper_view = new WallpaperView(blur, opacity);
  wallpaper_widget->SetContentsView(new LayerControlView(wallpaper_view));
  *out_wallpaper_view = wallpaper_view;
  int animation_type =
      controller->ShouldShowInitialAnimation()
          ? WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE
          : wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
  aura::Window* wallpaper_window = wallpaper_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(wallpaper_window, animation_type);

  // Enable wallpaper transition for the following cases:
  // 1. Initial wallpaper animation after device boot.
  // 2. Wallpaper fades in from a non empty background.
  // 3. From an empty background, chrome transit to a logged in user session.
  // 4. From an empty background, guest user logged in.
  if (controller->ShouldShowInitialAnimation() ||
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller()
          ->IsAnimating() ||
      Shell::Get()->session_controller()->NumberOfLoggedInUsers()) {
    ::wm::SetWindowVisibilityAnimationTransition(wallpaper_window,
                                                 ::wm::ANIMATE_SHOW);
    base::TimeDelta animation_duration = controller->animation_duration();
    if (!animation_duration.is_zero()) {
      ::wm::SetWindowVisibilityAnimationDuration(wallpaper_window,
                                                 animation_duration);
    }
  } else {
    // Disable animation if transition to login screen from an empty background.
    ::wm::SetWindowVisibilityAnimationTransition(wallpaper_window,
                                                 ::wm::ANIMATE_NONE);
  }

  aura::Window* container = root_window->GetChildById(container_id);
  wallpaper_widget->SetBounds(container->bounds());

  return wallpaper_widget;
}

}  // namespace ash
