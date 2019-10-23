// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/tsf_text_store.h"

#include <initguid.h>  // for GUID_NULL and GUID_PROP_INPUTSCOPE

#include <InputScope.h>
#include <OleCtl.h>
#include <wrl/client.h>

#if defined(OS_WIN)
#include <vector>
#endif

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/mock_tsf_bridge.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace ui {
namespace {

class MockTextInputClient : public TextInputClient {
 public:
  ~MockTextInputClient() {}
  MOCK_METHOD1(SetCompositionText, void(const ui::CompositionText&));
  MOCK_METHOD0(ConfirmCompositionText, void());
  MOCK_METHOD0(ClearCompositionText, void());
  MOCK_METHOD1(InsertText, void(const base::string16&));
  MOCK_METHOD1(InsertChar, void(const ui::KeyEvent&));
  MOCK_CONST_METHOD0(GetTextInputType, ui::TextInputType());
  MOCK_CONST_METHOD0(GetTextInputMode, ui::TextInputMode());
  MOCK_CONST_METHOD0(GetTextDirection, base::i18n::TextDirection());
  MOCK_CONST_METHOD0(GetTextInputFlags, int());
  MOCK_CONST_METHOD0(CanComposeInline, bool());
  MOCK_CONST_METHOD0(GetCaretBounds, gfx::Rect());
  MOCK_CONST_METHOD2(GetCompositionCharacterBounds, bool(uint32_t, gfx::Rect*));
  MOCK_CONST_METHOD0(HasCompositionText, bool());
  MOCK_CONST_METHOD0(GetFocusReason, ui::TextInputClient::FocusReason());
  MOCK_METHOD0(ShouldDoLearning, bool());
  MOCK_CONST_METHOD1(GetTextRange, bool(gfx::Range*));
  MOCK_CONST_METHOD1(GetCompositionTextRange, bool(gfx::Range*));
  MOCK_CONST_METHOD1(GetEditableSelectionRange, bool(gfx::Range*));
  MOCK_METHOD1(SetEditableSelectionRange, bool(const gfx::Range&));
  MOCK_METHOD1(DeleteRange, bool(const gfx::Range&));
  MOCK_CONST_METHOD2(GetTextFromRange,
                     bool(const gfx::Range&, base::string16*));
  MOCK_METHOD0(OnInputMethodChanged, void());
  MOCK_METHOD1(ChangeTextDirectionAndLayoutAlignment,
               bool(base::i18n::TextDirection));
  MOCK_METHOD2(ExtendSelectionAndDelete, void(size_t, size_t));
  MOCK_METHOD1(EnsureCaretNotInRect, void(const gfx::Rect&));
  MOCK_CONST_METHOD1(IsTextEditCommandEnabled, bool(TextEditCommand));
  MOCK_METHOD1(SetTextEditCommandForNextKeyEvent, void(TextEditCommand));
  MOCK_CONST_METHOD0(GetClientSourceForMetrics, ukm::SourceId());
  MOCK_METHOD2(SetCompositionFromExistingText,
               bool(const gfx::Range&, const std::vector<ui::ImeTextSpan>&));
  MOCK_METHOD3(SetActiveCompositionForAccessibility,
               void(const gfx::Range&, const base::string16&, bool));
};

class MockInputMethodDelegate : public internal::InputMethodDelegate {
 public:
  ~MockInputMethodDelegate() {}
  MOCK_METHOD1(DispatchKeyEventPostIME, EventDispatchDetails(KeyEvent*));
};

class MockStoreACPSink : public ITextStoreACPSink {
 public:
  MockStoreACPSink() : ref_count_(0) {}

  // IUnknown
  ULONG STDMETHODCALLTYPE AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }
  ULONG STDMETHODCALLTYPE Release() override {
    const LONG count = InterlockedDecrement(&ref_count_);
    if (!count) {
      delete this;
      return 0;
    }
    return static_cast<ULONG>(count);
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** report) override {
    if (iid == IID_IUnknown || iid == IID_ITextStoreACPSink) {
      *report = static_cast<ITextStoreACPSink*>(this);
    } else {
      *report = nullptr;
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  // ITextStoreACPSink
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnTextChange,
                             HRESULT(DWORD, const TS_TEXTCHANGE*));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, OnSelectionChange, HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnLayoutChange,
                             HRESULT(TsLayoutCode, TsViewCookie));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, OnStatusChange, HRESULT(DWORD));
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnAttrsChange,
                             HRESULT(LONG, LONG, ULONG, const TS_ATTRID*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, OnLockGranted, HRESULT(DWORD));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnStartEditTransaction,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             OnEndEditTransaction,
                             HRESULT());

 private:
  virtual ~MockStoreACPSink() {}

  volatile LONG ref_count_;
};

