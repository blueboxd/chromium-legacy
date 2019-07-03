// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_fragment_root_win.h"

#include <unordered_map>

#include "base/no_destructor.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/base/win/atl_module.h"

namespace ui {

class AXFragmentRootPlatformNodeWin : public AXPlatformNodeWin,
                                      public IRawElementProviderFragmentRoot,
                                      public IRawElementProviderAdviseEvents {
 public:
  BEGIN_COM_MAP(AXFragmentRootPlatformNodeWin)
  COM_INTERFACE_ENTRY(IRawElementProviderFragmentRoot)
  COM_INTERFACE_ENTRY(IRawElementProviderAdviseEvents)
  COM_INTERFACE_ENTRY_CHAIN(AXPlatformNodeWin)
  END_COM_MAP()

  static AXFragmentRootPlatformNodeWin* Create(
      AXPlatformNodeDelegate* delegate) {
    // Make sure ATL is initialized in this module.
    win::CreateATLModuleIfNeeded();

    CComObject<AXFragmentRootPlatformNodeWin>* instance = nullptr;
    HRESULT hr =
        CComObject<AXFragmentRootPlatformNodeWin>::CreateInstance(&instance);
    DCHECK(SUCCEEDED(hr));
    instance->Init(delegate);
    instance->AddRef();
    return instance;
  }

  //
  // IRawElementProviderSimple methods.
  //

  STDMETHOD(get_HostRawElementProvider)
  (IRawElementProviderSimple** host_element_provider) override {
    UIA_VALIDATE_CALL_1_ARG(host_element_provider);

    HWND hwnd = GetDelegate()->GetTargetForNativeAccessibilityEvent();
    return UiaHostProviderFromHwnd(hwnd, host_element_provider);
  }

  //
  // IRawElementProviderFragment methods.
  //

  STDMETHOD(get_FragmentRoot)
  (IRawElementProviderFragmentRoot** fragment_root) override {
    UIA_VALIDATE_CALL_1_ARG(fragment_root);

    QueryInterface(IID_PPV_ARGS(fragment_root));
    return S_OK;
  }

  STDMETHOD(GetRuntimeId)(SAFEARRAY** runtime_id) {
    UIA_VALIDATE_CALL_1_ARG(runtime_id);

    // UIA obtains a runtime ID for a fragment root from the associated HWND.
    *runtime_id = nullptr;
    return S_OK;
  }

  //
  // IRawElementProviderFragmentRoot methods.
  //

  // x and y are in pixels, in screen coordinates.
  STDMETHOD(ElementProviderFromPoint)
  (double x,
   double y,
   IRawElementProviderFragment** element_provider) override {
    UIA_VALIDATE_CALL_1_ARG(element_provider);

    *element_provider = nullptr;

    gfx::NativeViewAccessible hit_element = nullptr;

    // Descend the tree until we get a non-hit or can't go any further.
    AXPlatformNode* node_to_test = this;
    do {
      gfx::NativeViewAccessible test_result =
          node_to_test->GetDelegate()->HitTestSync(x, y);
      if (test_result != nullptr && test_result != hit_element) {
        hit_element = test_result;
        node_to_test = AXPlatformNode::FromNativeViewAccessible(test_result);
      } else {
        node_to_test = nullptr;
      }
    } while (node_to_test);

    if (hit_element)
      hit_element->QueryInterface(element_provider);

    return S_OK;
  }

  STDMETHOD(GetFocus)(IRawElementProviderFragment** focus) override {
    UIA_VALIDATE_CALL_1_ARG(focus);

    *focus = nullptr;

    gfx::NativeViewAccessible focused_element = GetDelegate()->GetFocus();
    if (focused_element != nullptr) {
      focused_element->QueryInterface(IID_PPV_ARGS(focus));
    }

    return S_OK;
  }

  //
  // IRawElementProviderAdviseEvents methods.
  //
  STDMETHODIMP AdviseEventAdded(EVENTID event_id,
                                SAFEARRAY* property_ids) override {
    if (event_id == UIA_LiveRegionChangedEventId) {
      live_region_change_listeners_++;

      if (live_region_change_listeners_ == 1) {
        // Fire a LiveRegionChangedEvent for each live-region to tell the
        // newly-attached assistive technology about the regions.
        //
        // Ideally we'd be able to direct these events to only the
        // newly-attached AT, but we don't have that capability, so we only
        // fire events when the *first* AT attaches. (A common scenario will
        // be an attached screen-reader, then a software-keyboard attaches to
        // handle an input field; we don't want the screen-reader to announce
        // that every live-region has changed.) There isn't a perfect solution,
        // but this heuristic seems to work well in practice.
        FireLiveRegionChangeRecursive();
      }
    }
    return S_OK;
  }

  STDMETHODIMP AdviseEventRemoved(EVENTID event_id,
                                  SAFEARRAY* property_ids) override {
    if (event_id == UIA_LiveRegionChangedEventId) {
      DCHECK(live_region_change_listeners_ > 0);
      live_region_change_listeners_--;
    }
    return S_OK;
  }

 private:
  int32_t live_region_change_listeners_ = 0;
};

class AXFragmentRootMapWin {
 public:
  static AXFragmentRootMapWin& GetInstance() {
    static base::NoDestructor<AXFragmentRootMapWin> instance;
    return *instance;
  }

