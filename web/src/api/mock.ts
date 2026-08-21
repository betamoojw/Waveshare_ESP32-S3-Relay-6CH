import type { Capabilities, CommandResult, Device, Diagnostics, NetworkStatus, RelayList, RelayState } from './types'

export const mockCapabilities: Capabilities = {
  apiVersion: '1.0',
  minimumUiVersion: '1.0.0',
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
    firmwareUpdate: false,
  },
  permissions: ['relay:read', 'relay:command', 'diagnostics:read', 'configuration:read'],
}

export const mockDevice: Device = {
  name: 'Plant room actuator',
  model: mockCapabilities.model,
  serialSuffix: '0001',
  firmwareVersion: '1.00',
  buildId: 'ws_esp32-s3-relay-6ch',
  uptimeMs: 16_620_000,
  lifecycle: 'operational',
  lifecycleReason: 'Configuration valid',
  configurationGeneration: 12,
}

export const mockNetwork: NetworkStatus = {
  state: 'online',
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