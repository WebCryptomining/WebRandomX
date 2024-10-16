import Wrapper from './wrapper'

;(async () => {
  const Module = (await import('../../build/web-randomx.js')).default
  const wasmBin = (await import('../../build/web-randomx.wasm')).default
  const module = await Module({
    locateFile (path) {
      if (path.endsWith('.wasm')) {
        return wasmBin
      }
      return path
    }
  })
  new Wrapper(module)
})()
