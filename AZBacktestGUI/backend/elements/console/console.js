function updateCursor() {
    const content = document.getElementById('console-content')
    const cursor  = document.getElementById('console-cursor')
    const box     = document.getElementById('console').getBoundingClientRect()

    const cursorY = Math.round((52 - 20) / 2)

    if (content.lastChild) {
        const spanRect = content.lastChild.getBoundingClientRect()
        cursor.style.left = (spanRect.right - box.left) + 'px'
        cursor.style.top = cursorY + 'px'
    } else {
        const contentRect = content.getBoundingClientRect()
        cursor.style.left = (contentRect.left - box.left) + 'px'
        cursor.style.top = cursorY + 'px'
    }
}

export function openConsole() {
    document.getElementById('console').style.display = 'block'
    document.getElementById('tint').style.display = 'block'
    updateCursor()
}

export function closeConsole() {
    const el      = document.getElementById('console-content')
    const printout = document.getElementById('console-printout')
    const spans   = Array.from(el.querySelectorAll('span'))
    document.getElementById('console').style.display = 'none'
    document.getElementById('console').style.height = ''
    document.getElementById('tint').style.display = 'none'
    printout.style.display = 'none'
    printout.innerHTML = ''
    while (spans.length > 0) {
        spans.pop().remove()
    }
    updateCursor()
}

export function consoleAppend(keypress) {
    const printout = document.getElementById('console-printout')
    if (printout.style.display !== 'none') {
        printout.style.display = 'none'
        printout.innerHTML = ''
        document.getElementById('console').style.height = ''
    }
    const el   = document.getElementById('console-content')
    const span = document.createElement('span')
    span.style.textTransform = 'uppercase'
    span.textContent = keypress
    el.appendChild(span)
    updateCursor()
}

export function consoleDelete() {
    const el = document.getElementById('console-content')
    if (el.lastChild) el.lastChild.remove()
    updateCursor()
}

export function consoleSubmit() {
    const el   = document.getElementById('console-content')
    const text = Array.from(el.querySelectorAll('span')).map(s => s.textContent).join('').trim()
    const words = text.split(/\s+/).filter(w => w.length > 0)
    Array.from(el.querySelectorAll('span')).forEach(s => s.remove())
    updateCursor()
    return words
}

export function consoleGetText() {
    const el = document.getElementById('console-content')
    return Array.from(el.querySelectorAll('span')).map(s => s.textContent).join('')
}

export function consoleDeleteBlock() {
    const el    = document.getElementById('console-content')
    const spans = Array.from(el.querySelectorAll('span'))
    if (spans.length === 0) return
    if (spans[spans.length - 1].textContent === ' ') {
        spans[spans.length - 1].remove()
    } else {
        while (spans.length > 0 && spans[spans.length - 1].textContent !== ' ') {
            spans.pop().remove()
        }
    }
    updateCursor()
}

export function addConsolePane(rows) {
    const consoleEl = document.getElementById('console')
    const printout  = document.getElementById('console-printout')

    printout.innerHTML = ''

    const pane = document.createElement('div')
    pane.className = 'console-pane'

    for (const { label, value } of rows) {
        const row = document.createElement('div')
        row.className = 'console-pane-row'

        const lEl = document.createElement('span')
        lEl.className = 'console-pane-label'
        lEl.textContent = label

        const vEl = document.createElement('span')
        vEl.className = 'console-pane-value'
        vEl.textContent = value

        row.appendChild(lEl)
        row.appendChild(vEl)
        pane.appendChild(row)
    }

    printout.appendChild(pane)
    printout.style.display = 'block'

    // 52px = input bar, 20px = printout padding, 34px = row height + gap per row
    consoleEl.style.height = (52 + 20 + rows.length * 34) + 'px'
    updateCursor()
}