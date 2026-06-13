const _DEFAULT_CSV = 'C:/Users/b0owl/Projects/Backtests/CVD/results/trades.csv'
export const TRADES_CSV = localStorage.getItem('tradesCsvPath') || _DEFAULT_CSV
export const fsUrl = path => '/@fs/' + path

import { keyboardEvents } from './keyboardEvents/keyboardEvents.js'
import { openConsole, closeConsole, consoleAppend, consoleDelete, consoleDeleteBlock, consoleSubmit } from './elements/console/console.js'
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
                if (words[0] === 'GO' && words[1] === 'STATPAGE') {
                    document.querySelector('.equityCurve').style.display = 'none'
                    document.querySelector('.wrOverTime').style.display = 'flex'
                } else if (words[0] === 'GO' && words[1] === 'EQUITY') {
                    document.querySelector('.equityCurve').style.display = 'block'
                    document.querySelector('.wrOverTime').style.display = 'none'
                } else if (words[0] === 'SET' && words[1] === 'RESULTS') {
                    localStorage.setItem('tradesCsvPath', words[2])
                    location.reload()
                }
            }
            consoleOpen = false
            closeConsole()
        } else if (key === 'Shift') {
            allowAsChar = false 
        } else {
            allowAsChar = true
        }
    },
    true
)

keyboardEvents(
    (key) => {
        if (consoleOpen && allowAsChar && key !== 'Control' && key !== 'Shift') consoleAppend(key)
    },
    false
)

setInterval(() => {
    document.getElementById('console-cursor').style.display = 'block'
}, 500)

setInterval(() => {
    document.getElementById('console-cursor').style.display = 'none'
}, 1000)
