// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.testing.host.stream;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;

/** Fake for {@link CardConfiguration}. */
public class FakeCardConfiguration implements CardConfiguration {

  private static final ColorDrawable COLOR_DRAWABLE = new ColorDrawable(Color.RED);

  private int bottomMargin = 1;
  private int startMargin = 2;
  private int endMargin = 3;

  @Override
  public int getDefaultCornerRadius() {
    return 0;
  }

  @Override
  public Drawable getCardBackground() {
    return COLOR_DRAWABLE;
  }

  @Override
  public int getCardBottomMargin() {
    return bottomMargin;
  }

  @Override
  public int getCardStartMargin() {
    return startMargin;
  }

  @Override
  public int getCardEndMargin() {
    return endMargin;
  }

  public void setCardStartMargin(int cardStartMargin) {
    this.startMargin = cardStartMargin;
  }

  public void setCardEndMargin(int cardEndMargin) {
    this.endMargin = cardEndMargin;
  }

  public void setCardBottomMargin(int cardBottomMargin) {
    this.bottomMargin = cardBottomMargin;
  }
}
