(function () {
  'use strict';

  var MAX_SPEED = 104;
  var PROFILE_POWER = 1.6;
  var TARGETS = { '0-100': 100, '0-60': 60, 'speedo': 0 };
  var GPS_LAG_MS = 300;
  var LIFT_MARGIN_MS = 1200;
  var STOP_SETTLE_MS = 1500;
  var BRAKE_G = 1.0;
  var G = 9.81;

  function xForTarget(target) {
    return 1 - Math.pow(1 - target / MAX_SPEED, 1 / PROFILE_POWER);
  }

  var X_100 = xForTarget(100);
  var X_60 = xForTarget(60);

  function xFor(mode) {
    return mode === '0-60' ? X_60 : X_100;
  }

  var sources = [];
  var nextPingAt = 0;
  var stoppedSince = Date.now();
  var drive = null;
  var model = {
    stage: 'SEARCHING_GPS',
    mode: '0-100',
    satellites: 0,
    gpsOn: true,
    calibrating: false,
    calibratingUntil: 0,
    nextSatelliteAt: 0,
    lastResultSeconds: null,
    nextId: 1,
    runs: []
  };

  function round(value, digits) {
    var factor = Math.pow(10, digits);
    return Math.round(value * factor) / factor;
  }

  function profileSpeed(run, t) {
    var x = Math.min(Math.max(t / run.T, 0), 1);
    return MAX_SPEED * (1 - Math.pow(1 - x, PROFILE_POWER));
  }

  function profileAccel(run, t) {
    var x = Math.min(Math.max(t / run.T, 0), 1);
    var dvdt = (MAX_SPEED * PROFILE_POWER) / run.T * Math.pow(1 - x, PROFILE_POWER - 1);
    return dvdt / 3.6 / 9.81;
  }

  function makeSamples(run, duration) {
    var samples = [];
    for (var t = 0; t <= duration + 0.001; t += 0.1) {
      samples.push({
        t: round(t, 1),
        s: round(profileSpeed(run, t), 1),
        a: round(profileAccel(run, t), 2)
      });
    }
    return samples;
  }

  function makeRunEntry(id, resultSeconds, date, mode) {
    mode = mode || '0-100';
    var run = { T: resultSeconds / xFor(mode) };
    return {
      id: id,
      resultSeconds: resultSeconds,
      date: date,
      mode: mode,
      samples: makeSamples(run, resultSeconds + 0.5)
    };
  }

  function fakeDate() {
    var now = new Date();
    function pad(value) {
      return (value < 10 ? '0' : '') + value;
    }
    return now.getUTCFullYear() + '-' + pad(now.getUTCMonth() + 1) + '-' + pad(now.getUTCDate()) + ' ' +
      pad(now.getUTCHours()) + ':' + pad(now.getUTCMinutes()) + ':' + pad(now.getUTCSeconds());
  }

  function dispatch(type, data) {
    sources = sources.filter(function (source) {
      return !source.closed;
    });
    sources.forEach(function (source) {
      source.dispatch(type, data);
    });
  }

  function FakeEventSource(url) {
    this.url = url;
    this.listeners = {};
    this.closed = false;
    sources.push(this);
  }

  FakeEventSource.prototype.addEventListener = function (type, listener) {
    if (!this.listeners[type]) {
      this.listeners[type] = [];
    }
    this.listeners[type].push(listener);
  };

  FakeEventSource.prototype.removeEventListener = function (type, listener) {
    var list = this.listeners[type] || [];
    var index = list.indexOf(listener);
    if (index >= 0) {
      list.splice(index, 1);
    }
  };

  FakeEventSource.prototype.close = function () {
    this.closed = true;
  };

  FakeEventSource.prototype.dispatch = function (type, data) {
    (this.listeners[type] || []).slice().forEach(function (listener) {
      try {
        listener.call(this, { type: type, data: data });
      } catch (error) {
        console.error(error);
      }
    }, this);
  };

  function benchResult(result) {
    var entry = {
      id: model.nextId++,
      resultSeconds: round(result.resultSeconds, 2),
      date: fakeDate(),
      mode: result.mode,
      samples: result.samples.map(function (sample) {
        return { t: round(sample.t, 1), s: round(sample.s, 1), a: round(sample.a, 2) };
      })
    };
    model.runs.unshift(entry);
    model.lastResultSeconds = entry.resultSeconds;
    updateStatus();
    dispatch('result', JSON.stringify({
      id: entry.id,
      resultSeconds: entry.resultSeconds,
      date: entry.date,
      mode: entry.mode
    }));
  }

  var bench = BenchLogic.createBench({ onResult: benchResult });

  function carSpeedAt(t) {
    if (!drive) {
      return 0;
    }
    if (drive.phase === 'pull') {
      return t < drive.startAt ? 0 : profileSpeed(drive.run, (t - drive.startAt) / 1000);
    }
    if (drive.phase === 'brake') {
      return Math.max(0, drive.fromSpeed - (BRAKE_G * G * 3.6 * (t - drive.startAt)) / 1000);
    }
    return 0;
  }

  function carAccelAt(t) {
    if (!drive) {
      return 0;
    }
    if (drive.phase === 'pull' && t >= drive.startAt && t < drive.endAt) {
      return profileAccel(drive.run, (t - drive.startAt) / 1000);
    }
    if (drive.phase === 'brake' && carSpeedAt(t) > 0) {
      return BRAKE_G;
    }
    return 0;
  }

  function advanceDrive(now) {
    if (drive && drive.phase === 'pull' && now >= drive.endAt) {
      var endAt = drive.endAt;
      drive = {
        phase: 'brake',
        startAt: endAt,
        fromSpeed: profileSpeed(drive.run, (endAt - drive.startAt) / 1000)
      };
    }
    if (drive && drive.phase === 'brake' && carSpeedAt(now) <= 0) {
      drive = null;
    }
    if (carSpeedAt(now) > 0) {
      stoppedSince = null;
    } else if (stoppedSince === null) {
      stoppedSince = now;
    }
  }

  function canLaunch() {
    return model.stage === 'READY' &&
      bench.getStage() === 'READY' &&
      bench.getMode() !== 'speedo' &&
      !drive &&
      stoppedSince !== null &&
      Date.now() - stoppedSince >= STOP_SETTLE_MS;
  }

  function startPull() {
    if (!canLaunch()) {
      return;
    }
    var now = Date.now();
    var mode = bench.getMode();
    var targetSeconds = mode === '0-60' ? 2.9 + Math.random() * 0.3 : 6.9 + Math.random() * 0.4;
    model.calibrating = false;
    drive = {
      phase: 'pull',
      startAt: now,
      endAt: now + targetSeconds * 1000 + LIFT_MARGIN_MS,
      run: { T: targetSeconds / xFor(mode) }
    };
  }

  function brakeCar() {
    var now = Date.now();
    var speed = carSpeedAt(now);
    drive = speed > 0 ? { phase: 'brake', startAt: now, fromSpeed: speed } : null;
  }

  function startCalibration() {
    if (bench.getStage() !== 'READY') {
      return;
    }
    model.calibrating = true;
    model.calibratingUntil = Date.now() + 1800;
  }

  function clearRuns() {
    model.runs = [];
    model.nextId = 1;
    model.lastResultSeconds = null;
    updateStatus();
  }

  function toggleGps() {
    model.gpsOn = !model.gpsOn;
    model.nextSatelliteAt = 0;
    if (!model.gpsOn) {
      model.stage = 'SEARCHING_GPS';
    }
    updateStatus();
  }

  function stateJson() {
    return {
      stage: bench.getStage() !== 'READY' ? bench.getStage() : model.stage,
      mode: model.mode,
      speedKmh: round(bench.getSpeedKmh(), 1),
      satellites: model.satellites,
      calibrating: model.calibrating,
      elapsedSeconds: round(bench.getElapsedSeconds(Date.now()), 2),
      lastResultSeconds: model.lastResultSeconds
    };
  }

  function step(now) {
    if (model.calibrating && now >= model.calibratingUntil) {
      model.calibrating = false;
    }

    if (now >= model.nextSatelliteAt) {
      if (model.gpsOn && model.satellites < 11) {
        model.satellites++;
        model.nextSatelliteAt = now + 1200;
      } else if (!model.gpsOn && model.satellites > 0) {
        model.satellites--;
        model.nextSatelliteAt = now + 200;
      }
    }

    if (model.stage === 'SEARCHING_GPS') {
      if (model.gpsOn && model.satellites >= 4 && !model.calibrating) {
        model.stage = 'CALIBRATING_FIRST';
        startCalibration();
      }
    } else if (model.stage === 'CALIBRATING_FIRST' && !model.calibrating) {
      model.stage = 'READY';
    }

    advanceDrive(now);

    bench.tick(now, carAccelAt(now), model.gpsOn ? carSpeedAt(now - GPS_LAG_MS) : null);

    updateStatus();
  }

  function jsonResponse(status, payload) {
    return new Response(JSON.stringify(payload), {
      status: status,
      headers: { 'Content-Type': 'application/json' }
    });
  }

  window.fetch = function (input, init) {
    var url = typeof input === 'string' ? input : (input && input.url) || '';
    var method = ((init && init.method) || (input && input.method) || 'GET').toUpperCase();
    var path = url.replace(/^https?:\/\/[^/]+/, '').split('?')[0];

    if (method === 'GET' && path === '/api/state') {
      return Promise.resolve(jsonResponse(200, stateJson()));
    }

    if (method === 'GET' && path === '/api/runs') {
      return Promise.resolve(jsonResponse(200, model.runs.map(function (run) {
        return { id: run.id, resultSeconds: run.resultSeconds, date: run.date, mode: run.mode };
      })));
    }

    if (method === 'GET' && path.indexOf('/api/runs/') === 0) {
      var id = parseInt(path.slice('/api/runs/'.length), 10);
      var run = model.runs.find(function (entry) {
        return entry.id === id;
      });
      if (!run) {
        return Promise.resolve(jsonResponse(404, { error: 'not found' }));
      }
      return Promise.resolve(jsonResponse(200, {
        id: run.id,
        gpsDateTime: run.date || null,
        resultSeconds: run.resultSeconds,
        mode: run.mode,
        samples: run.samples
      }));
    }

    if (method === 'POST' && path === '/api/recalibrate') {
      startCalibration();
      return Promise.resolve(jsonResponse(200, { ok: true }));
    }

    if (method === 'POST' && path.indexOf('/api/mode/') === 0) {
      var modeValue = path.slice('/api/mode/'.length);
      if (!Object.prototype.hasOwnProperty.call(TARGETS, modeValue)) {
        return Promise.resolve(jsonResponse(400, { error: 'invalid mode' }));
      }
      bench.setMode(modeValue);
      model.mode = modeValue;
      updateStatus();
      return Promise.resolve(jsonResponse(200, { ok: true, mode: modeValue }));
    }

    return Promise.resolve(new Response('not found', { status: 404 }));
  };

  window.EventSource = FakeEventSource;

  function updateStatus() {
    var status = document.getElementById('mock-status');
    if (status) {
      status.textContent = model.mode + ' · ' + model.runs.length + ' run(s) · ' +
        Math.round(bench.getSpeedKmh()) + ' km/h' + (canLaunch() ? ' · ready' : '');
    }
    var gpsButton = document.getElementById('mock-gps');
    if (gpsButton) {
      gpsButton.textContent = model.gpsOn ? 'GPS: on' : 'GPS: off';
    }
  }

  function buildPanel() {
    var style = document.createElement('style');
    style.textContent =
      '#app { bottom: 56px; }' +
      '#mock-panel { position: fixed; left: 0; right: 0; bottom: 0; height: 56px; display: flex;' +
      ' align-items: center; gap: 6px; padding: 0 10px; background: #111; border-top: 1px solid #444;' +
      ' overflow-x: auto; z-index: 10; font-family: system-ui, sans-serif; }' +
      '#mock-panel .mock-tag { color: #888; font-size: 10px; letter-spacing: 1px; margin-right: 4px; white-space: nowrap; }' +
      '#mock-panel button { background: #000; color: #ddd; border: 1px solid #555; border-radius: 8px;' +
      ' padding: 8px 10px; font: inherit; font-size: 12px; white-space: nowrap; cursor: pointer; }' +
      '#mock-panel button:active { background: #333; }' +
      '#mock-status { color: #777; font-size: 11px; white-space: nowrap; margin-left: auto; }';
    document.head.appendChild(style);

    var panel = document.createElement('div');
    panel.id = 'mock-panel';

    var tag = document.createElement('span');
    tag.className = 'mock-tag';
    tag.textContent = 'TEST MOCK';
    panel.appendChild(tag);

    [
      ['mock-launch', 'Launch', startPull],
      ['mock-abort', 'Abort run', brakeCar],
      ['mock-recal', 'Recalibrate', startCalibration],
      ['mock-gps', 'GPS: on', toggleGps],
      ['mock-clear', 'Clear runs', clearRuns]
    ].forEach(function (definition) {
      var button = document.createElement('button');
      button.id = definition[0];
      button.textContent = definition[1];
      button.addEventListener('click', definition[2]);
      panel.appendChild(button);
    });

    var status = document.createElement('span');
    status.id = 'mock-status';
    panel.appendChild(status);

    document.body.appendChild(panel);
    updateStatus();
  }

  model.runs = [
    makeRunEntry(3, 6.87, '', '0-100'),
    makeRunEntry(2, 6.38, '2026-09-28 18:47:09', '0-100'),
    makeRunEntry(1, 6.62, '2026-09-21 08:15:32', '0-100')
  ];
  model.nextId = 4;
  model.lastResultSeconds = 6.87;

  buildPanel();

  var tickCount = 0;
  setInterval(function () {
    tickCount++;
    var now = Date.now();
    step(now);
    if (tickCount % 2 === 0) {
      dispatch('state', JSON.stringify(stateJson()));
    }
    if (now >= nextPingAt) {
      nextPingAt = now + 1000;
      dispatch('ping', '{}');
    }
  }, 100);
})();
