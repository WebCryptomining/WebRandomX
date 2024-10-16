class Wrapper {
  constructor (module) {
    this.throttleWait = 0
    this.throttledStart = 0
    this.throttledHashes = 0
    this.workThrottledBound = this.workThrottled.bind(this)
    this.currentJob = null
    this.target = new Uint8Array([255, 255, 255, 255, 255, 255, 255, 255])
    this.variant = 0
    this.height = 0
    this.blob = []
    this.input = new Uint8Array(module.HEAPU8.buffer, module._malloc(84), 84)
    this.output = new Uint8Array(module.HEAPU8.buffer, module._malloc(32), 32)
    this.seed_input = new Uint8Array(module.HEAPU8.buffer, module._malloc(32), 32)
    this.module = module
    self.postMessage('ready')
    self.onmessage = this.onMessage.bind(this)
  }

  onMessage (response) {
    const data = response.data
    if (!this.currentJob || this.currentJob.job_id !== data.job_id) {
      this.setJob(data)
    }
    if (data.throttle) {
      this.throttleWait = 1 / (1 - data.throttle) - 1
      this.throttledStart = this.now()
      this.throttledHashes = 0
      this.workThrottled()
    } else {
      this.work()
    }
  }

  hexToBytes (hex) {
    const len = hex.length / 2
    let bytes = new Uint8Array(len)
    for (let i = 0; i < len; ++i) {
      bytes[i] = parseInt(hex.substr(i*2, 2), 16)
    }
    return bytes
  }

  bytesToHex (bytes) {
    let hex = ''
    for (let i = 0; i < bytes.length; ++i) {
      hex += (bytes[i] >>> 4).toString(16)
      hex += (15 & bytes[i]).toString(16)
    }
    return hex
  }

  meetsTarget (output, target) {
    for (let i = 1; i <= target.length; ++i) {
      if (output[output.length - i] > target[target.length - i]) {
        return false
      }
      if (output[output.length - i] < target[target.length - i]) {
        return true
      }
    }
    return false
  }

  setJob (data) {
    this.currentJob = data
    this.blob = this.hexToBytes(data.blob)
    this.input.set(this.blob)
    const target = this.hexToBytes(data.target)
    if (target.length <= 8) {
      for (let i = 1; i <= target.length; ++i) {
        this.target[this.target.length-i] = target[target.length-i]
      }
      for (let i = 0; i < this.target.length - target.length; ++i) {
        this.target[i] = 255
      }
    } else {
      this.target = target
    }
    this.variant = data.variant === undefined ? 0 : data.variant
    this.height = data.height === undefined ? 0 : data.height
    this.seed_blob = this.hexToBytes(data.seed_hash)
    this.seed_input.set(this.seed_blob)
  }

  now () {
    if (self.performance) {
      return self.performance.now()
    }
    return Date.now()
  }

  hash (input, output, byteLength, variant, height, seed) {
    const nonce = 4294967295 * Math.random() + 1 >>> 0
    this.input[39] = (4278190080 & nonce) >> 24
    this.input[40] = (16711680 & nonce) >> 16
    this.input[41] = (65280 & nonce) >> 8
    this.input[42] = (255 & nonce) >> 0
    return this.module._randomx_hash(BigInt(height), BigInt(height), seed.byteOffset, input.byteOffset, byteLength, output.byteOffset)
  }

  work () {
    const workStart = this.now()
    let hashes = 0, ifMeetTarget = false, interval = 0
    while (!ifMeetTarget && interval < 1e3) {
      hashes += this.hash(this.input, this.output, this.blob.length, this.variant, this.height, this.seed_input)
      ifMeetTarget = this.meetsTarget(this.output, this.target)
      interval = this.now() - workStart
    }
    const hashesPerSecond = hashes / (interval / 1e3)
    if (ifMeetTarget) {
      const nonce = this.bytesToHex(this.input.subarray(39, 43))
      const result = this.bytesToHex(this.output)
      self.postMessage({
        hashesPerSecond: hashesPerSecond,
        hashes: hashes,
        job_id: this.currentJob.job_id,
        nonce: nonce,
        result: result
      })
    } else {
      self.postMessage({
        hashesPerSecond: hashesPerSecond,
        hashes: hashes
      })
    }
  }

  workThrottled () {
    const workStart = this.now()
    this.hash(this.input, this.output, this.blob.length, this.variant, this.height, this.seed_input)
    const workEnd = this.now(),
          interval = workEnd - workStart
    +this.throttledHashes
    const totalInterval = workEnd - this.throttledStart,
          hashesPerSecond = this.throttledHashes / (totalInterval / 1e3)
    if (this.meetsTarget(this.output, this.target)) {
      const nonce = this.bytesToHex(this.input.subarray(39, 43))
      const result = this.bytesToHex(this.output)
      self.postMessage({
        hashesPerSecond: hashesPerSecond,
        hashes: this.throttledHashes,
        job_id: this.currentJob.job_id,
        nonce: nonce,
        result: result
      })
    } else if (totalInterval > 1e3) {
      self.postMessage({
        hashesPerSecond: hashesPerSecond,
        hashes: this.throttledHashes
      })
    } else {
      setTimeout(this.workThrottledBound, Math.min(2e3, interval * this.throttleWait))
    }
  }
}

export {
  Wrapper as default
}