#!/usr/bin/env node
'use strict';

var BenchLogic = require('./benchlogic.js');

var LAG_MS = 300;
var G = 9.81;
var failures = 0;

function check(name, condition, detail) {
  if (condition) {
    console.log('  ok   - ' + name);
  } else {
    failures++;
    console.log('  FAIL - ' + name + (detail ? ' (' + detail + ')' : ''));
  }
}

function kmhPerSecond(g) {
  return g * G * 3.6;
}

function runBench(config) {
  var arms = 0;
  var results = [];
  var stages = [];
  var benchStarts = [];
  var bench = BenchLogic.createBench({
    onStageChange: function (stage, info) {
      stages.push(stage);
      if (stage === 'BENCH_ARMING') {
        arms++;
        benchStarts.push(info.benchStartMillis);
      }
    },
    onResult: function (result) {
      results.push(result);
    }
  });

  bench.setMode(config.mode || '0-100');

  for (var t = 0; t <= config.duration; t += 2) {
    if (config.onTick) {
      config.onTick(t, bench);
    }
    bench.tick(t, config.rawG(t), config.gpsSpeed(t));
  }

  return { arms: arms, results: results, stages: stages, benchStarts: benchStarts, bench: bench };
}

function pullRawG(g, startAt, liftAt) {
  return function (t) {
    return t >= startAt && t < liftAt ? g : 0;
  };
}

function pullSpeed(initialKmh, g, startAt, liftAt) {
  return function (t) {
    if (t <= startAt) {
      return initialKmh;
    }
    var accelEnd = Math.min(t, liftAt);
    return initialKmh + (kmhPerSecond(g) * (accelEnd - startAt)) / 1000;
  };
}

function noGps() {
  return function () {
    return 0;
  };
}

function expectedResult(initialKmh, g, startAt, targetKmh) {
  return startAt / 1000 + (targetKmh - initialKmh) / kmhPerSecond(g) + LAG_MS / 1000;
}

function assertRun(name, run, expected, options) {
  options = options || {};
  console.log(name + ':');
  if (options.expectArms) {
    check('arms once', run.arms === 1, 'arms=' + run.arms);
  }
  check('saves one run', run.results.length === 1, 'saves=' + run.results.length);
  if (run.results.length === 1) {
    var result = run.results[0].resultSeconds;
    check(
      'result ' + expected.toFixed(2) + 's +/- 0.15s',
      Math.abs(result - expected) < 0.15,
      'got ' + result.toFixed(3) + 's'
    );
  }
}

function assertNoRun(name, run) {
  console.log(name + ':');
  check('never arms', run.arms === 0, 'arms=' + run.arms);
  check('saves nothing', run.results.length === 0, 'saves=' + run.results.length);
}

console.log('Scenario A: standstill 0.45 g launch, 0-100, lift after target');
assertRun(
  'A',
  runBench({
    mode: '0-100',
    duration: 12000,
    rawG: pullRawG(0.45, 0, 7600),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.45, 0, 7600))
  }),
  expectedResult(0, 0.45, 0, 100),
  { expectArms: true }
);

console.log('Scenario B: standstill 0.25 g launch, 0-60 (regression: EMA race)');
assertRun(
  'B',
  runBench({
    mode: '0-60',
    duration: 12000,
    rawG: pullRawG(0.25, 0, 8100),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.25, 0, 8100))
  }),
  expectedResult(0, 0.25, 0, 60),
  { expectArms: true }
);

console.log('Scenario C: standstill 0.205 g launch, 0-60 (just above threshold)');
assertRun(
  'C',
  runBench({
    mode: '0-60',
    duration: 13000,
    rawG: pullRawG(0.205, 0, 9600),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.205, 0, 9600))
  }),
  expectedResult(0, 0.205, 0, 60),
  { expectArms: true }
);

console.log('Scenario D: standstill 0.2005 g launch, 0-60 (barely above threshold)');
assertRun(
  'D',
  runBench({
    mode: '0-60',
    duration: 13000,
    rawG: pullRawG(0.2005, 0, 9800),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.2005, 0, 9800))
  }),
  expectedResult(0, 0.2005, 0, 60),
  { expectArms: true }
);

console.log('Scenario E: rolling 20 km/h launch at 0.30 g, 0-60 (must be ignored)');
assertNoRun(
  'E',
  runBench({
    mode: '0-60',
    duration: 9000,
    rawG: pullRawG(0.3, 2000, 6275),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(20, 0.3, 2000, 6275))
  })
);

console.log('Scenario F: rolling 90 km/h launch at 0.30 g, 0-100 (must be ignored)');
assertNoRun(
  'F',
  runBench({
    mode: '0-100',
    duration: 7000,
    rawG: pullRawG(0.3, 2000, 3400),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(90, 0.3, 2000, 3400))
  })
);

