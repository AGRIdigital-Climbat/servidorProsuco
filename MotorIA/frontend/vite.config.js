import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";

const API_PATHS = ["/chat", "/busqueda", "/documentos", "/tipos-documento", "/voz", "/health"];

const proxy = {};
for (const path of API_PATHS) {
  proxy[path] = "http://127.0.0.1:8001";
}

export default defineConfig({
  plugins: [vue()],
  base: "./",
  server: {
    port: 5173,
    proxy,
  },
  build: {
    outDir: "dist",
    chunkSizeWarningLimit: 600,
  },
});
