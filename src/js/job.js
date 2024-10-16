class Job {
  // The miner will only start if no other tabs are already mining. Default
  static IF_EXCLUSIVE_TAB = 'ifExclusiveTab'
  // The miner will always start and immediately kill all miners in other tabs that have not specified Minero.FORCE_MULTI_TAB
  static FORCE_EXCLUSIVE_TAB = 'forceExclusiveTab'
  // The miner will always start. It will not announce its presence to other tabs, will not kill any other miners and can't be killed by other miners
  static FORCE_MULTI_TAB = 'forceMultiTab'

  static CONFIG = {
    WEBSOCKET_SHARDS: [
      ['ws://Your Server IP']
    ]
  }
}

export {
  Job as default
}
