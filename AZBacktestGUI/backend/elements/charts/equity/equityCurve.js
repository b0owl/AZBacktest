import { TRADES_CSV, fsUrl } from '../../../App.js';
import { Chart } from 'chart.js/auto';

const container = document.querySelector('.equityCurve');
const canvas = document.createElement('canvas');
container.appendChild(canvas);

const response = await fetch(fsUrl(TRADES_CSV));
if (!response.ok) throw new Error(`trades.csv not found (${response.status})`);

const text = await response.text();
const rows = text.trim().split('\n').slice(1);

// Extract and downsample data
const MAX_POINTS = 1000;
const allLabels = [];
const allEquity = [];

for (const row of rows) {
  const cols = row.split(',');
  allLabels.push(cols[0]);
  allEquity.push(Number(cols[6]));
}

const step = Math.max(1, Math.floor(allEquity.length / MAX_POINTS));
const labels = allLabels.filter((_, i) => i % step === 0);
const equity = allEquity.filter((_, i) => i % step === 0);

// Per-step returns from the downsampled equity
const returns = [];
for (let i = 1; i < equity.length; i++) returns.push(equity[i] - equity[i - 1]);

// Shuffle (without replacement) — every permutation uses each return once,
// so all paths land on the same final equity as the actual curve.
function generateShuffledPath() {
    const shuffled = [...returns].sort(() => Math.random() - 0.5);
    let value = 0;
    const path = [0];
    for (const r of shuffled) {
        value += r;
        path.push(value);
    }
    return path;
}

// Bootstrap (with replacement) — draws returns randomly, paths diverge at the end.
function generateBootstrapPath() {
    let value = 0;
    const path = [0];
    for (let i = 0; i < returns.length; i++) {
        value += returns[Math.floor(Math.random() * returns.length)];
        path.push(value);
    }
    return path;
}

const shuffled = Array.from({ length: 50 }, (_, i) => ({
    label: `Shuffle ${i + 1}`,
    data: generateShuffledPath(),
    borderColor: 'rgba(148, 163, 184, 0.1)',
    borderWidth: 1,
    pointRadius: 0,
    tension: 0,
    fill: false,
}));

const bootstrapped = Array.from({ length: 50 }, (_, i) => ({
    label: `Bootstrap ${i + 1}`,
    data: generateBootstrapPath(),
    borderColor: 'rgba(251, 146, 60, 0.1)',
    borderWidth: 1,
    pointRadius: 0,
    tension: 0,
    fill: false,
}));

const simulations = [...shuffled, ...bootstrapped];

// Create chart
const equityCurve = new Chart(canvas, {
  type: 'line',
  data: {
    labels,
    datasets: [
      {
        label: 'Equity',
        data: equity,
        borderColor: 'rgba(59, 130, 246, 0.55)',
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.2,
      },
      ...simulations
    ]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
    animation: false, // important for 500 lines
    elements: {
      line: {
        tension: 0.2
      },
      point: {
        radius: 0
      }
    },
    plugins: {
      legend: {
        display: false // 500 sims would break UI otherwise
      }
    },
    scales: {
      x: {
        ticks: {
          maxTicksLimit: 10
        }
      }
    }
  }
});