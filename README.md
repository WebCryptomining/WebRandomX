# WebRandomX

WebRandomX is a JavaScript/WASM implementation of [tevador/RandomX](https://github.com/tevador/RandomX) PoW for web platforms.

## Build

### Get Source

```
git clone https://github.com/WebCryptomining/WebRandomX.git
```

### WASM Binary

Prerequisites: emcc, cmake, make

```shell
cd WebRandomX
mkdir build && cd build
emcmake cmake -DARCH=native ..
make
```

To generate `web-randomx-tests` and  `web-randomx-benchmark` executables for testing, just set the `TESTS` option to true and run the generated scripts with Node.js:

```shell
emcmake cmake -DARCH=native -DTESTS=true ..
node web-randomx-tests.js
node web-randomx-benchmark.js [options]
```

### Web App

Prerequisites: npm

```shell
npm install
```

For development, run:

```shell
npm run dev
```

Then lauch Chrome debugger through VSCode or manually.

For production, run:

```shell
npm run build
```

Webpack will generate the files and put them in the WebRandomX/dist folder. They can be deployed with nginx or Apache.

**Note**: The proxy server address should be configured in `src/js/job.js`.
