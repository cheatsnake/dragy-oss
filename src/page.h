#ifndef PAGE_H
#define PAGE_H

#include <Arduino.h>

static const char INDEX_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#000000">
<title>Dragy</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body {
  background: #000;
  color: #fff;
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  -webkit-user-select: none;
  user-select: none;
  -webkit-tap-highlight-color: transparent;
}
#app {
  position: fixed;
  inset: 0;
  display: flex;
  flex-direction: column;
  padding: calc(12px + env(safe-area-inset-top)) calc(16px + env(safe-area-inset-right)) calc(12px + env(safe-area-inset-bottom)) calc(16px + env(safe-area-inset-left));
}
.view { display: flex; flex-direction: column; flex: 1; min-height: 0; }
.view[hidden] { display: none; }
.topbar { display: flex; align-items: center; justify-content: space-between; min-height: 32px; }
.sats { font-size: 18px; color: #bbb; font-variant-numeric: tabular-nums; }
.satlabel { font-size: 12px; color: #666; }
.caldot { width: 14px; height: 14px; border-radius: 50%; background: #fff; animation: pulse 1s ease-in-out infinite alternate; }
.caldot[hidden] { display: none; }
@keyframes pulse { from { opacity: 1; } to { opacity: 0.15; } }
.modes { display: flex; gap: 4px; padding: 2px; border: 1px solid #444; border-radius: 10px; }
.modebtn { padding: 5px 9px; background: none; border: 0; border-radius: 7px; color: #888; font: inherit; font-size: 13px; cursor: pointer; }
.modebtn.active { background: #fff; color: #000; }
.center { flex: 1; display: flex; align-items: center; justify-content: center; min-height: 0; }
.big { font-size: clamp(72px, 30vw, 150px); font-weight: 700; font-variant-numeric: tabular-nums; line-height: 1; }
.big .unit { font-size: 0.22em; font-weight: 400; color: #888; margin-left: 0.3em; }
.message { font-size: clamp(24px, 8vw, 40px); color: #ddd; text-align: center; }
.bottomrow { display: flex; align-items: flex-end; justify-content: space-between; gap: 12px; min-height: 34px; }
.actions { display: flex; gap: 12px; }
.timer { font-size: 22px; color: #bbb; font-variant-numeric: tabular-nums; }
.timer .unit { font-size: 12px; color: #666; margin-left: 3px; }
.btn {
  display: block;
  width: 100%;
  padding: 14px;
  margin-top: 12px;
  background: #000;
  color: #fff;
  border: 1px solid #fff;
  border-radius: 12px;
  font: inherit;
  font-size: 17px;
  cursor: pointer;
}
.btn:active { background: #fff; color: #000; }
.btn.ghost { width: auto; padding: 8px 14px; margin: 0; border-color: #444; color: #bbb; font-size: 15px; }
.btn.ghost:active { background: #222; color: #fff; }
.title { font-size: 18px; color: #bbb; }
.spacer { width: 70px; }
.runlist { flex: 1; overflow-y: auto; margin-top: 8px; -webkit-overflow-scrolling: touch; }
.runrow {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
  width: 100%;
  padding: 16px 4px;
  background: none;
  border: 0;
  border-bottom: 1px solid #333;
  color: #fff;
  font: inherit;
  text-align: left;
  cursor: pointer;
}
.runrow:active { background: #111; }
.runresult { font-size: 24px; font-weight: 600; font-variant-numeric: tabular-nums; }
.runresult .unit { font-size: 14px; font-weight: 400; color: #888; margin-left: 2px; }
.runmode { font-size: 11px; font-weight: 400; color: #888; border: 1px solid #444; border-radius: 6px; padding: 1px 6px; margin-left: 8px; vertical-align: middle; }
.rundate { font-size: 13px; color: #888; }
.empty { color: #666; text-align: center; padding: 32px 0; }
#detail { overflow-y: auto; }
.detailhead { text-align: center; padding: 8px 0 4px; }
.detailhead .big { font-size: clamp(56px, 22vw, 110px); }
.detailmode { display: inline-block; font-size: 12px; color: #888; border: 1px solid #444; border-radius: 6px; padding: 2px 8px; margin-top: 8px; }
.detailmode[hidden] { display: none; }
.date { font-size: 14px; color: #888; margin-top: 6px; }
.chartbox { margin-top: 16px; }
.charttitle { font-size: 14px; color: #999; margin-bottom: 4px; }
canvas { display: block; width: 100%; height: 200px; }
</style>
</head>
<body>
<div id="app">

  <div id="live" class="view">
    <div class="topbar">
      <div id="sats" class="sats"></div>
      <div id="modes" class="modes">
        <button class="modebtn" data-mode="0-100">0&ndash;100</button>
        <button class="modebtn" data-mode="0-60">0&ndash;60</button>
        <button class="modebtn" data-mode="speedo">Speed</button>
      </div>
      <div id="caldot" class="caldot" hidden></div>
    </div>
    <div class="center">
      <div id="message" class="message" hidden>Searching signal...</div>
      <div id="bigvalue" class="big" hidden><span id="bigtext">0</span><span id="bigunit" class="unit"></span></div>
    </div>
    <div class="bottomrow">
      <div class="actions">
        <button id="historybtn" class="btn ghost">History</button>
        <button id="recalbtn" class="btn ghost">Recalibrate</button>
      </div>
      <div id="timer" class="timer"></div>
    </div>
  </div>

  <div id="history" class="view" hidden>
    <div class="topbar">
      <button id="backlive" class="btn ghost">&larr; Live</button>
      <div class="title">History</div>
      <div class="spacer"></div>
    </div>
    <div id="runlist" class="runlist"></div>
  </div>

  <div id="detail" class="view" hidden>
    <div class="topbar">
      <button id="backhistory" class="btn ghost">&larr; History</button>
      <div></div>
    </div>
    <div class="detailhead">
      <div class="big"><span id="dresult">0.00</span><span class="unit">s</span></div>
      <div id="dmode" class="detailmode" hidden></div>
      <div id="ddate" class="date"></div>
    </div>
    <div class="chartbox">
      <div class="charttitle">Speed, km/h</div>
      <canvas id="speedchart"></canvas>
    </div>
    <div class="chartbox">
      <div class="charttitle">Acceleration, g</div>
      <canvas id="accelchart"></canvas>
    </div>
  </div>

</div>
<script>
(function () {
  'use strict';

  var views = {
    live: document.getElementById('live'),
    history: document.getElementById('history'),
    detail: document.getElementById('detail')
  };
  var satsEl = document.getElementById('sats');
  var caldotEl = document.getElementById('caldot');
  var modeButtons = Array.prototype.slice.call(document.querySelectorAll('.modebtn'));
  var messageEl = document.getElementById('message');
  var bigvalueEl = document.getElementById('bigvalue');
  var bigtextEl = document.getElementById('bigtext');
  var bigunitEl = document.getElementById('bigunit');
  var timerEl = document.getElementById('timer');
  var runlistEl = document.getElementById('runlist');
  var dresultEl = document.getElementById('dresult');
  var dmodeEl = document.getElementById('dmode');
  var ddateEl = document.getElementById('ddate');
  var speedchartEl = document.getElementById('speedchart');
  var accelchartEl = document.getElementById('accelchart');

  var state = null;
  var resultMode = false;
  var resultTimer = null;
  var currentView = 'live';
  var shownMode = null;

  function show(name) {
    currentView = name;
    for (var key in views) {
      views[key].hidden = key !== name;
    }
  }

  function fmtSeconds(value) {
    var seconds = Number(value);
    return (isFinite(seconds) ? seconds : 0).toFixed(2);
  }

  function pad2(value) {
    return (value < 10 ? '0' : '') + value;
  }

  function fmtDateTime(value) {
    if (!value) {
      return 'No GPS date';
    }
    var text = String(value);
    if (text.indexOf('T') === -1) {
      text = text.replace(' ', 'T');
    }
    if (!/(Z|[+-]\d\d:?\d\d)$/.test(text)) {
      text += 'Z';
    }
    var date = new Date(text);
    if (isNaN(date.getTime())) {
      return String(value);
    }
    return date.getFullYear() + '-' + pad2(date.getMonth() + 1) + '-' + pad2(date.getDate()) + ' ' +
      pad2(date.getHours()) + ':' + pad2(date.getMinutes()) + ':' + pad2(date.getSeconds());
  }

  function renderMode(mode) {
    modeButtons.forEach(function (button) {
      button.classList.toggle('active', button.dataset.mode === mode);
    });
  }

  function showMessage(text) {
    messageEl.textContent = text;
    messageEl.hidden = false;
    bigvalueEl.hidden = true;
  }

  function showBig(text, unit) {
    bigtextEl.textContent = text;
    bigunitEl.textContent = unit || '';
    bigvalueEl.hidden = false;
    messageEl.hidden = true;
  }

  function renderState(next) {
    state = next;
    satsEl.innerHTML = next.satellites + '<span class="satlabel"> sats</span>';
    caldotEl.hidden = !next.calibrating;

    var modeChanged = shownMode !== null && shownMode !== next.mode;
    shownMode = next.mode;
    renderMode(next.mode);

    if (next.stage === 'BENCH_ARMING' || next.stage === 'BENCH_RUNNING') {
      clearResult();
    }

    if (modeChanged) {
      clearResult();
    }

    if (resultMode) {
      timerEl.innerHTML = Math.round(next.speedKmh) + '<span class="unit">km/h</span>';
      return;
    }

    if (next.stage === 'SEARCHING_GPS') {
      showMessage('Searching signal...');
      timerEl.textContent = '';
      return;
    }

    if (next.stage === 'CALIBRATING_FIRST') {
      showMessage('Calibration...');
      timerEl.textContent = '';
      return;
    }

    showBig(String(Math.round(next.speedKmh)), 'km/h');

    if (next.stage === 'BENCH_ARMING' || next.stage === 'BENCH_RUNNING') {
      timerEl.textContent = Number(next.elapsedSeconds).toFixed(1);
    } else {
      timerEl.textContent = '';
    }
  }

  function showResult(result) {
    resultMode = true;
    if (currentView === 'live') {
      showBig(fmtSeconds(result.resultSeconds), 's');
      if (state) {
        timerEl.innerHTML = Math.round(state.speedKmh) + '<span class="unit">km/h</span>';
      }
    }
    clearTimeout(resultTimer);
    resultTimer = setTimeout(clearResult, 10000);
  }

  function clearResult() {
    if (!resultMode) {
      return;
    }
    resultMode = false;
    clearTimeout(resultTimer);
    if (state) {
      renderState(state);
    }
  }

  function drawChart(canvas, samples, pick, signed, baseMin, baseMax, step) {
    var dpr = window.devicePixelRatio || 1;
    var width = canvas.clientWidth || 320;
    var height = canvas.clientHeight || 200;
    canvas.width = Math.round(width * dpr);
    canvas.height = Math.round(height * dpr);

    var ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);

    var padLeft = 40;
    var padRight = 10;
    var padTop = 10;
    var padBottom = 22;
    var plotWidth = width - padLeft - padRight;
    var plotHeight = height - padTop - padBottom;

    if (!samples || samples.length < 2) {
      ctx.fillStyle = '#666';
      ctx.font = '13px system-ui, sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('No data', padLeft + plotWidth / 2, padTop + plotHeight / 2);
      return;
    }

    var maxT = Number(samples[samples.length - 1].t) || 1;
    var dataMin = 0;
    var dataMax = 0;
    samples.forEach(function (sample) {
      var value = Number(pick(sample)) || 0;
      if (value < dataMin) {
        dataMin = value;
      }
      if (value > dataMax) {
        dataMax = value;
      }
    });

    var epsilon = step / 1000;
    var maxY = Math.max(baseMax, Math.ceil((dataMax - epsilon) / step) * step);
    var minY = signed ? Math.min(baseMin, Math.floor((dataMin + epsilon) / step) * step) : 0;

    function x(t) {
      return padLeft + (Number(t) / maxT) * plotWidth;
    }

    function y(value) {
      return padTop + (1 - (Number(value) - minY) / (maxY - minY)) * plotHeight;
    }

    ctx.font = '10px system-ui, sans-serif';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'middle';
    for (var i = Math.round(minY / step); i <= Math.round(maxY / step); i++) {
      var value = i * step;
      var gridY = y(value);
      ctx.strokeStyle = '#222';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(padLeft, gridY);
      ctx.lineTo(padLeft + plotWidth, gridY);
      ctx.stroke();
      ctx.fillStyle = '#777';
      ctx.fillText(signed ? (value === 0 ? '0' : value.toFixed(1)) : String(Math.round(value)), padLeft - 5, gridY);
    }

    ctx.strokeStyle = '#333';
    ctx.strokeRect(padLeft, padTop, plotWidth, plotHeight);

    ctx.fillStyle = '#777';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'top';
    ctx.fillText('0', padLeft, padTop + plotHeight + 5);
    ctx.fillText(maxT.toFixed(1), padLeft + plotWidth, padTop + plotHeight + 5);

    ctx.save();
    ctx.beginPath();
    ctx.rect(padLeft, padTop, plotWidth, plotHeight);
    ctx.clip();
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1.5;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.beginPath();
    samples.forEach(function (sample, index) {
      if (index === 0) {
        ctx.moveTo(x(sample.t), y(pick(sample)));
      } else {
        ctx.lineTo(x(sample.t), y(pick(sample)));
      }
    });
    ctx.stroke();
    ctx.restore();
  }

  function openHistory() {
    runlistEl.innerHTML = '<div class="empty">Loading...</div>';
    show('history');
    fetch('/api/runs').then(function (response) {
      if (!response.ok) {
        throw new Error('http ' + response.status);
      }
      return response.json();
    }).then(function (runs) {
      if (!runs.length) {
        runlistEl.innerHTML = '<div class="empty">No runs yet</div>';
        return;
      }
      runlistEl.innerHTML = '';
      runs.forEach(function (run) {
        var row = document.createElement('button');
        row.className = 'runrow';
        var result = document.createElement('span');
        result.className = 'runresult';
        result.innerHTML = fmtSeconds(run.resultSeconds) + '<span class="unit">s</span>';
        if (run.mode) {
          var modeTag = document.createElement('span');
          modeTag.className = 'runmode';
          modeTag.textContent = run.mode;
          result.appendChild(modeTag);
        }
        var date = document.createElement('span');
        date.className = 'rundate';
        date.textContent = fmtDateTime(run.date);
        row.appendChild(result);
        row.appendChild(date);
        row.addEventListener('click', function () {
          openRun(run.id);
        });
        runlistEl.appendChild(row);
      });
    }).catch(function () {
      runlistEl.innerHTML = '<div class="empty">Failed to load runs</div>';
    });
  }

  function openRun(id) {
    fetch('/api/runs/' + id).then(function (response) {
      if (!response.ok) {
        throw new Error('http ' + response.status);
      }
      return response.json();
    }).then(function (run) {
      dresultEl.textContent = fmtSeconds(run.resultSeconds);
      dmodeEl.textContent = run.mode || '';
      dmodeEl.hidden = !run.mode;
      ddateEl.textContent = fmtDateTime(run.gpsDateTime);
      show('detail');
      drawChart(speedchartEl, run.samples, function (sample) { return sample.s; }, false, 0, 100, 20);
      drawChart(accelchartEl, run.samples, function (sample) { return sample.a; }, true, -0.2, 0.8, 0.2);
    }).catch(function () {
      openHistory();
    });
  }

  document.getElementById('historybtn').addEventListener('click', openHistory);
  document.getElementById('backhistory').addEventListener('click', openHistory);
  document.getElementById('backlive').addEventListener('click', function () {
    show('live');
  });
  document.getElementById('recalbtn').addEventListener('click', function () {
    fetch('/api/recalibrate', { method: 'POST' }).catch(function () {});
  });

  modeButtons.forEach(function (button) {
    button.addEventListener('click', function () {
      fetch('/api/mode/' + button.dataset.mode, { method: 'POST' }).catch(function () {});
    });
  });

  fetch('/api/state').then(function (response) {
    return response.json();
  }).then(renderState).catch(function () {});

  var events = new EventSource('/events');
  events.addEventListener('state', function (event) {
    renderState(JSON.parse(event.data));
  });
  events.addEventListener('result', function (event) {
    showResult(JSON.parse(event.data));
  });
})();
</script>
</body>
</html>
)HTML";

#endif
