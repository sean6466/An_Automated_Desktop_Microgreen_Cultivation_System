import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],

  server: {
    host: "0.0.0.0",

    allowedHosts: [
      "kwrvh-101-115-182-117.free.pinggy.net"
    ],

    proxy: {
      "/api": {
        target: "http://192.168.1.102",
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, "")
      }
    },

    hmr: {
      protocol: "wss",
      host: "kwrvh-101-115-182-117.free.pinggy.net",
      clientPort: 443
    }
  }
});