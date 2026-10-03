(function (root, factory) {
  var api = factory();
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = api;
  }
  root.BenchLogic = api;
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // 1:1 JavaScript port of the run detection pipeline in src/accelerometer.cpp,
  // src/gps.cpp and the stage logic in src/main.cpp. Used by test/detect.test.js
  // and injected into test/mock.js. Keep in lockstep with the firmware.

  var ACCELEROMETER_HISTORY_SIZE = 16;
  var SPEED_HISTORY_SIZE = 12;
  var LAUNCH_CONFIRM_SAMPLES = 4;
  var MOTION_START_THRESHOLD_G = 0.2;
  var MOTION_STOP_THRESHOLD_G = 0.1;
  var BUMP_FILTER_ALPHA = 0.2;
  var LAUNCH_MAX_START_SPEED_KMH = 3.0;
  var LAUNCH_CONFIRM_SPEED_DELTA_KMH = 4.0;
  var LAUNCH_CONFIRM_WINDOW_MS = 3000;
  var BENCH_STALL_TIMEOUT_MS = 3000;
  var BENCH_MIN_PROGRESS_KMH = 0.5;
  var BENCH_ABORT_SPEED_DROP_KMH = 3.0;
  var RESET_SPEED_KMH = 3.0;
  var ACCELEROMETER_INTERVAL_MS = 100;
  var RUN_SAMPLE_INTERVAL_MS = 100;
  var RUN_MAX_SAMPLES = 400;

  var TARGET_SPEEDS = { '0-100': 100, '0-60': 60, 'speedo': 0 };

  function Ring(size) {
    this.size = size;
    this.items = [];
    this.index = 0;
    this.count = 0;
  }

  Ring.prototype.push = function (item) {
    this.items[this.index] = item;
    this.index = (this.index + 1) % this.size;
    if (this.count < this.size) {
      this.count++;
    }
  };

  Ring.prototype.at = function (i) {
    var oldest = this.count < this.size ? 0 : this.index;
    return this.items[(oldest + i) % this.size];
  };

  function createBench(options) {
    var listeners = options || {};

    var accelHistory = new Ring(ACCELEROMETER_HISTORY_SIZE);
    var speedHistory = new Ring(SPEED_HISTORY_SIZE);

    var filteredAccelerationG = 0;
    var motionActive = false;

    var mode = '0-100';
    var stage = 'READY';
    var benchStartMillis = 0;
    var armStartMillis = 0;
    var speedAtArmKmh = 0;
    var peakSpeedKmh = 0;
    var stallCheckStartMillis = 0;
    var stallCheckStartSpeedKmh = 0;

    var lastAccelMillis = null;
    var lastSpeedMillis = null;

    var recordingActive = false;
    var recordingLaunchMillis = 0;
    var recordingNextMillis = 0;
    var recordedSamples = [];

    function emitStage(nextStage, info) {
      if (nextStage === stage) {
        return;
      }
      stage = nextStage;
      if (listeners.onStageChange) {
        listeners.onStageChange(stage, info || {});
      }
    }

    function measures() {
      return mode !== 'speedo';
    }

    function targetSpeedKmh() {
      return TARGET_SPEEDS[mode] || 0;
    }

    function pushAccelSample(t, horizontalG) {
      accelHistory.push({ t: t, g: horizontalG });
    }

    function pushSpeedSample(t, speedKmh) {
      speedHistory.push({ t: t, v: speedKmh });
    }

    function accelerationAtMillis(t) {
      for (var i = accelHistory.count - 1; i >= 0; i--) {
        var sample = accelHistory.at(i);
        if (sample.t <= t) {
          return sample.g;
        }
      }
      return null;
    }

    function speedAtMillis(t) {
      for (var i = speedHistory.count - 1; i >= 0; i--) {
        var sample = speedHistory.at(i);
        if (sample.t <= t) {
          return sample.v;
        }
      }
      return null;
    }

    function oldestSpeedKmh() {
      return speedHistory.count === 0 ? null : speedHistory.at(0).v;
    }

    function getSpeedKmh() {
      return speedHistory.count === 0 ? 0 : speedHistory.at(speedHistory.count - 1).v;
    }

    function updateMotionState() {
      if (!motionActive) {
        if (filteredAccelerationG > MOTION_START_THRESHOLD_G) {
          motionActive = true;
        }
        return;
      }
      if (filteredAccelerationG < MOTION_STOP_THRESHOLD_G) {
        motionActive = false;
      }
    }

    function detectLaunchStart() {
      if (accelHistory.count < LAUNCH_CONFIRM_SAMPLES) {
        return null;
      }

      for (var i = 1; i <= LAUNCH_CONFIRM_SAMPLES; i++) {
        if (accelHistory.at(accelHistory.count - i).g <= MOTION_START_THRESHOLD_G) {
          return null;
        }
      }

      var launchIndex = accelHistory.count - LAUNCH_CONFIRM_SAMPLES;
      while (launchIndex > 0 && accelHistory.at(launchIndex - 1).g >= MOTION_STOP_THRESHOLD_G) {
        launchIndex--;
      }

      return accelHistory.at(launchIndex).t;
    }

    function speedCrossingMillis(target) {
      for (var i = 1; i < speedHistory.count; i++) {
        var previous = speedHistory.at(i - 1);
        var current = speedHistory.at(i);

        if (previous.v < target && current.v >= target) {
          var span = current.v - previous.v;
          if (span <= 0) {
            return current.t;
          }
          var fraction = (target - previous.v) / span;
          return previous.t + fraction * (current.t - previous.t);
        }
      }

      return null;
    }

    function startRecording(launchStartMillis) {
      recordingLaunchMillis = launchStartMillis;
      recordingNextMillis = launchStartMillis;
      recordedSamples = [];
      recordingActive = true;
    }

    function updateRecording(now) {
      if (!recordingActive) {
        return;
      }

      while (recordingActive && recordingNextMillis <= now) {
        var speed = speedAtMillis(recordingNextMillis);
        var acceleration = accelerationAtMillis(recordingNextMillis);

        if (speed !== null || acceleration !== null) {
          if (recordedSamples.length >= RUN_MAX_SAMPLES) {
            recordingActive = false;
            break;
          }
          recordedSamples.push({
            t: (recordingNextMillis - recordingLaunchMillis) / 1000,
            s: speed === null ? 0 : speed,
            a: acceleration === null ? 0 : acceleration
          });
        }

        recordingNextMillis += RUN_SAMPLE_INTERVAL_MS;
      }
    }

    function startRun(launchStartMillis, now) {
      benchStartMillis = launchStartMillis;
      speedAtArmKmh = getSpeedKmh();
      peakSpeedKmh = speedAtArmKmh;
      stallCheckStartSpeedKmh = speedAtArmKmh;
      armStartMillis = now;
      stallCheckStartMillis = now;
      motionActive = true;
      startRecording(launchStartMillis);
      emitStage('BENCH_ARMING', { benchStartMillis: benchStartMillis });
    }

    function resetRun() {
      recordingActive = false;
      recordedSamples = [];
      emitStage('READY', {});
    }

    function finishRun(now) {
      recordingActive = false;

      var crossing = speedCrossingMillis(targetSpeedKmh());
      var resultSeconds =
        crossing !== null && crossing > benchStartMillis
          ? (crossing - benchStartMillis) / 1000
          : (now - benchStartMillis) / 1000;

      var samples = recordedSamples;
      recordedSamples = [];

      if (listeners.onResult) {
        listeners.onResult({ resultSeconds: resultSeconds, mode: mode, samples: samples });
      }

      emitStage('READY', {});
    }

    function runStage(now) {
      if (stage === 'READY') {
        if (!measures()) {
          return;
        }

        var launchStartMillis = detectLaunchStart();
        if (launchStartMillis === null) {
          return;
        }

        var speedAtLaunch = speedAtMillis(launchStartMillis);
        if (speedAtLaunch === null) {
          speedAtLaunch = oldestSpeedKmh();
        }
        if (speedAtLaunch !== null && speedAtLaunch > LAUNCH_MAX_START_SPEED_KMH) {
          return;
        }

        startRun(launchStartMillis, now);
        return;
      }

      if (stage === 'BENCH_ARMING') {
        var armingSpeed = getSpeedKmh();
        if (armingSpeed > peakSpeedKmh) {
          peakSpeedKmh = armingSpeed;
        }

        if (armingSpeed >= targetSpeedKmh()) {
          finishRun(now);
          return;
        }

        if (armingSpeed - speedAtArmKmh >= LAUNCH_CONFIRM_SPEED_DELTA_KMH) {
          stallCheckStartMillis = now;
          stallCheckStartSpeedKmh = armingSpeed;
          emitStage('BENCH_RUNNING', {});
          return;
        }

        if (now - armStartMillis >= LAUNCH_CONFIRM_WINDOW_MS) {
          resetRun();
          return;
        }

        if (!motionActive) {
          resetRun();
        }
        return;
      }

      if (stage === 'BENCH_RUNNING') {
        var runningSpeed = getSpeedKmh();
        if (runningSpeed >= targetSpeedKmh()) {
          finishRun(now);
          return;
        }

        if (runningSpeed > peakSpeedKmh) {
          peakSpeedKmh = runningSpeed;
        }

        if (now - stallCheckStartMillis >= BENCH_STALL_TIMEOUT_MS) {
          if (runningSpeed - stallCheckStartSpeedKmh >= BENCH_MIN_PROGRESS_KMH) {
            stallCheckStartMillis = now;
            stallCheckStartSpeedKmh = runningSpeed;
          } else {
            resetRun();
            return;
          }
        }

        if (runningSpeed < peakSpeedKmh - BENCH_ABORT_SPEED_DROP_KMH) {
          resetRun();
          return;
        }

        if (runningSpeed <= RESET_SPEED_KMH && !motionActive) {
          resetRun();
        }
      }
    }

    function tick(now, rawG, gpsSpeedKmh) {
      if (lastAccelMillis === null || now - lastAccelMillis >= ACCELEROMETER_INTERVAL_MS) {
        lastAccelMillis = now;
        pushAccelSample(now, rawG);
        filteredAccelerationG =
          rawG * BUMP_FILTER_ALPHA + filteredAccelerationG * (1 - BUMP_FILTER_ALPHA);
        updateMotionState();
      }

      if (
        gpsSpeedKmh !== null &&
        gpsSpeedKmh !== undefined &&
        (lastSpeedMillis === null || now - lastSpeedMillis >= RUN_SAMPLE_INTERVAL_MS)
      ) {
        lastSpeedMillis = now;
        pushSpeedSample(now, gpsSpeedKmh);
      }

      runStage(now);
      updateRecording(now);
    }

    return {
      tick: tick,
      setMode: function (nextMode) {
        if (!Object.prototype.hasOwnProperty.call(TARGET_SPEEDS, nextMode)) {
          return;
        }
        if (stage !== 'READY') {
          resetRun();
        }
        mode = nextMode;
      },
      getMode: function () {
        return mode;
      },
      getStage: function () {
        return stage;
      },
      getSpeedKmh: getSpeedKmh,
      getElapsedSeconds: function (now) {
        return stage === 'BENCH_ARMING' || stage === 'BENCH_RUNNING'
          ? (now - benchStartMillis) / 1000
          : 0;
      }
    };
  }

  return { createBench: createBench };
});
