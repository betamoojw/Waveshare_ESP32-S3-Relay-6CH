import type { Capabilities, CommandResult, Device, Diagnostics, KnxConfiguration, KnxConfigurationUpdate, ModbusConfiguration, ModbusConfigurationUpdate, NetworkStatus, OtaStatus, RelayList, RelayState, User, WifiManagement, WifiProfileUpdate, WifiRecoveryAp } from './types'

export const mockCapabilities: Capabilities = {
  apiVersion: '1.0',
  minimumUiVersion: '1.0.0',
  versions: { hardware: 'HW-A01', firmware: 'FW-1.4.0+preview', configuration: 'CFG-4', api: 'API-v1', modbus: 'MODBUS-v1', knxApplication: 'KNX-APP-v1', filesystem: 'FS-v1' },
  deviceId: '00000000-0000-0000-0000-000000000001',
  bootId: 'preview-4f8219ac',
  model: 'Waveshare ESP32-S3 Relay 6CH',
  channels: Array.from({ length: 6 }, (_, id) => ({ id, physicalLabel: `CH${id + 1}`, contactFeedback: false })),
  features: {
    wifi: true,
    ethernet: false,
    modbus: true,
    knx: true,
    scenes: false,
    timers: false,
    remoteRestart: false,
    remoteFactoryReset: false,
    firmwareUpdate: true,
  },
  permissions: ['relay:read', 'relay:command', 'diagnostics:read', 'configuration:read', 'configuration:write', 'users:manage', 'firmware:update'],
}

export const mockDevice: Device = {
  name: 'Plant room actuator',
  model: mockCapabilities.model,
  serialSuffix: '0001',
  firmwareVersion: 'FW-1.4.0+preview',
  buildId: 'development',
  uptimeMs: 16_620_000,
  lifecycle: 'operational',
  lifecycleReason: 'Configuration valid',
  configurationGeneration: 12,
}

export const mockNetwork: NetworkStatus = {
  state: 'online',
  activeTransport: 'wifi',
  ipv4Address: '192.168.10.42',
  rssi: -58,
  activeProfileIndex: 0,
  recoveryApActive: false,
  lastConnectedAgeMs: 960_000,
}

let snapshotSequence = 84
const relays: RelayList['relays'] = Array.from({ length: 6 }, (_, id) => ({
  id,
  physicalLabel: `CH${id + 1}`,
  name: ['Lobby lights', 'Ventilation', 'Pump A', 'Pump B', 'Exterior lights', 'Spare'][id],
  requestedState: id === 0 || id === 3 ? 'on' : 'off',
  appliedState: id === 0 || id === 3 ? 'on' : 'off',
  verification: 'gpio-write',
  lastSource: id === 3 ? 'modbus' : 'web',
  transitionSequence: 40 + id,
  lastTransitionAgeMs: (id + 1) * 21_000,
  fault: id === 5 ? 'Output unavailable' : null,
  lockedOut: id === 2,
  enabled: id !== 5,
}))

export const mockDiagnostics: Diagnostics = {
  device: { uptimeMs: 913_240, bootCount: 18, resetReason: 'power-on', lifecycle: 'degraded' },
  persistentCounters: {
    bootCount: 18,
    watchdogCount: 1,
    brownoutCount: 2,
    configErrorCount: 1,
    otaFailureCount: 0,
    networkFailureCount: 4,
    modbusErrorCount: 3,
    knxErrorCount: 2,
    storageErrorCount: 0,
  },
  firmware: { version: '1.0.0', buildId: 'preview' },
  hardware: { model: 'Waveshare ESP32-S3 Relay 6CH', revision: '1.0' },
  memory: {
    freeHeapBytes: 219_840,
    minimumFreeHeapBytes: 182_416,
    largestFreeHeapBlockBytes: 131_072,
    psram: { available: true, totalBytes: 8_388_608, freeBytes: 8_192_000, minimumFreeBytes: 8_126_464 },
  },
  cpu: { frequencyMhz: 240, coreCount: 2 },
  network: { state: 'online', connected: true, recoveryApActive: false, wifiRssiDbm: -54, ipv4Address: '192.168.1.42' },
  storage: { filesystemAvailable: true, settingsAvailable: true, settingsHealthy: true, configurationValid: true, configurationGeneration: 12 },
  faultState: { active: true, activeCount: 1 },
  relays: relays.map(({ id, requestedState, appliedState, lockedOut, fault, transitionSequence }) => ({
    id, requestedState, appliedState, lockedOut, faulted: fault !== null, transitionSequence,
  })),
  configurationValid: true,
  persistenceHealthy: true,
  taskWatchdogHealthy: true,
  heapLowWaterMarkBytes: 182_416,
  commandCounters: { accepted: 1842, rejected: 7, queueFull: 0 },
  faults: [{ code: 'knx.bus_off', severity: 'warning', summary: 'KNX bus is not currently online.', occurrenceCount: 2 }],
  protocols: {
    modbus: { available: true, unitId: 10, baudRate: 115200, validRequests: 28_419, errors: 3 },
    knx: { available: true, enabled: true, busOnline: false, individualAddress: '1.1.42', validTelegrams: 739, errors: 2 },
  },
}

