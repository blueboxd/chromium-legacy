// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_

#if defined(OS_WIN)
#include <windows.h>
#else
#include "ui/base/hit_test.h"
#endif

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/mojom/ui_base_types.mojom.h"
#include "ui/base/ui_base_types.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::DialogButton, ui::DialogButton> {
  static ui::mojom::DialogButton ToMojom(ui::DialogButton modal_type) {
    switch (modal_type) {
      case ui::DIALOG_BUTTON_NONE:
        return ui::mojom::DialogButton::NONE;
      case ui::DIALOG_BUTTON_OK:
        return ui::mojom::DialogButton::OK;
      case ui::DIALOG_BUTTON_CANCEL:
        return ui::mojom::DialogButton::CANCEL;
      default:
        NOTREACHED();
        return ui::mojom::DialogButton::NONE;
    }
  }

  static bool FromMojom(ui::mojom::DialogButton modal_type,
                        ui::DialogButton* out) {
    switch (modal_type) {
      case ui::mojom::DialogButton::NONE:
        *out = ui::DIALOG_BUTTON_NONE;
        return true;
      case ui::mojom::DialogButton::OK:
        *out = ui::DIALOG_BUTTON_OK;
        return true;
      case ui::mojom::DialogButton::CANCEL:
        *out = ui::DIALOG_BUTTON_CANCEL;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

template <>
struct EnumTraits<ui::mojom::ModalType, ui::ModalType> {
  static ui::mojom::ModalType ToMojom(ui::ModalType modal_type) {
    switch (modal_type) {
      case ui::MODAL_TYPE_NONE:
        return ui::mojom::ModalType::NONE;
      case ui::MODAL_TYPE_WINDOW:
        return ui::mojom::ModalType::WINDOW;
      case ui::MODAL_TYPE_CHILD:
        return ui::mojom::ModalType::CHILD;
      case ui::MODAL_TYPE_SYSTEM:
        return ui::mojom::ModalType::SYSTEM;
      default:
        NOTREACHED();
        return ui::mojom::ModalType::NONE;
    }
  }

  static bool FromMojom(ui::mojom::ModalType modal_type, ui::ModalType* out) {
    switch (modal_type) {
      case ui::mojom::ModalType::NONE:
        *out = ui::MODAL_TYPE_NONE;
        return true;
      case ui::mojom::ModalType::WINDOW:
        *out = ui::MODAL_TYPE_WINDOW;
        return true;
      case ui::mojom::ModalType::CHILD:
        *out = ui::MODAL_TYPE_CHILD;
        return true;
      case ui::mojom::ModalType::SYSTEM:
        *out = ui::MODAL_TYPE_SYSTEM;
        return true;
      default:
        NOTREACHED();
        return false;
    }
  }
};

template <>
struct EnumTraits<ui::mojom::HitTest, int> {
  static ui::mojom::HitTest ToMojom(int hit_test) {
    switch (hit_test) {
      case HTNOWHERE:
        return ui::mojom::HitTest::kNowhere;
      case HTBORDER:
        return ui::mojom::HitTest::kBorder;
      case HTBOTTOM:
        return ui::mojom::HitTest::kBottom;
      case HTBOTTOMLEFT:
        return ui::mojom::HitTest::kBottomLeft;
      case HTBOTTOMRIGHT:
        return ui::mojom::HitTest::kBottomRight;
      case HTCAPTION:
        return ui::mojom::HitTest::kCaption;
      case HTCLIENT:
        return ui::mojom::HitTest::kClient;
      case HTCLOSE:
        return ui::mojom::HitTest::kClose;
      case HTERROR:
        return ui::mojom::HitTest::kError;
#if !defined(OS_WIN)
      // On Windows, HTGROWBOX is same as HTSIZE, this case never appears.
      case HTGROWBOX:
        return ui::mojom::HitTest::kGrowbox;
#endif
      case HTHELP:
        return ui::mojom::HitTest::kHelp;
      case HTHSCROLL:
        return ui::mojom::HitTest::kHScroll;
      case HTLEFT:
        return ui::mojom::HitTest::kLeft;
      case HTMENU:
        return ui::mojom::HitTest::kMenu;
      case HTMAXBUTTON:
        return ui::mojom::HitTest::kMaxButton;
      case HTMINBUTTON:
        return ui::mojom::HitTest::kMinButton;
#if !defined(OS_WIN)
      // On Windows, HTREDUCE is same as HTMINBUTTON, this case never appears.
      case HTREDUCE:
        return ui::mojom::HitTest::kReduce;
#endif
      case HTRIGHT:
        return ui::mojom::HitTest::kRight;
      case HTSIZE:
        return ui::mojom::HitTest::kSize;
      case HTSYSMENU:
        return ui::mojom::HitTest::kSysMenu;
      case HTTOP:
        return ui::mojom::HitTest::kTop;
      case HTTOPLEFT:
        return ui::mojom::HitTest::kTopLeft;
      case HTTOPRIGHT:
        return ui::mojom::HitTest::kTopRight;
      case HTTRANSPARENT:
        return ui::mojom::HitTest::kTransparent;
      case HTVSCROLL:
        return ui::mojom::HitTest::kVScroll;
#if !defined(OS_WIN)
      // On Windows, HTZOOM is same as HTMAXBUTTON, this case never appears.
      case HTZOOM:
        return ui::mojom::HitTest::kZoom;
#endif
      default:
        NOTREACHED();
        return ui::mojom::HitTest::kNowhere;
    }
  }

  static bool FromMojom(ui::mojom::HitTest hit_test, int* out) {
    switch (hit_test) {
      case ui::mojom::HitTest::kNowhere:
        *out = HTNOWHERE;
        return true;
      case ui::mojom::HitTest::kBorder:
        *out = HTBORDER;
        return true;
      case ui::mojom::HitTest::kBottom:
        *out = HTBOTTOM;
        return true;
      case ui::mojom::HitTest::kBottomLeft:
        *out = HTBOTTOMLEFT;
        return true;
      case ui::mojom::HitTest::kBottomRight:
        *out = HTBOTTOMRIGHT;
        return true;
      case ui::mojom::HitTest::kCaption:
        *out = HTCAPTION;
        return true;
      case ui::mojom::HitTest::kClient:
        *out = HTCLIENT;
        return true;
      case ui::mojom::HitTest::kClose:
        *out = HTCLOSE;
        return true;
      case ui::mojom::HitTest::kError:
        *out = HTERROR;
        return true;
      case ui::mojom::HitTest::kGrowbox:
        *out = HTGROWBOX;
        return true;
      case ui::mojom::HitTest::kHelp:
        *out = HTHELP;
        return true;
      case ui::mojom::HitTest::kHScroll:
        *out = HTHSCROLL;
        return true;
      case ui::mojom::HitTest::kLeft:
        *out = HTLEFT;
        return true;
      case ui::mojom::HitTest::kMenu:
        *out = HTMENU;
        return true;
      case ui::mojom::HitTest::kMaxButton:
        *out = HTMAXBUTTON;
        return true;
      case ui::mojom::HitTest::kMinButton:
        *out = HTMINBUTTON;
        return true;
      case ui::mojom::HitTest::kReduce:
        *out = HTREDUCE;
        return true;
      case ui::mojom::HitTest::kRight:
        *out = HTRIGHT;
        return true;
      case ui::mojom::HitTest::kSize:
        *out = HTSIZE;
        return true;
      case ui::mojom::HitTest::kSysMenu:
        *out = HTSYSMENU;
        return true;
      case ui::mojom::HitTest::kTop:
        *out = HTTOP;
        return true;
      case ui::mojom::HitTest::kTopLeft:
        *out = HTTOPLEFT;
        return true;
      case ui::mojom::HitTest::kTopRight:
        *out = HTTOPRIGHT;
        return true;
      case ui::mojom::HitTest::kTransparent:
        *out = HTTRANSPARENT;
        return true;
      case ui::mojom::HitTest::kVScroll:
        *out = HTVSCROLL;
        return true;
      case ui::mojom::HitTest::kZoom:
        *out = HTZOOM;
        return true;
      default:
        NOTREACHED();
        *out = HTNOWHERE;
        return false;
    }
  }
};

template <>
struct EnumTraits<ui::mojom::WindowShowState, ui::WindowShowState> {
  static ui::mojom::WindowShowState ToMojom(ui::WindowShowState modal_type) {
    switch (modal_type) {
      case ui::SHOW_STATE_DEFAULT:
        return ui::mojom::WindowShowState::kDefault;
      case ui::SHOW_STATE_NORMAL:
        return ui::mojom::WindowShowState::kNormal;
      case ui::SHOW_STATE_MINIMIZED:
        return ui::mojom::WindowShowState::kMinimized;
      case ui::SHOW_STATE_MAXIMIZED:
        return ui::mojom::WindowShowState::kMaximized;
      case ui::SHOW_STATE_INACTIVE:
        return ui::mojom::WindowShowState::kInactive;
      case ui::SHOW_STATE_FULLSCREEN:
        return ui::mojom::WindowShowState::kFullscreen;
      case ui::SHOW_STATE_END:
        NOTREACHED();
        break;
    }
    NOTREACHED();
    return ui::mojom::WindowShowState::kDefault;
  }

  static bool FromMojom(ui::mojom::WindowShowState modal_type,
                        ui::WindowShowState* out) {
    switch (modal_type) {
      case ui::mojom::WindowShowState::kDefault:
        *out = ui::SHOW_STATE_DEFAULT;
        return true;
      case ui::mojom::WindowShowState::kNormal:
        *out = ui::SHOW_STATE_NORMAL;
        return true;
      case ui::mojom::WindowShowState::kMinimized:
        *out = ui::SHOW_STATE_MINIMIZED;
        return true;
      case ui::mojom::WindowShowState::kMaximized:
        *out = ui::SHOW_STATE_MAXIMIZED;
        return true;
      case ui::mojom::WindowShowState::kInactive:
        *out = ui::SHOW_STATE_INACTIVE;
        return true;
      case ui::mojom::WindowShowState::kFullscreen:
        *out = ui::SHOW_STATE_FULLSCREEN;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_UI_BASE_TYPES_MOJOM_TRAITS_H_
