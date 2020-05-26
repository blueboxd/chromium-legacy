// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.animation.Animator;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.VideoView;

import androidx.annotation.VisibleForTesting;
import androidx.core.math.MathUtils;
import androidx.core.view.GestureDetectorCompat;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Encapsulates the video player functionality of the Photo Picker dialog.
 */
public class PickerVideoPlayer
        extends FrameLayout implements View.OnClickListener, SeekBar.OnSeekBarChangeListener,
                                       View.OnSystemUiVisibilityChangeListener {
    /**
     * A callback interface for notifying about video playback status.
     */
    public interface VideoPlaybackStatusCallback {
        // Called when the video starts playing.
        void onVideoPlaying();

        // Called when the video stops playing.
        void onVideoEnded();
    }

    // The callback to use for reporting playback progress in tests.
    private static VideoPlaybackStatusCallback sProgressCallback;

    // The amount of time (in milliseconds) to skip when fast forwarding/rewinding.
    private static final int SKIP_LENGTH_IN_MS = 10000;

    // The DecorView for the dialog the player is shown in.
    private View mDecorView;

    // The resources to use.
    private Resources mResources;

    // The video preview view.
    private final VideoView mVideoView;

    // The MediaPlayer object used to control the VideoView.
    private MediaPlayer mMediaPlayer;

    // The container view for all the UI elements overlaid on top of the video.
    private final View mVideoOverlayContainer;

    // The container view for the UI video controls within the overlaid window.
    private final View mVideoControls;

    // The large Play button overlaid on top of the video.
    private final ImageView mLargePlayButton;

    // The Mute button for the video.
    private final ImageView mMuteButton;

    // Keeps track of whether audio track is enabled or not.
    private boolean mAudioOn = true;

    // The Fullscreen button.
    private final ImageView mFullscreenButton;

    // Keeps track of whether full screen is enabled or not.
    private boolean mFullScreenEnabled;

    // Keeps track of whether full screen was toggled via the button in-app or via a system handled
    // user gesture (such as dragging from the top).
    private boolean mFullScreenToggledInApp;

    // The remaining video playback time.
    private final TextView mRemainingTime;

    // The message shown when seeking, to remind the user of the fast forward/back feature.
    private final LinearLayout mFastForwardMessage;

    // The SeekBar showing the video playback progress (allows user seeking).
    private final SeekBar mSeekBar;

    // A flag to control when the playback monitor schedules new tasks.
    private boolean mRunPlaybackMonitoringTask;

    // The previous options for the System UI visibility.
    private int mPreviousSystemUiVisibilityOptions;

    // The object to convert touch events into gestures.
    private GestureDetectorCompat mGestureDetector;

    // An OnGestureListener class for handling double tap.
    private class DoubleTapGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDoubleTap(MotionEvent e) {
            return onDoubleTapVideo(e);
        }
    }

    /**
     * Constructor for inflating from XML.
     */
    public PickerVideoPlayer(Context context, AttributeSet attrs) {
        super(context, attrs);
        mResources = context.getResources();

        LayoutInflater.from(context).inflate(R.layout.video_player, this);

        mVideoView = findViewById(R.id.video_player);
        mVideoOverlayContainer = findViewById(R.id.video_overlay_container);
        mVideoControls = findViewById(R.id.video_controls);
        mLargePlayButton = findViewById(R.id.video_player_play_button);
        mMuteButton = findViewById(R.id.mute);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        mFullscreenButton = findViewById(R.id.fullscreen);
        mRemainingTime = findViewById(R.id.remaining_time);
        mSeekBar = findViewById(R.id.seek_bar);
        mFastForwardMessage = findViewById(R.id.fast_forward_message);

        mVideoOverlayContainer.setOnClickListener(this);
        mLargePlayButton.setOnClickListener(this);
        mMuteButton.setOnClickListener(this);
        mFullscreenButton.setOnClickListener(this);
        mSeekBar.setOnSeekBarChangeListener(this);

        mGestureDetector = new GestureDetectorCompat(context, new DoubleTapGestureListener());
        mVideoOverlayContainer.setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                mGestureDetector.onTouchEvent(event);
                return false;
            }
        });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mVideoControls.getVisibility() != View.GONE) {
            // When configuration changes, the video overlay controls need to be synced to the new
            // video size. Post a task, so that size adjustments happen after layout of the video
            // controls has completed.
            ThreadUtils.postOnUiThread(() -> { syncOverlayControlsSize(); });
        }
    }

    /**
     * Start playback of a video in an overlay above the photo picker.
     * @param uri The uri of the video to start playing.
     * @param decorView The decorView for the dialog.
     */
    public void startVideoPlaybackAsync(Uri uri, View decorView) {
        mDecorView = decorView;

        setVisibility(View.VISIBLE);

        mVideoView.setVisibility(View.VISIBLE);
        mVideoView.setVideoURI(uri);

        mVideoView.setOnPreparedListener((MediaPlayer mediaPlayer) -> {
            mMediaPlayer = mediaPlayer;
            startVideoPlayback();

            mMediaPlayer.setOnVideoSizeChangedListener(
                    (MediaPlayer player, int width, int height) -> {
                        // Once the size of the video player is known, it is possible to calculate
                        // the correct size of the overlay container and show it. This way the
                        // controls won't briefly appear in the wrong position.
                        syncOverlayControlsSize();
                        mVideoOverlayContainer.setVisibility(View.VISIBLE);
                    });

            if (sProgressCallback != null) {
                mMediaPlayer.setOnInfoListener((MediaPlayer player, int what, int extra) -> {
                    if (what == MediaPlayer.MEDIA_INFO_VIDEO_RENDERING_START) {
                        sProgressCallback.onVideoPlaying();
                        return true;
                    }
                    return false;
                });
            }
        });

        mVideoView.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
            @Override
            public void onCompletion(MediaPlayer mediaPlayer) {
                // Once we reach the completion point, show the overlay controls (without fading
                // away) to indicate that playback has reached the end of the video (and didn't
                // break before reaching the end). This also allows the user to restart playback
                // from the start, by pressing Play.
                switchToPlayButton();
                updateProgress();
                showOverlayControls(/*animateAway=*/false);
                if (sProgressCallback != null) {
                    sProgressCallback.onVideoEnded();
                }
            }
        });
    }

    /**
     * Ends video playback (if a video is playing) and closes the video player. Aborts if the video
     * playback container is not showing.
     * @return true if a video container was showing, false otherwise.
     */
    public boolean closeVideoPlayer() {
        if (getVisibility() != View.VISIBLE) {
            return false;
        }

        setVisibility(View.GONE);
        stopVideoPlayback();
        mVideoView.setMediaController(null);
        mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
        return true;
    }

    public boolean onDoubleTapVideo(MotionEvent e) {
        int videoPos = mMediaPlayer.getCurrentPosition();
        int duration = mMediaPlayer.getDuration();

        // A click to the left (of the center of) the Play button counts as rewinding, and a click
        // to the right of it counts as fast forwarding.
        float x = e.getX();
        float midX = mLargePlayButton.getX() + (mLargePlayButton.getWidth() / 2);
        videoPos += (x > midX) ? SKIP_LENGTH_IN_MS : -SKIP_LENGTH_IN_MS;
        MathUtils.clamp(videoPos, 0, duration);

        videoSeekTo(videoPos);
        return true;
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.video_overlay_container) {
            showOverlayControls(/*animateAway=*/true);
        } else if (id == R.id.video_player_play_button) {
            toggleVideoPlayback();
        } else if (id == R.id.mute) {
            toggleMute();
        } else if (id == R.id.fullscreen) {
            toggleAndroidSystemUiForFullscreen();
        }
    }

    // View.OnSystemUiVisibilityChangeListener:

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
            mDecorView.setOnSystemUiVisibilityChangeListener(null);
            onExitFullScreenMode();

            if (!mFullScreenToggledInApp) {
                // When the user drops out of full screen via a system gesture, such as dragging
                // from the top of the screen, the system sends the visibility change event before
                // the resize has happened, so the new video size isn't known yet. Syncing
                // immediately would make the overlay controls appear in the wrong location.
                getHandler().post(() -> syncOverlayControlsSize());
                return;
            }
        } else {
            onEnterFullScreenMode();
        }

        syncOverlayControlsSize();
        mFullScreenToggledInApp = false;
    }

    // SeekBar.OnSeekBarChangeListener:

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser) {
            final boolean seekDuringPlay = mVideoView.isPlaying();
            mMediaPlayer.setOnSeekCompleteListener(mp -> {
                mMediaPlayer.setOnSeekCompleteListener(null);
                if (seekDuringPlay) {
                    startVideoPlayback();
                }
            });

            float percentage = progress / 100f;
            int position = Math.round(percentage * mVideoView.getDuration());
            videoSeekTo(position);
            updateProgress();
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {
        cancelFadeAwayAnimation();
        mFastForwardMessage.setVisibility(View.VISIBLE);
        mLargePlayButton.setVisibility(View.GONE);
    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        fadeAwayVideoControls();
        mFastForwardMessage.setVisibility(View.GONE);
        mLargePlayButton.setVisibility(View.VISIBLE);
    }

    private void videoSeekTo(int position) {
        if (Build.VERSION.SDK_INT >= 26) {
            mMediaPlayer.seekTo(position, MediaPlayer.SEEK_CLOSEST);
        } else {
            // On older versions, sync to nearest previous key frame.
            mVideoView.seekTo(position);
        }
    }

    private void showOverlayControls(boolean animateAway) {
        cancelFadeAwayAnimation();

        if (animateAway && mVideoView.isPlaying()) {
            fadeAwayVideoControls();
            startPlaybackMonitor();
        }
    }

    private void fadeAwayVideoControls() {
        mVideoOverlayContainer.animate()
                .alpha(0.0f)
                .setStartDelay(3000)
                .setDuration(1000)
                .setListener(new Animator.AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animation) {}

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        enableClickableButtons(false);
                        stopPlaybackMonitor();
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {}

                    @Override
                    public void onAnimationRepeat(Animator animation) {}
                });
    }

    private void cancelFadeAwayAnimation() {
        // Canceling the animation will leave the alpha in the state it had reached while animating,
        // so we need to explicitly set the alpha to 1.0 to reset it.
        mVideoOverlayContainer.animate().cancel();
        mVideoOverlayContainer.setAlpha(1.0f);
        enableClickableButtons(true);
    }

    private void enableClickableButtons(boolean enable) {
        mLargePlayButton.setClickable(enable);
        mMuteButton.setClickable(enable);
        mFullscreenButton.setClickable(enable);
    }

    private void updateProgress() {
        String current;
        String total;
        try {
            current = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getCurrentPosition()));
            total = DecodeVideoTask.formatDuration(Long.valueOf(mVideoView.getDuration()));
        } catch (IllegalStateException exception) {
            // VideoView#getCurrentPosition throws this error if the dialog has been dismissed while
            // waiting to update the status.
            return;
        }

        String formattedProgress =
                mResources.getString(R.string.photo_picker_video_duration, current, total);
        mRemainingTime.setText(formattedProgress);
        mRemainingTime.setContentDescription(
                mResources.getString(R.string.accessibility_playback_time, current, total));
        int percentage = mVideoView.getDuration() == 0
                ? 0
                : mVideoView.getCurrentPosition() * 100 / mVideoView.getDuration();
        mSeekBar.setProgress(percentage);

        if (mVideoView.isPlaying() && mRunPlaybackMonitoringTask) {
            startPlaybackMonitor();
        }
    }

    private void startVideoPlayback() {
        mMediaPlayer.start();
        switchToPauseButton();
        showOverlayControls(/*animateAway=*/true);
    }

    private void stopVideoPlayback() {
        stopPlaybackMonitor();

        mMediaPlayer.pause();
        switchToPlayButton();
        showOverlayControls(/*animateAway=*/false);
    }

    private void toggleVideoPlayback() {
        if (mVideoView.isPlaying()) {
            stopVideoPlayback();
        } else {
            startVideoPlayback();
        }
    }

    private void switchToPlayButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_play_circle_filled_white_24dp);
        mLargePlayButton.setContentDescription(
                mResources.getString(R.string.accessibility_play_video));
    }

    private void switchToPauseButton() {
        mLargePlayButton.setImageResource(R.drawable.ic_pause_circle_outline_white_24dp);
        mLargePlayButton.setContentDescription(
                mResources.getString(R.string.accessibility_pause_video));
    }

    private void syncOverlayControlsSize() {
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                mVideoView.getMeasuredWidth(), mVideoView.getMeasuredHeight());
        mVideoControls.setLayoutParams(params);
    }

    private void toggleMute() {
        mAudioOn = !mAudioOn;
        if (mAudioOn) {
            mMediaPlayer.setVolume(1f, 1f);
            mMuteButton.setImageResource(R.drawable.ic_volume_on_white_24dp);
            mMuteButton.setContentDescription(
                    mResources.getString(R.string.accessibility_mute_video));
        } else {
            mMediaPlayer.setVolume(0f, 0f);
            mMuteButton.setImageResource(R.drawable.ic_volume_off_white_24dp);
            mMuteButton.setContentDescription(
                    mResources.getString(R.string.accessibility_unmute_video));
        }
    }

    private void onEnterFullScreenMode() {
        assert !mFullScreenEnabled;
        mFullscreenButton.setImageResource(R.drawable.ic_full_screen_exit_white_24dp);
        mFullscreenButton.setContentDescription(
                mResources.getString(R.string.accessibility_exit_full_screen));
        mFullScreenEnabled = true;
    }

    private void onExitFullScreenMode() {
        assert mFullScreenEnabled;
        mFullscreenButton.setImageResource(R.drawable.ic_full_screen_white_24dp);
        mFullscreenButton.setContentDescription(
                mResources.getString(R.string.accessibility_full_screen));
        mFullScreenEnabled = false;
    }

    private void toggleAndroidSystemUiForFullscreen() {
        mFullScreenToggledInApp = true;
        if (!mFullScreenEnabled) {
            mDecorView.setOnSystemUiVisibilityChangeListener(this);
            mPreviousSystemUiVisibilityOptions = mDecorView.getSystemUiVisibility();
            mDecorView.setSystemUiVisibility(mPreviousSystemUiVisibilityOptions
                    | View.SYSTEM_UI_FLAG_IMMERSIVE | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        } else {
            mDecorView.setSystemUiVisibility(mPreviousSystemUiVisibilityOptions);
        }

        // Calling setSystemUiVisibility will result in Android showing/hiding its system UI to go
        // into or out of full screen mode. This happens asynchronously and once that change is
        // complete the video player needs to respond to those changes. Flow therefore continues in
        // onSystemUiVisibilityChange, which the system calls when it is done.
    }

    private void startPlaybackMonitor() {
        mRunPlaybackMonitoringTask = true;
        startPlaybackMonitorTask();
    }

    private void startPlaybackMonitorTask() {
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> updateProgress(), 250);
    }

    private void stopPlaybackMonitor() {
        mRunPlaybackMonitoringTask = false;
    }

    /** Sets the video playback progress callback. For testing use only. */
    @VisibleForTesting
    public static void setProgressCallback(VideoPlaybackStatusCallback callback) {
        sProgressCallback = callback;
    }
}
