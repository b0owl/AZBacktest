export function keyboardEvents(onKey, wantCode) {
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT') return
        if (e.code === 'Space') e.preventDefault()
        if (wantCode) {
            onKey(e.code, e.ctrlKey)
        } else {
            onKey(e.key, e.ctrlKey)
        }
    })
}