const wait = (durationMs: number) => new Promise((resolve) => window.setTimeout(resolve, durationMs))

export async function getMockRelays(): Promise<RelayList> {
  await wait(120)
  return { bootId: mockCapabilities.bootId, snapshotSequence, relays: structuredClone(relays) }
}

export async function commandMockRelay(channel: number, state: RelayState): Promise<CommandResult> {
  await wait(650)
  const relay = relays.find((candidate) => candidate.id === channel)
  if (!relay || !relay.enabled) throw new Error('relay.unavailable')
  if (relay.lockedOut && state === 'on') throw new Error('relay.safety_lockout')
  const idempotent = relay.appliedState === state
  relay.requestedState = state
  relay.appliedState = state
  relay.lastSource = 'web'
  relay.lastTransitionAgeMs = 0
  if (!idempotent) {
    relay.transitionSequence += 1
    snapshotSequence += 1
  }
  return {
    correlationId: crypto.randomUUID(),
    result: idempotent ? 'idempotent' : 'applied',
    channel,
    appliedState: state,
    sequence: relay.transitionSequence,
  }
}

export const mockWifiManagement: WifiManagement = {
  generation: 12,
  activeProfileIndex: 0,
  scan: {
    state: 'complete',
    sequence: 3,
    results: [
      { ssid: 'Plant-Network', rssi: -48, channel: 6, secured: true },
      { ssid: 'Service-LAN', rssi: -67, channel: 11, secured: true },
      { ssid: 'Commissioning', rssi: -74, channel: 1, secured: false },
    ],
  },
  profiles: [
    { index: 0, enabled: true, ssid: 'Plant-Network', hasPassphrase: true, ipv4: { mode: 'static', address: '192.168.10.42', subnetMask: '255.255.255.0', gateway: '192.168.10.1', dns: '192.168.10.1' } },
    { index: 1, enabled: true, ssid: 'Service-LAN', hasPassphrase: true, ipv4: { mode: 'dhcp', address: '', subnetMask: '', gateway: '', dns: '' } },
    { index: 2, enabled: false, ssid: '', hasPassphrase: false, ipv4: { mode: 'dhcp', address: '', subnetMask: '', gateway: '', dns: '' } },
  ],
  recoveryAp: { enabled: true, ssidPrefix: 'Relay', channel: 1, timeoutMs: 0, remainActiveWhileOffline: true, active: false },
}

export const mockUsers: User[] = [
  { id: 0, username: 'admin', role: 'administrator', enabled: true },
  { id: 1, username: 'operator', role: 'guest', enabled: true },
]

export const mockOtaStatus: OtaStatus = {
  state: 'idle', progressPercent: 0, currentVersion: '1.00', buildEnvironment: 'development', availableVersion: null, error: null,
}

export const mockModbusConfiguration: ModbusConfiguration = {
  generation: 12,
  role: 'server',
  unitId: 10,
  baudRate: 115200,
  parity: 'none',
  dataBits: 8,
  stopBits: 1,
}

export const mockKnxConfiguration: KnxConfiguration = {
  generation: 12,
  enabled: true,
  individualAddress: '1.1.20',
  startupTransmitDelayMs: 3000,
  minimumTelegramIntervalMs: 100,
  cyclicStatusIntervalMs: 0,
  heartbeatIntervalMs: 60_000,
  readSwitchObject: true,
  heartbeatGroupAddress: '0/0/1',
  centralSwitchGroupAddress: '0/0/2',
  centralOffGroupAddress: '0/0/3',
  deviceFaultGroupAddress: '0/0/4',
  channels: Array.from({ length: 6 }, (_, index) => ({
    index,
    switchGroupAddress: `1/0/${index * 2 + 1}`,
    statusGroupAddress: `1/0/${index * 2 + 2}`,
    faultGroupAddress: '',
    commandPolarityInverted: false,
    statusPolarityInverted: false,
    sendStatusAfterStartup: true,
    participatesInCentralSwitch: true,
    participatesInCentralOff: true,
  })),
}

function assertGeneration(expectedGeneration: number) {
  if (expectedGeneration !== mockDevice.configurationGeneration) throw new Error('configuration.generation_conflict')
}

function advanceGeneration() {
  mockDevice.configurationGeneration += 1
  mockWifiManagement.generation = mockDevice.configurationGeneration
  mockModbusConfiguration.generation = mockDevice.configurationGeneration
  mockKnxConfiguration.generation = mockDevice.configurationGeneration
}

