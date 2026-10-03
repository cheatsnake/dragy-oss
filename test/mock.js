(function () {
  'use strict';

  var MAX_SPEED = 104;
  var PROFILE_POWER = 1.6;
  var X_100 = 1 - Math.pow(1 - 100 / MAX_SPEED, 1 / PROFILE_POWER);

  var sources = [];
  var nextPingAt = 0;
  var model = {
    stage: 'SEARCHING_GPS',
    satellites: 0,
    gpsOn: true,
    speedKmh: 0,
    calibrating: false,
    calibratingUntil: 0,
    nextSatelliteAt: 0,
    lastResultSeconds: null,
    nextId: 1,
    runs: [],
    run: null
  };

  function round(value, digits) {
    var factor = Math.pow(10, digits);
    return Math.round(value * factor) / factor;
  }

  function profileSpeed(run, t) {
    var x = Math.min(Math.max(t / run.T, 0), 1);
    return MAX_SPEED * (1 - Math.pow(1 - x, PROFILE_POWER));
  }

  function profileTime(run, speed) {
    return run.T * (1 - Math.pow(1 - speed / MAX_SPEED, 1 / PROFILE_POWER));
  }

  function profileAccel(run, t) {
    var x = Math.min(Math.max(t / run.T, 0), 1);
    var dvdt = MAX_SPEED * PROFILE_POWER / run.T * Math.pow(1 - x, PROFILE_POWER - 1);
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

  function makeRunEntry(id, resultSeconds, date) {
    var run = { T: resultSeconds / X_100 };
    return {
      id: id,
      resultSeconds: resultSeconds,
      date: date,
      samples: makeSamples(run, resultSeconds + 0.5)
    };
  }

  function fakeDate() {
    var now = new Date();
    function pad(value) {
      return (value < 10 ? '0' : '') + value;
    }
    return now.getFullYear() + '-' + pad(now.getMonth() + 1) + '-' + pad(now.getDate()) + ' ' +
      pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds());
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

  function stageName() {
    if (model.run) {
      return model.run.phase === 'ARMING' ? 'BENCH_ARMING' : 'BENCH_RUNNING';
    }
    return model.stage;
  }

  function stateJson() {
    return {
      stage: stageName(),
      speedKmh: round(model.speedKmh, 1),
      satellites: model.satellites,
      calibrating: model.calibrating,
      elapsedSeconds: model.run ? round((Date.now() - model.run.launchAt) / 1000, 2) : 0,
      lastResultSeconds: model.lastResultSeconds
    };
  }

  function startCalibration() {
    if (model.run) {
      return;
    }
    model.calibrating = true;
    model.calibratingUntil = Date.now() + 1800;
  }

  function startRun() {
    if (model.run) {
      return;
    }
    var now = Date.now();
    model.speedKmh = 0;
    model.calibrating = false;
    model.run = {
      launchAt: now - 400,
      armUntil: now + 600,
      phase: 'ARMING',
      T: 6.9 + Math.random() * 0.5,
      samples: [],
      nextSampleAt: now - 400
    };
  }

  function abortRun() {
    if (!model.run) {
      return;
    }
    model.run = null;
    model.speedKmh = 0;
    model.stage = 'READY';
  }

  function finishRun() {
    var run = model.run;
    var resultSeconds = round(profileTime(run, 100), 2);
    var entry = {
      id: model.nextId++,
      resultSeconds: resultSeconds,
      date: fakeDate(),
      samples: run.samples
    };
    model.runs.unshift(entry);
    model.lastResultSeconds = resultSeconds;
    model.run = null;
    model.stage = 'READY';
    model.speedKmh = 100;
    updateStatus();
    dispatch('result', JSON.stringify({
      id: entry.id,
      resultSeconds: resultSeconds,
      date: entry.date
    }));
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
    if (!model.gpsOn && !model.run) {
      model.stage = 'SEARCHING_GPS';
    }
    updateStatus();
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

    if (!model.run) {
      if (model.stage === 'SEARCHING_GPS') {
        if (model.gpsOn && model.satellites >= 4 && !model.calibrating) {
          model.stage = 'CALIBRATING_FIRST';
          startCalibration();
        }
      } else if (model.stage === 'CALIBRATING_FIRST' && !model.calibrating) {
        model.stage = 'READY';
      }
      if (model.speedKmh > 0) {
        model.speedKmh = Math.max(0, model.speedKmh - 12);
      }
      return;
    }

    var t = (now - model.run.launchAt) / 1000;
    model.speedKmh = profileSpeed(model.run, t);

    while (model.run.nextSampleAt <= now && model.run.samples.length < 400) {
      var sampleT = (model.run.nextSampleAt - model.run.launchAt) / 1000;
      model.run.samples.push({
        t: round(sampleT, 1),
        s: round(profileSpeed(model.run, sampleT), 1),
        a: round(profileAccel(model.run, sampleT), 2)
      });
      model.run.nextSampleAt += 100;
    }

    if (model.run.phase === 'ARMING' && now >= model.run.armUntil) {
      model.run.phase = 'RUNNING';
    }

    if (model.speedKmh >= 100) {
      finishRun();
    }
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
        return { id: run.id, resultSeconds: run.resultSeconds, date: run.date };
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
        samples: run.samples
      }));
    }

    if (method === 'POST' && path === '/api/recalibrate') {
      startCalibration();
      return Promise.resolve(jsonResponse(200, { ok: true }));
    }

    return Promise.resolve(new Response('not found', { status: 404 }));
  };

  window.EventSource = FakeEventSource;

  function updateStatus() {
    var status = document.getElementById('mock-status');
    if (status) {
      status.textContent = model.runs.length + ' run(s)';
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
      ['mock-launch', 'Launch', startRun],
      ['mock-abort', 'Abort run', abortRun],
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
    makeRunEntry(3, 6.87, ''),
    makeRunEntry(2, 6.38, '2026-09-28 18:47:09'),
    makeRunEntry(1, 6.62, '2026-09-21 08:15:32')
  ];
  model.nextId = 4;
  model.lastResultSeconds = 6.87;

  buildPanel();

  setInterval(function () {
    var now = Date.now();
    step(now);
    dispatch('state', JSON.stringify(stateJson()));
    if (now >= nextPingAt) {
      nextPingAt = now + 1000;
      dispatch('ping', '{}');
    }
  }, 200);
})();
