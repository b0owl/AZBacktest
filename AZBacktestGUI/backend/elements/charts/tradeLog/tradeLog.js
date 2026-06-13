import { TRADES_CSV, fsUrl } from '../../../App.js';

const container = document.querySelector('.stat-right');

const response = await fetch(fsUrl(TRADES_CSV));
if (!response.ok) throw new Error(`trades.csv not found (${response.status})`);

const text = await response.text();
const [header, ...rows] = text.trim().split('\n');
const cols = header.split(',');

const table = document.createElement('table');
table.style.cssText = 'width: 100%; border-collapse: collapse;';

const thead = document.createElement('thead');
thead.style.cssText = 'position: sticky; top: 0; background: #0d1117; z-index: 1;';
const headerRow = document.createElement('tr');
for (const col of cols) {
    const th = document.createElement('th');
    th.textContent = col;
    th.style.cssText = 'padding: 8px 10px; text-align: left; color: #8b949e; border-bottom: 1px solid #21262d; white-space: nowrap;';
    headerRow.appendChild(th);
}
thead.appendChild(headerRow);
table.appendChild(thead);

const tbody = document.createElement('tbody');
rows.forEach((row, i) => {
    const cells = row.split(',');
    const pnl = parseFloat(cells[5]);
    const tr = document.createElement('tr');
    tr.style.background = i % 2 === 0 ? '#161b27' : '#0d1117';
    for (let j = 0; j < cells.length; j++) {
        const td = document.createElement('td');
        td.textContent = cells[j];
        td.style.cssText = 'padding: 6px 10px; border-bottom: 1px solid #21262d; white-space: nowrap;';
        if (j === 5) td.style.color = pnl > 0 ? '#3fb950' : '#f85149';
        tr.appendChild(td);
    }
    tbody.appendChild(tr);
});
table.appendChild(tbody);
container.appendChild(table);
