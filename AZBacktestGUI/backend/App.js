export const TRADES_CSV = localStorage.getItem('tradesCsvPath') || ''
export const fsUrl = path => '/@fs/' + path

import { keyboardEvents } from './keyboardEvents/keyboardEvents.js'
import { openConsole, closeConsole, consoleAppend, consoleDelete, consoleDeleteBlock, consoleSubmit, consoleGetText } from './elements/console/console.js'
import './elements/console/console.css'
import './elements/charts/charts.css'
import '../rendering/stylesheet.css'


/* -- CONSOLE CONTROL BLOCK --  */
let consoleOpen = false
let allowAsChar = false
keyboardEvents(
    (key, ctrl) => {
        allowAsChar = false
        
        if (key === 'Space') {
            if (!consoleOpen) {
                consoleOpen = true
                openConsole()
            } else {
                allowAsChar = true
            }
        }
        else if (key === 'Escape' && consoleOpen) {
            consoleOpen = false
            closeConsole()
        }
        else if (key === 'KeyV' && ctrl && consoleOpen) {
            allowAsChar = false
            navigator.clipboard.readText().then(text => {
                for (const char of text) consoleAppend(char)
            })
        }
        else if (key === 'KeyC' && ctrl && consoleOpen) {
            allowAsChar = false
            navigator.clipboard.writeText(consoleGetText())
        }
        else if (key === 'Backspace' && consoleOpen) {
            if (ctrl)
                consoleDeleteBlock()
            else
                consoleDelete()
        }
        else if (key === 'Enter' && consoleOpen) {
            allowAsChar = false
            const words = consoleSubmit()
            if (words.length > 0) {
                const cmd = words[0].toUpperCase()
                const sub = (words[1] || '').toUpperCase()
                if (cmd === 'GO' && sub === 'STATPAGE') {
                    document.querySelector('.equityCurve').style.display = 'none'
                    document.querySelector('.wrOverTime').style.display = 'flex'
                } else if (cmd === 'GO' && sub === 'EQUITY') {
                    document.querySelector('.equityCurve').style.display = 'block'
                    document.querySelector('.wrOverTime').style.display = 'none'
                } else if (cmd === 'SET' && sub === 'RESULTS') {
                    localStorage.setItem('tradesCsvPath', words.slice(2).join(' '))
                    location.reload()
                }
            }
            consoleOpen = false
            closeConsole()
        } else if (key === 'Shift' || key === 'ControlLeft' || key === 'ControlRight') {
            allowAsChar = false
        } else if (!ctrl) {
            allowAsChar = true
        }
    },
    true
)

keyboardEvents(
    (key, ctrl) => {
        if (consoleOpen && allowAsChar && !ctrl && key !== 'Control' && key !== 'Shift') consoleAppend(key)
    },
    false
)

setInterval(() => {
    document.getElementById('console-cursor').style.display = 'block'
}, 500)

setInterval(() => {
    document.getElementById('console-cursor').style.display = 'none'
}, 1000)
