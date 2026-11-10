import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  server: {
    host: '0.0.0.0',
    port: 5173,
    proxy: {
      '/api': {
        target: 'https://participation-suggestion-opponents-quoted.trycloudflare.com',
        changeOrigin: true,
        secure: false,
      },
      '/uploads': {
        target: 'https://participation-suggestion-opponents-quoted.trycloudflare.com',
        changeOrigin: true,
        secure: false,
      },
    },
  },
})