class FakeInputMethod : public DummyInputMethod {
 public:
  FakeInputMethod() : client_(nullptr), count_show_ime_if_needed_(0) {}

  void SetFocusedTextInputClient(TextInputClient* client) override {
    count_set_focused_text_input_client_++;
    client_ = client;
  }

  TextInputClient* GetTextInputClient() const override { return client_; }

  void ShowVirtualKeyboardIfEnabled() override {
    count_show_ime_if_needed_++;
    // Set the input policy in textstore using TSFBridge
    tsf_bridge_->SetInputPanelPolicy(/*inputPanelPolicyManual*/ false);
  }

  void DetachTextInputClient(TextInputClient* client) override {
    if (client_ == client)
      client_ = nullptr;
    // Set the input policy in textstore using TSFBridge
    tsf_bridge_->SetInputPanelPolicy(/*inputPanelPolicyManual*/ true);
  }

  void OnTextInputTypeChanged(const TextInputClient* client) override {
    count_on_text_input_type_changed_++;
  }

  void SetTSFTextStoreForBridge(TSFTextStore* tsf_text_store) {
    tsf_bridge_ = new MockTSFBridge();
    tsf_bridge_->SetTSFTextStoreForTesting(tsf_text_store);
  }

  int count_show_ime_if_needed() const { return count_show_ime_if_needed_; }

 private:
  TextInputClient* client_;
  MockTSFBridge* tsf_bridge_;
  int count_show_ime_if_needed_;
  int count_set_focused_text_input_client_;
  int count_on_text_input_type_changed_;
};

const HWND kWindowHandle = reinterpret_cast<HWND>(1);

}  // namespace

class TSFInputPanelTest : public testing::Test {
 protected:
  void SetUp() override {
    text_store_ = new TSFTextStore();
    sink_ = new MockStoreACPSink();
    EXPECT_EQ(S_OK, text_store_->AdviseSink(IID_ITextStoreACPSink, sink_.get(),
                                            TS_AS_ALL_SINKS));
    text_store_->SetFocusedTextInputClient(kWindowHandle, &text_input_client_);
    text_store_->SetInputMethodDelegate(&input_method_delegate_);
    fake_input_method_ = std::make_unique<FakeInputMethod>();
    fake_input_method_->SetTSFTextStoreForBridge(text_store_.get());
  }

  void TearDown() override {
    EXPECT_EQ(S_OK, text_store_->UnadviseSink(sink_.get()));
    sink_ = nullptr;
    text_store_ = nullptr;
  }

  // Accessors to the internal state of TSFTextStore.

  base::win::ScopedCOMInitializer com_initializer_;
  MockTextInputClient text_input_client_;
  MockInputMethodDelegate input_method_delegate_;
  scoped_refptr<TSFTextStore> text_store_;
  scoped_refptr<MockStoreACPSink> sink_;
  std::unique_ptr<FakeInputMethod> fake_input_method_;
};

namespace {

TEST_F(TSFInputPanelTest, GetStatusTest) {
  TS_STATUS status = {};
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
}

TEST_F(TSFInputPanelTest, ManualInputPaneToAutomaticPolicyTest) {
  TS_STATUS status = {};
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
  fake_input_method_->ShowVirtualKeyboardIfEnabled();
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_NE((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
}

TEST_F(TSFInputPanelTest, AutomaticInputPaneToManualPolicyTest) {
  TS_STATUS status = {};
  // Invoke the virtual keyboard through InputMethod
  // and test if the automatic policy flag has been set or not.
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
  fake_input_method_->ShowVirtualKeyboardIfEnabled();
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_NE((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
  fake_input_method_->DetachTextInputClient(nullptr);
  EXPECT_EQ(S_OK, text_store_->GetStatus(&status));
  EXPECT_EQ((ULONG)TS_SD_INPUTPANEMANUALDISPLAYENABLE, status.dwDynamicFlags);
  EXPECT_EQ((ULONG)(TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT),
            status.dwStaticFlags);
}

}  // namespace
}  // namespace ui
