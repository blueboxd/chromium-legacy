// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Test suite for the ModuleActivityObserver class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ModuleActivityObserverTest {
    @Mock
    private ModuleInstaller mModuleInstallerMock;
    @Mock
    private Activity mActivityMock;

    private ModuleActivityObserver mObserver;

    @Before
    public void beforeTests() {
        MockitoAnnotations.initMocks(this);

        mObserver = spy(new ModuleActivityObserver());

        doReturn(mModuleInstallerMock).when(mObserver).getModuleInstaller();
        doReturn(new ArrayList<>()).when(mObserver).getRunningActivities();
        doReturn(ActivityState.CREATED).when(mObserver).getStateForActivity(any(Activity.class));
    }

    @Test
    public void whenOnCreate_verifySplitCompatted() {
        // Arrange.
        @ActivityState
        Integer newState = ActivityState.CREATED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mModuleInstallerMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResume_verifySplitCompatted() {
        // Arrange.
        @ActivityState
        Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mModuleInstallerMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResumeTwice_verifySplitCompattedOnlyOnce() {
        // Arrange.
        @ActivityState
        Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mModuleInstallerMock, times(1)).initActivity(mActivityMock);
    }

    @Test
    public void whenOnResumeAfterModuleInstall_verifySplitCompatted() {
        // Arrange.
        @ActivityState
        Integer newState = ActivityState.RESUMED;

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        mObserver.onModuleInstalled();
        mObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mModuleInstallerMock, times(2)).initActivity(mActivityMock);
    }

    @Test
    public void whenNotOnResumeOrNotOnCreate_verifyNotSplitCompatted() {
        // Act.
        mObserver.onActivityStateChange(mActivityMock, ActivityState.STARTED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.PAUSED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.STOPPED);
        mObserver.onActivityStateChange(mActivityMock, ActivityState.DESTROYED);

        // Assert.
        verify(mModuleInstallerMock, never()).initActivity(mActivityMock);
    }

    @Test
    public void whenMultipleInstances_verifySplitCompatCalledOnlyOnce() {
        // Arrange.
        @ActivityState
        Integer newState = ActivityState.RESUMED;

        ModuleInstaller newModuleInstallerMock = mock(ModuleInstaller.class);
        ModuleActivityObserver newObserver = spy(new ModuleActivityObserver());
        doReturn(newModuleInstallerMock).when(newObserver).getModuleInstaller();

        // Act.
        mObserver.onActivityStateChange(mActivityMock, newState);
        newObserver.onActivityStateChange(mActivityMock, newState);

        // Assert.
        verify(mModuleInstallerMock, times(1)).initActivity(mActivityMock);
        verify(newModuleInstallerMock, never()).initActivity(mActivityMock);
    }

    @Test
    public void whenOnModuleInstalled_verifyOnlyResumedActivitiesAreSplitCompatted() {
        // Arrange.
        List<Activity> activitiesList = new ArrayList<Activity>();
        Activity activityMock1 = mock(Activity.class);
        Activity activityMock2 = mock(Activity.class);
        Activity activityMock3 = mock(Activity.class);

        activitiesList.add(activityMock1);
        activitiesList.add(activityMock2);
        activitiesList.add(activityMock3);

        doReturn(activitiesList).when(mObserver).getRunningActivities();

        doReturn(ActivityState.RESUMED).when(mObserver).getStateForActivity(activityMock1);
        doReturn(ActivityState.PAUSED).when(mObserver).getStateForActivity(activityMock2);
        doReturn(ActivityState.DESTROYED).when(mObserver).getStateForActivity(activityMock3);

        // Act.
        mObserver.onModuleInstalled();

        // Assert.
        verify(mModuleInstallerMock, times(1)).initActivity(any(Activity.class));
    }
}
