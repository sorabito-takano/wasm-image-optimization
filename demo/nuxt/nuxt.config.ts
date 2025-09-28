// https://nuxt.com/docs/api/configuration/nuxt-config
import wasmImageOptimizationPlugin from '@sorabito-takano/wasm-image-optimization/vite-plugin';

export default defineNuxtConfig({
  compatibilityDate: '2025-07-15',
  devtools: { enabled: true },
  
  // TypeScript configuration
  typescript: {
    typeCheck: false // Disable type checking during build for better performance
  },
  
  // CSS configuration
  css: ['@/assets/css/global.css'],
  
  // Auto imports configuration
  imports: {
    autoImport: true
  },
  
  // Alias configuration
  alias: {
    '@': '.',
    '~': '.'
  },
  
  // Enable SSR; WebWorkers only run on client
  ssr: false, // Enable SSR by default, but handle WebWorkers client-side

  // Vite configuration for better WASM support
  vite: {
    plugins: [wasmImageOptimizationPlugin('.nuxt/dist/client/_nuxt/')],
    resolve: {
      alias: {
        '@': '.',
        '~': '.'
      }
    },
  },
})