export async function saveMockModbusConfiguration(configuration: ModbusConfigurationUpdate): Promise<ModbusConfiguration> {
  await wait(300)
  assertGeneration(configuration.expectedGeneration)
  Object.assign(mockModbusConfiguration, {
    unitId: configuration.unitId,
    baudRate: configuration.baudRate,
    parity: configuration.parity,
    stopBits: configuration.stopBits,
    dataBits: 8 as const,
  })
  advanceGeneration()
  mockDiagnostics.protocols.modbus.unitId = mockModbusConfiguration.unitId
  mockDiagnostics.protocols.modbus.baudRate = mockModbusConfiguration.baudRate
  return structuredClone(mockModbusConfiguration)
}

export async function setMockModbusRole(role: ModbusConfiguration['role']): Promise<ModbusConfiguration> {
  await wait(200)
  mockModbusConfiguration.role = role
  return structuredClone(mockModbusConfiguration)
}

export async function saveMockKnxConfiguration(configuration: KnxConfigurationUpdate): Promise<KnxConfiguration> {
  await wait(300)
  assertGeneration(configuration.expectedGeneration)
  const { expectedGeneration, ...replacement } = configuration
  void expectedGeneration
  Object.assign(mockKnxConfiguration, structuredClone(replacement))
  advanceGeneration()
  mockDiagnostics.protocols.knx.enabled = mockKnxConfiguration.enabled
  mockDiagnostics.protocols.knx.individualAddress = mockKnxConfiguration.individualAddress || null
  return structuredClone(mockKnxConfiguration)
}

export async function saveMockWifiProfile(profile: WifiProfileUpdate): Promise<WifiManagement> {
  await wait(300)
  assertGeneration(profile.expectedGeneration)
  const current = mockWifiManagement.profiles[profile.index]
  mockWifiManagement.profiles[profile.index] = {
    index: profile.index, enabled: profile.enabled, ssid: profile.ssid,
    hasPassphrase: profile.clearPassphrase ? false : Boolean(profile.passphrase) || current.hasPassphrase,
    ipv4: structuredClone(profile.ipv4),
  }
  advanceGeneration()
  return structuredClone(mockWifiManagement)
}

export async function deleteMockWifiProfile(index: number, expectedGeneration: number): Promise<WifiManagement> {
  await wait(250)
  assertGeneration(expectedGeneration)
  mockWifiManagement.profiles.splice(index, 1)
  mockWifiManagement.profiles.push({ index: 2, enabled: false, ssid: '', hasPassphrase: false, ipv4: { mode: 'dhcp', address: '', subnetMask: '', gateway: '', dns: '' } })
  mockWifiManagement.profiles.forEach((profile, profileIndex) => { profile.index = profileIndex })
  if (mockWifiManagement.activeProfileIndex === index) mockWifiManagement.activeProfileIndex = null
  advanceGeneration()
  return structuredClone(mockWifiManagement)
}

export async function moveMockWifiProfile(index: number, toIndex: number, expectedGeneration: number): Promise<WifiManagement> {
  await wait(250)
  assertGeneration(expectedGeneration)
  const [profile] = mockWifiManagement.profiles.splice(index, 1)
  mockWifiManagement.profiles.splice(toIndex, 0, profile)
  mockWifiManagement.profiles.forEach((candidate, profileIndex) => { candidate.index = profileIndex })
  if (mockWifiManagement.activeProfileIndex === index) mockWifiManagement.activeProfileIndex = toIndex
  advanceGeneration()
  return structuredClone(mockWifiManagement)
}

export async function connectMockWifiProfile(index: number): Promise<WifiManagement> {
  await wait(450)
  if (!mockWifiManagement.profiles[index]?.enabled) throw new Error('wifi.profile_disabled')
  mockWifiManagement.activeProfileIndex = index
  mockWifiManagement.recoveryAp.active = false
  return structuredClone(mockWifiManagement)
}

export async function saveMockRecoveryAp(recoveryAp: Omit<WifiRecoveryAp, 'active'>, expectedGeneration: number): Promise<WifiManagement> {
  await wait(250)
  assertGeneration(expectedGeneration)
  mockWifiManagement.recoveryAp = { ...structuredClone(recoveryAp), active: mockWifiManagement.recoveryAp.active }
  advanceGeneration()
  return structuredClone(mockWifiManagement)
}

export async function saveMockUser(user: Omit<User, 'id'> & { id?: number }): Promise<User[]> {
  await wait(250)
  const saved = { ...user, id: user.id ?? Math.max(...mockUsers.map((candidate) => candidate.id), -1) + 1 }
  const index = mockUsers.findIndex((candidate) => candidate.id === saved.id)
  if (index >= 0) mockUsers[index] = saved
  else mockUsers.push(saved)
  return structuredClone(mockUsers)
}
