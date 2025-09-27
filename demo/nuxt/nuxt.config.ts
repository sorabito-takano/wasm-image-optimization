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
  
  // WebWorker and WASM support
  nitro: {
    experimental: {
      wasm: true
    },
  // Static file delivery configuration
    publicAssets: [
      {
        dir: 'public/wasm-image-reducer',
        baseURL: '/wasm-image-reducer',
        maxAge: 60 * 60 * 24 * 7 // 1 week cache
      }
    ]
  },

  // Enable SSR; WebWorkers only run on client
  ssr: true, // Enable SSR by default, but handle WebWorkers client-side

  // Vite configuration for better WASM support
  vite: {
    plugins: [wasmImageOptimizationPlugin('.nuxt/dist/client/_nuxt/')],
    resolve: {
      alias: {
        '@': '.',
        '~': '.'
      }
    },
  // Static file serving rules for dev server
    server: {
      fs: {
        allow: ['..', './public/wasm-image-reducer']
      }
    }
  },

  // Experimental features to improve WASM integration
  experimental: {
    payloadExtraction: false, // Disable payload extraction for better client-side hydration
  },

  // Runtime public configuration
  runtimeConfig: {
    public: {
      wasmImageReducerPath: '/wasm-image-reducer'
    }
  }
})