console.log('Scenario G: 0-60 with the pedal kept down past the target (no bogus re-runs)');
assertRun(
  'G',
  runBench({
    mode: '0-60',
    duration: 13000,
    rawG: pullRawG(0.25, 0, 9800),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.25, 0, 9800))
  }),
  expectedResult(0, 0.25, 0, 60),
  { expectArms: true }
);

console.log('Scenario H: hard braking from 40 km/h (no spurious arming)');
(function () {
  var stopAt = 2000 + (40 / kmhPerSecond(0.5)) * 1000;
  var run = runBench({
    mode: '0-60',
    duration: 8000,
    rawG: function (t) {
      return t >= 2000 && t < stopAt ? 0.5 : 0;
    },
    gpsSpeed: function (t) {
      if (t <= 2000) {
        return 40;
      }
      if (t >= stopAt) {
        return 0;
      }
      return 40 - (kmhPerSecond(0.5) * (t - 2000)) / 1000;
    }
  });
  assertNoRun('H', run);
})();

console.log('Scenario I: speedo mode never arms even during a hard pull');
assertNoRun(
  'I',
  runBench({
    mode: 'speedo',
    duration: 8000,
    rawG: pullRawG(0.45, 0, 7600),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.45, 0, 7600))
  })
);

console.log('Scenario J: switching mode mid-run aborts without a result');
(function () {
  var run = runBench({
    mode: '0-100',
    duration: 8000,
    onTick: function (t, bench) {
      if (t === 1500) {
        bench.setMode('speedo');
      }
    },
    rawG: pullRawG(0.35, 0, 7600),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.35, 0, 7600))
  });
  console.log('J:');
  check('armed once before the switch', run.arms === 1, 'arms=' + run.arms);
  check('saves nothing', run.results.length === 0, 'saves=' + run.results.length);
  check('back to READY', run.bench.getStage() === 'READY', run.bench.getStage());
})();

console.log('Scenario K: walk-back backdates to the first sample above 0.1 g');
(function () {
  var run = runBench({
    mode: '0-100',
    duration: 2500,
    rawG: function (t) {
      return t < 800 ? 0.15 : 0.25;
    },
    gpsSpeed: noGps()
  });
  console.log('K:');
  check('arms once', run.arms === 1, 'arms=' + run.arms);
  check('launch backdated to 0 ms', run.benchStarts[0] === 0, 'launchStart=' + run.benchStarts[0]);
})();

console.log('Scenario L: walk-back stops at a sample below 0.1 g');
(function () {
  var run = runBench({
    mode: '0-100',
    duration: 2500,
    rawG: function (t) {
      if (t < 400) {
        return 0.05;
      }
      if (t < 800) {
        return 0.15;
      }
      return t < 1200 ? 0.05 : 0.25;
    },
    gpsSpeed: noGps()
  });
  console.log('L:');
  check('arms once', run.arms === 1, 'arms=' + run.arms);
  check(
    'launch backdated to 1200 ms',
    run.benchStarts[0] === 1200,
    'launchStart=' + run.benchStarts[0]
  );
})();

console.log('Scenario M: samples at exactly 0.2 g never trigger a launch');
assertNoRun(
  'M',
  runBench({
    mode: '0-100',
    duration: 5000,
    rawG: function () {
      return 0.2;
    },
    gpsSpeed: noGps()
  })
);

console.log('Scenario N: three of four samples above 0.2 g never trigger a launch');
assertNoRun(
  'N',
  runBench({
    mode: '0-100',
    duration: 5000,
    rawG: function (t) {
      return t % 400 === 300 ? 0.1 : 0.25;
    },
    gpsSpeed: noGps()
  })
);

console.log('Scenario O: slow launch after a long gentle pre-ramp keeps its backdated start');
assertRun(
  'O',
  runBench({
    mode: '0-60',
    duration: 13000,
    rawG: (function () {
      var rampAt = 800;
      var pullTo = 8000;
      return function (t) {
        if (t < rampAt) {
          return 0.15;
        }
        return t < pullTo ? 0.25 : 0;
      };
    })(),
    gpsSpeed: (function (base) {
      return function (t) {
        return base(t - LAG_MS);
      };
    })(pullSpeed(0, 0.25, 0, 8000))
  }),
  expectedResult(0, 0.25, 0, 60),
  { expectArms: true }
);

if (failures > 0) {
  console.log('\nFAILED: ' + failures + ' check(s)');
  process.exit(1);
}

console.log('\nPASS: detection logic behaves as expected');
