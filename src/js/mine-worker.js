import Job from './job'

class MineWorker {
  constructor () {
    this.worker = new Worker(new URL('./worker.js', import.meta.url))
    this.worker.onmessage = this.onReady.bind(this)
    this.currentJob = null
    this.jobCallback = () => {}

    this._isReady = false
    this.hashesPerSecond = 0
    this.hashesTotal = 0
    this.running = false
    this.lastMessageTimestamp = Date.now()
  }

  onReady (response) {
    if (response.data !== "ready" || this._isReady) {
      throw 'Expecting first message to be "ready", got ' + response
    }
    this._isReady = true
    this.worker.onmessage = this.onReceiveMsg.bind(this)
    if (this.currentJob) {
      this.running = true
      this.worker.postMessage(this.currentJob)
    }
  }

  onReceiveMsg (response) {
    if (response.data.result) {
      this.jobCallback(response.data)
    }
    this.hashesPerSecond = .5 * this.hashesPerSecond + .5 * response.data.hashesPerSecond
    this.hashesTotal += response.data.hashes
    this.lastMessageTimestamp = Date.now()
    if (this.running) {
      this.worker.postMessage(this.currentJob)
    }
  }

  setJob (job, callback) {
    this.currentJob = job
    this.jobCallback = callback
    if (this._isReady && !this.running) {
      this.running = true
      this.worker.postMessage(this.currentJob)
    }
  }

  stop () {
    if (this.worker) {
      this.worker.terminate()
      this.worker = null
    }
    this.running = false
  }
}

export {
  MineWorker as default
}