  void AddFragmentRoot(gfx::AcceleratedWidget widget,
                       AXFragmentRootWin* fragment_root) {
    map_[widget] = fragment_root;
  }

  void RemoveFragmentRoot(gfx::AcceleratedWidget widget) { map_.erase(widget); }

  ui::AXFragmentRootWin* GetFragmentRoot(gfx::AcceleratedWidget widget) {
    return map_[widget];
  }

 private:
  std::unordered_map<gfx::AcceleratedWidget, AXFragmentRootWin*> map_;
};

AXFragmentRootWin::AXFragmentRootWin(gfx::AcceleratedWidget widget,
                                     AXFragmentRootDelegateWin* delegate)
    : widget_(widget), delegate_(delegate) {
  platform_node_ = ui::AXFragmentRootPlatformNodeWin::Create(this);
  AXFragmentRootMapWin::GetInstance().AddFragmentRoot(widget, this);
}

AXFragmentRootWin::~AXFragmentRootWin() {
  AXFragmentRootMapWin::GetInstance().RemoveFragmentRoot(widget_);
  platform_node_->Destroy();
  platform_node_ = nullptr;
}

AXFragmentRootWin* AXFragmentRootWin::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return AXFragmentRootMapWin::GetInstance().GetFragmentRoot(widget);
}

gfx::NativeViewAccessible AXFragmentRootWin::GetNativeViewAccessible() {
  return platform_node_.Get();
}

gfx::NativeViewAccessible AXFragmentRootWin::GetParent() {
  return delegate_->GetParentOfAXFragmentRoot();
}

int AXFragmentRootWin::GetChildCount() {
  return delegate_->GetChildOfAXFragmentRoot() ? 1 : 0;
}

gfx::NativeViewAccessible AXFragmentRootWin::ChildAtIndex(int index) {
  if (index == 0) {
    return delegate_->GetChildOfAXFragmentRoot();
  }

  return nullptr;
}

gfx::NativeViewAccessible AXFragmentRootWin::HitTestSync(int x, int y) {
  AXPlatformNodeDelegate* child_delegate = GetChildNodeDelegate();
  if (child_delegate)
    return child_delegate->HitTestSync(x, y);

  return nullptr;
}

gfx::NativeViewAccessible AXFragmentRootWin::GetFocus() {
  AXPlatformNodeDelegate* child_delegate = GetChildNodeDelegate();
  if (child_delegate)
    return child_delegate->GetFocus();

  return nullptr;
}

const ui::AXUniqueId& AXFragmentRootWin::GetUniqueId() const {
  return unique_id_;
}

gfx::AcceleratedWidget
AXFragmentRootWin::GetTargetForNativeAccessibilityEvent() {
  return widget_;
}

AXPlatformNodeDelegate* AXFragmentRootWin::GetChildNodeDelegate() {
  gfx::NativeViewAccessible child = delegate_->GetChildOfAXFragmentRoot();
  if (child)
    return ui::AXPlatformNode::FromNativeViewAccessible(child)->GetDelegate();

  return nullptr;
}

}  // namespace ui
