// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a simple base class for a DirectShow input pin. It may only be
// used in a single threaded apartment.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <wrl/client.h>

#include "base/memory/ref_counted.h"

namespace media {

class PinBase : public IPin,
                public IMemInputPin,
                public base::RefCounted<PinBase> {
 public:
  explicit PinBase(IBaseFilter* owner);

  // Function used for changing the owner.
  // If the owner is deleted the owner should first call this function
  // with owner = NULL.
  void SetOwner(IBaseFilter* owner);

  // Checks if a media type is acceptable. This is called when this pin is
  // connected to an output pin. Must return true if the media type is
  // acceptable, false otherwise.
  virtual bool IsMediaTypeValid(const AM_MEDIA_TYPE* media_type) = 0;

  // Enumerates valid media types.
  virtual bool GetValidMediaType(int index, AM_MEDIA_TYPE* media_type) = 0;

  // Called when new media is received. Note that this is not on the same
  // thread as where the pin is created.
  STDMETHODIMP Receive(IMediaSample* sample) override = 0;

  STDMETHODIMP Connect(IPin* receive_pin,
                       const AM_MEDIA_TYPE* media_type) override;

  STDMETHODIMP ReceiveConnection(IPin* connector,
                                 const AM_MEDIA_TYPE* media_type) override;

  STDMETHODIMP Disconnect() override;

  STDMETHODIMP ConnectedTo(IPin** pin) override;

  STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* media_type) override;

  STDMETHODIMP QueryPinInfo(PIN_INFO* info) override;

  STDMETHODIMP QueryDirection(PIN_DIRECTION* pin_dir) override;

  STDMETHODIMP QueryId(LPWSTR* id) override;

  STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* media_type) override;

  STDMETHODIMP EnumMediaTypes(IEnumMediaTypes** types) override;

  STDMETHODIMP QueryInternalConnections(IPin** pins, ULONG* no_pins) override;

  STDMETHODIMP EndOfStream() override;

  STDMETHODIMP BeginFlush() override;

  STDMETHODIMP EndFlush() override;

  STDMETHODIMP NewSegment(REFERENCE_TIME start,
                          REFERENCE_TIME stop,
                          double dRate) override;

  // Inherited from IMemInputPin.
  STDMETHODIMP GetAllocator(IMemAllocator** allocator) override;

  STDMETHODIMP NotifyAllocator(IMemAllocator* allocator,
                               BOOL read_only) override;

  STDMETHODIMP GetAllocatorRequirements(
      ALLOCATOR_PROPERTIES* properties) override;

  STDMETHODIMP ReceiveMultiple(IMediaSample** samples,
                               long sample_count,
                               long* processed) override;
  STDMETHODIMP ReceiveCanBlock() override;

  // Inherited from IUnknown.
  STDMETHODIMP QueryInterface(REFIID id, void** object_ptr) override;

  STDMETHODIMP_(ULONG) AddRef() override;

  STDMETHODIMP_(ULONG) Release() override;

 protected:
  friend class base::RefCounted<PinBase>;
  virtual ~PinBase();

 private:
  AM_MEDIA_TYPE current_media_type_;
  Microsoft::WRL::ComPtr<IPin> connected_pin_;
  // owner_ is the filter owning this pin. We don't reference count it since
  // that would create a circular reference count.
  IBaseFilter* owner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_PIN_BASE_WIN_H_
