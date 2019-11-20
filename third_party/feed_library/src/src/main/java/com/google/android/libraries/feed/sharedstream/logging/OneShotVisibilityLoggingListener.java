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

package com.google.android.libraries.feed.sharedstream.logging;

/** Implementation of a {@link LoggingListener} that only notifies a listener of the first view. */
public class OneShotVisibilityLoggingListener implements LoggingListener {

  private final LoggingListener loggingListener;
  protected boolean viewLogged;

  public OneShotVisibilityLoggingListener(LoggingListener loggingListener) {
    this.loggingListener = loggingListener;
  }

  @Override
  public void onViewVisible() {
    if (viewLogged) {
      return;
    }

    loggingListener.onViewVisible();
    viewLogged = true;
  }

  @Override
  public void onContentClicked() {
    loggingListener.onContentClicked();
  }

  @Override
  public void onContentSwiped() {
    loggingListener.onContentSwiped();
  }

  @Override
  public void onScrollStateChanged(int newScrollState) {
    loggingListener.onScrollStateChanged(newScrollState);
  }
}
