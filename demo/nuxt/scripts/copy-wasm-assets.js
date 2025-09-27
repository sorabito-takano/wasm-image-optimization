#!/usr/bin/env node

import { copyFileSync, mkdirSync, existsSync, readdirSync, statSync } from 'fs'
import { join, dirname } from 'path'
import { fileURLToPath } from 'url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = dirname(__filename)

// Path configuration
const projectRoot = join(__dirname, '..')
const sourceDir = join(projectRoot, '..', '..', 'dist') // wasm-image-reducer/dist
const targetDir = join(projectRoot, 'public', 'wasm-image-reducer')

console.log('📦 Starting automatic copy of WASM Image Reducer assets...')
console.log(`🔍 Source: ${sourceDir}`)
console.log(`🎯 Target: ${targetDir}`)

// Create target directory if it doesn't exist
if (!existsSync(targetDir)) {
  mkdirSync(targetDir, { recursive: true })
}

// Recursively copy a directory
function copyDirectory(src, dest) {
  if (!existsSync(dest)) {
    mkdirSync(dest, { recursive: true })
  }

  const entries = readdirSync(src)
  
  for (const entry of entries) {
    const srcPath = join(src, entry)
    const destPath = join(dest, entry)
    
    if (statSync(srcPath).isDirectory()) {
      copyDirectory(srcPath, destPath)
    } else {
      copyFileSync(srcPath, destPath)
      console.log(`✅ Copied: ${entry}`)
    }
  }
}

try {
  if (existsSync(sourceDir)) {
  // Copy entire dist directory
    copyDirectory(sourceDir, targetDir)
  console.log('✨ Finished copying WASM assets!')
  } else {
    console.warn(`⚠️  Source directory not found: ${sourceDir}`)
  console.log('💡 Please build the wasm-image-reducer package')
  }
} catch (error) {
  console.error('❌ Error occurred while copying assets:', error.message)
  process.exit(1)
}