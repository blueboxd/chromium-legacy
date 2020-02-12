// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a simple base class for DirectShow filters. It may only be used in
// a single threaded apartment.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <stddef.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace media {

class FilterBase : public IBaseFilter, public base::RefCounted<FilterBase> {
 public:
  FilterBase();

  // Number of pins connected to this filter.
  virtual size_t NoOfPins() = 0;
  // Returns the IPin interface pin no index.
  virtual IPin* GetPin(int index) = 0;

  // Inherited from IUnknown.
  STDMETHODIMP QueryInterface(REFIID id, void** object_ptr) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // Inherited from IBaseFilter.
  STDMETHODIMP EnumPins(IEnumPins** enum_pins) override;

  STDMETHODIMP FindPin(LPCWSTR id, IPin** pin) override;

  STDMETHODIMP QueryFilterInfo(FILTER_INFO* info) override;

  STDMETHODIMP JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) override;

  STDMETHODIMP QueryVendorInfo(LPWSTR* vendor_info) override;

  // Inherited from IMediaFilter.
  STDMETHODIMP Stop() override;

  STDMETHODIMP Pause() override;

  STDMETHODIMP Run(REFERENCE_TIME start) override;

  STDMETHODIMP GetState(DWORD msec_timeout, FILTER_STATE* state) override;

  STDMETHODIMP SetSyncSource(IReferenceClock* clock) override;

  STDMETHODIMP GetSyncSource(IReferenceClock** clock) override;

  // Inherited from IPersistent.
  STDMETHODIMP GetClassID(CLSID* class_id) override = 0;

 protected:
  friend class base::RefCounted<FilterBase>;
  virtual ~FilterBase();

 private:
  FILTER_STATE state_;
  Microsoft::WRL::ComPtr<IFilterGraph> owning_graph_;

  DISALLOW_COPY_AND_ASSIGN(FilterBase);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_FILTER_BASE_WIN_H_
