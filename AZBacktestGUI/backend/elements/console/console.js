function updateCursor() {
    const content = document.getElementById('console-content')
    const cursor  = document.getElementById('console-cursor')
    const box     = document.getElementById('console').getBoundingClientRect()

    const consoleEl = document.getElementById('console')
    const cursorY = Math.round((consoleEl.offsetHeight - 20) / 2)

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
    const el    = document.getElementById('console-content')
    const spans = Array.from(el.querySelectorAll('span'))
    document.getElementById('console').style.display = 'none'
    document.getElementById('tint').style.display = 'none'
    while (spans.length > 0) {
        spans.pop().remove()
    }
    updateCursor()
}

export function consoleAppend(keypress) {
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
    const words = text.toUpperCase().split(/\s+/).filter(w => w.length > 0)
    Array.from(el.querySelectorAll('span')).forEach(s => s.remove())
    updateCursor()
    return words
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
