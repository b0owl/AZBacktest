import { Chart } from 'chart.js/auto';
import { TRADES_CSV, fsUrl } from '../../../App.js';

const container = document.querySelector('.stat-left');
const canvas = document.createElement('canvas');
container.appendChild(canvas);

const response = await fetch(fsUrl(TRADES_CSV));
if (!response.ok) throw new Error(`trades.csv not found (${response.status})`);

const text = await response.text();
const rows = text.trim().split('\n').slice(1);

const WINDOW = 50;
const labels = [];
const winRates = [];

for (let i = WINDOW - 1; i < rows.length; i++) {
    const window = rows.slice(i - WINDOW + 1, i + 1);
    const wins = window.filter(r => Number(r.split(',')[5]) > 0).length;
    labels.push(rows[i].split(',')[0]);
    winRates.push((wins / WINDOW) * 100);
}

new Chart(canvas, {
    type: 'line',
    data: {
        labels,
        datasets: [{
            label: `Win Rate (${WINDOW}-trade rolling)`,
            data: winRates,
            borderColor: 'rgba(59, 130, 246, 1)',
            borderWidth: 2,
            pointRadius: 0,
            tension: 0.3,
            fill: {
                target: 'origin',
                above: 'rgba(59, 130, 246, 0.08)',
            },
        }],
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
            legend: { display: false },
        },
        scales: {
            x: { ticks: { maxTicksLimit: 10 } },
            y: {
                min: 0,
                max: 100,
                ticks: {
                    callback: v => v + '%',
                    stepSize: 10,
                },
            },
        },
    },
});
