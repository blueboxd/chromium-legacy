// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/field_data_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/renderer_id.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

using base::UTF8ToUTF16;

namespace autofill {

class FieldDataManagerTest : public content::RenderViewTest {
 public:
  FieldDataManagerTest() {}
  ~FieldDataManagerTest() override {}

 protected:
  void SetUp() override {
    RenderViewTest::SetUp();

    LoadHTML(
        "<input type='text' id='name1' value='first'>"
        "<input type='password' id='name2' value=''>");
    blink::WebLocalFrame* web_frame = GetMainFrame();
    blink::WebElementCollection inputs =
        web_frame->GetDocument().GetElementsByHTMLTagName("input");
    for (blink::WebElement element = inputs.FirstItem(); !element.IsNull();
         element = inputs.NextItem()) {
      control_elements_.push_back(element.To<blink::WebFormControlElement>());
    }
  }

  void TearDown() override {
    control_elements_.clear();
    RenderViewTest::TearDown();
  }

  std::vector<blink::WebFormControlElement> control_elements_;
};

TEST_F(FieldDataManagerTest, UpdateFieldDataMap) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(control_elements_[0],
                                         control_elements_[0].Value().Utf16(),
                                         FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].UniqueRendererFormControlId());
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(UTF8ToUTF16("first"), field_data_manager->GetUserTypedValue(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[0],
                                         UTF8ToUTF16("newvalue"),
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(UTF8ToUTF16("newvalue"), field_data_manager->GetUserTypedValue(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[1],
                                         control_elements_[1].Value().Utf16(),
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(FieldPropertiesFlags::kNoFlags,
            field_data_manager->GetFieldPropertiesMask(FieldRendererId(
                control_elements_[1].UniqueRendererFormControlId())));

  field_data_manager->ClearData();
  EXPECT_FALSE(field_data_manager->HasFieldData(id));
}

TEST_F(FieldDataManagerTest, UpdateFieldDataMapWithNullValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0], FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].UniqueRendererFormControlId());
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(base::string16(), field_data_manager->GetUserTypedValue(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0], FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(base::string16(), field_data_manager->GetUserTypedValue(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[0],
                                         control_elements_[0].Value().Utf16(),
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(UTF8ToUTF16("first"), field_data_manager->GetUserTypedValue(id));
}

TEST_F(FieldDataManagerTest, FindMachedValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(control_elements_[0],
                                         control_elements_[0].Value().Utf16(),
                                         FieldPropertiesFlags::kUserTyped);
  EXPECT_TRUE(
      field_data_manager->FindMachedValue(UTF8ToUTF16("first_element")));
  EXPECT_FALSE(
      field_data_manager->FindMachedValue(UTF8ToUTF16("second_element")));
}

}  // namespace autofill
