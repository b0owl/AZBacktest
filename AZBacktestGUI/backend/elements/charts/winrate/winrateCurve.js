import '../../../App.js';
import { Chart } from 'chart.js/auto';

const container = document.querySelector('.chart');
const canvas = document.createElement('canvas');
container.appendChild(canvas);

// Load CSV
const response = await fetch('/@fs/C:/Users/b0owl/Projects/Backtests/CVD/results/trades.csv');
if (!response.ok) throw new Error(`trades.csv not found (${response.status})`);

const text = await response.text();
const rows = text.trim().split('\n').slice(1); 

const allWins = [];
const wrOverTime = [];
let totalTrades = 0;


for (let i = 0; i < rows.length; i++) {
    if (Number(rows[i].split(',')[5]) >= 0) allWins.push(i);
    totalTrades++
    const winRate = allWins.length / totalTrades;

    if (i % 10 === 0) wrOverTime.push(winRate);
}


// Create chart
const chart = new Chart(canvas, {
  type: 'line',
  data: {
    labels: Array.from({ length: wrOverTime.length }, (_, i) => i * 10),
    datasets: [
      {
        label: 'Win Rate',
        data: wrOverTime,
        borderColor: '#3b82f6',
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.2,
      },
    ]
  },
  options: {
    responsive: true,
    maintainAspectRatio: false,
  }
});