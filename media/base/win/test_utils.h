// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_TEST_UTILS_H_
#define MEDIA_BASE_WIN_TEST_UTILS_H_

#include <wrl/client.h>
#include <wrl/implements.h>

#include "testing/gmock/include/gmock/gmock.h"

#define MOCK_STDCALL_METHOD0(Name, Types) \
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD1(Name, Types) \
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD2(Name, Types) \
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD3(Name, Types) \
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD4(Name, Types) \
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD5(Name, Types) \
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD6(Name, Types) \
  MOCK_METHOD6_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD7(Name, Types) \
  MOCK_METHOD7_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD8(Name, Types) \
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

#define MOCK_STDCALL_METHOD9(Name, Types) \
  MOCK_METHOD9_WITH_CALLTYPE(STDMETHODCALLTYPE, Name, Types)

// Helper ON_CALL and EXPECT_CALL for Microsoft::WRL::ComPtr, e.g.
//   COM_EXPECT_CALL(foo_, Bar());
// where |foo_| is ComPtr<D3D11FooMock>.
#define COM_ON_CALL(obj, call) ON_CALL(*obj.Get(), call)
#define COM_EXPECT_CALL(obj, call) EXPECT_CALL(*obj.Get(), call)

namespace media {

// Use this action when using SetArgPointee with COM pointers.
// e.g.
// COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
//  .WillRepeatedly(DoAll(
//     SetComPointee<1>(video_device_mock_.Get()), Return(S_OK)));
ACTION_TEMPLATE(SetComPointee,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p)) {
  p->AddRef();
  *std::get<k>(args) = p;
}

// Same as above, but returns S_OK for convenience.
// e.g.
// COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
//  .WillRepeatedly(SetComPointeeAndReturnOk<1>(video_device_mock_.Get()));
ACTION_TEMPLATE(SetComPointeeAndReturnOk,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p)) {
  p->AddRef();
  *std::get<k>(args) = p;
  return S_OK;
}

// Use this function to create a mock so that they are ref-counted correctly.
template <typename Interface>
Microsoft::WRL::ComPtr<Interface> MakeComPtr() {
  return Microsoft::WRL::Make<Interface>();
}

}  // namespace media

#endif  // MEDIA_BASE_WIN_TEST_UTILS_H_
