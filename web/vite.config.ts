import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { gzipSync } from 'node:zlib'
import { readdirSync, readFileSync, writeFileSync } from 'node:fs'
import { extname, join } from 'node:path'

const outputDirectory = join(import.meta.dirname, '../data/www')

function embeddedAssetManifest() {
  return {
    name: 'embedded-asset-manifest',
    closeBundle() {
      const assetDirectory = join(outputDirectory, 'assets')
      const assets = readdirSync(assetDirectory)
        .filter((name) => ['.js', '.css', '.woff2'].includes(extname(name)))
        .sort()
        .map((name) => {
          const contents = readFileSync(join(assetDirectory, name))
          const gzipName = `${name}.gz`
          writeFileSync(join(assetDirectory, gzipName), gzipSync(contents, { level: 9 }))
          const extension = extname(name)
          return {
            url: `/assets/${name}`,
            file: `/www/assets/${name}`,
            gzipFile: `/www/assets/${gzipName}`,
            contentType: extension === '.js' ? 'text/javascript; charset=utf-8'
              : extension === '.css' ? 'text/css; charset=utf-8' : 'font/woff2',
          }
        })
      writeFileSync(join(outputDirectory, 'asset-manifest.json'), `${JSON.stringify({ version: 1, assets })}\n`)
    },
  }
}

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), embeddedAssetManifest()],
  build: {
    outDir: '../data/www',
    emptyOutDir: true,
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name]-[hash].js',
        chunkFileNames: 'assets/[name]-[hash].js',
        assetFileNames: 'assets/[name]-[hash][extname]',
      },
    },
  },
})
