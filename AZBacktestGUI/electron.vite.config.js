import { defineConfig } from 'electron-vite'
import { resolve } from 'path'

export default defineConfig({
    main: {
        build: {
            lib: {
                entry: resolve(__dirname, 'electron/main.js')
            }
        }
    },
    preload: {
        build: {
            lib: {
                entry: resolve(__dirname, 'electron/preload.js')
            }
        }
    },
    renderer: {
        root: 'rendering',
        server: {
            fs: { allow: ['../../..'] }
        },
        build: {
            rollupOptions: {
                input: resolve(__dirname, 'rendering/index.html')
            }
        }
    }
})
