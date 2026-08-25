import { z } from 'zod'

export const relayStateSchema = z.enum(['on', 'off'])

export const relaySchema = z.object({
  id: z.number().int().nonnegative(),
  physicalLabel: z.string(),
  name: z.string().optional(),
  requestedState: relayStateSchema,
  appliedState: relayStateSchema,
  verification: z.enum(['gpio-write', 'contact-feedback', 'unavailable']),
  lastSource: z.enum(['safety', 'button', 'knx', 'modbus', 'web', 'cli', 'restore']),
  transitionSequence: z.number().int().nonnegative(),
  lastTransitionAgeMs: z.number().int().nonnegative(),
  fault: z.string().nullable(),
  lockedOut: z.boolean(),
  enabled: z.boolean(),
})

export const capabilitiesSchema = z.object({
  apiVersion: z.string(),
  minimumUiVersion: z.string(),
  versions: z.object({
    hardware: z.string(),
    firmware: z.string(),
    configuration: z.string(),
    api: z.string(),
    modbus: z.string(),
    knxApplication: z.string(),
    filesystem: z.string(),
  }),
  deviceId: z.string(),
  bootId: z.string(),
  model: z.string(),
  channels: z.array(z.object({
    id: z.number().int().nonnegative(),
    physicalLabel: z.string(),
    contactFeedback: z.boolean(),
  })),
  features: z.object({
    wifi: z.boolean(),
    ethernet: z.boolean(),
    modbus: z.boolean(),
    knx: z.boolean(),
    scenes: z.boolean(),
    timers: z.boolean(),
    remoteRestart: z.boolean(),
    remoteFactoryReset: z.boolean(),
    firmwareUpdate: z.boolean(),
  }),
  permissions: z.array(z.string()),
})

export const deviceSchema = z.object({
  name: z.string(),
  model: z.string(),
  serialSuffix: z.string(),
  firmwareVersion: z.string(),
  buildId: z.string(),
  uptimeMs: z.number().int().nonnegative(),
  lifecycle: z.enum(['booting', 'configuring', 'operational', 'degraded', 'fault', 'restarting']),
  lifecycleReason: z.string(),
  configurationGeneration: z.number().int().nonnegative(),
})

export const networkSchema = z.object({
  state: z.enum(['disabled', 'connecting', 'online', 'recovery-ap', 'connecting-ethernet', 'online-ethernet']),
  activeTransport: z.enum(['none', 'wifi', 'ethernet']),
  ipv4Address: z.string().nullable(),
  rssi: z.number().int(),
  activeProfileIndex: z.number().int().nullable(),
  recoveryApActive: z.boolean(),
  lastConnectedAgeMs: z.number().int().nonnegative().nullable(),
})

export const wifiProfileSchema = z.object({
  index: z.number().int().min(0).max(2),
  enabled: z.boolean(),
  ssid: z.string().max(32),
  hasPassphrase: z.boolean(),
  ipv4: z.object({
    mode: z.enum(['dhcp', 'static']),
    address: z.string(),
    subnetMask: z.string(),
    gateway: z.string(),
    dns: z.string(),
  }),
})

export const wifiManagementSchema = z.object({
  generation: z.number().int().nonnegative(),
  activeProfileIndex: z.number().int().min(0).max(2).nullable(),
  scan: z.object({
    state: z.enum(['idle', 'scanning', 'complete', 'failed']),
    sequence: z.number().int().nonnegative(),
    results: z.array(z.object({
      ssid: z.string().max(32),
      rssi: z.number().int(),
      channel: z.number().int().min(1).max(14),
      secured: z.boolean(),
    })).max(16),
  }),
  profiles: z.array(wifiProfileSchema).max(3),
  recoveryAp: z.object({
    enabled: z.boolean(),
	ssidPrefix: z.string().min(1).max(23),
	channel: z.number().int().min(1).max(13),
	timeoutMs: z.number().int().nonnegative(),
	remainActiveWhileOffline: z.boolean(),
    active: z.boolean(),
  }),
})

export const modbusConfigurationSchema = z.object({
  generation: z.number().int().nonnegative(),
  role: z.enum(['server', 'client']),
  unitId: z.number().int().min(1).max(247),
  baudRate: z.union([z.literal(9600), z.literal(19200), z.literal(38400), z.literal(57600), z.literal(115200)]),
  parity: z.enum(['none', 'even', 'odd']),
  dataBits: z.literal(8),
  stopBits: z.union([z.literal(1), z.literal(2)]),
})

const knxIndividualAddressSchema = z.string().regex(/^$|^(?:[0-9]|1[0-5])\.(?:[0-9]|1[0-5])\.(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/)
const knxGroupAddressSchema = z.string().regex(/^$|^(?:[0-9]|[12][0-9]|3[01])\/[0-7]\/(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$/)
const knxPeriodicIntervalSchema = z.number().int().min(0).max(86_400_000).refine((value) => value === 0 || value >= 10_000)

export const knxChannelConfigurationSchema = z.object({
  index: z.number().int().min(0).max(5),
  switchGroupAddress: knxGroupAddressSchema,
  statusGroupAddress: knxGroupAddressSchema,
  faultGroupAddress: knxGroupAddressSchema,
  commandPolarityInverted: z.boolean(),
  statusPolarityInverted: z.boolean(),
  sendStatusAfterStartup: z.boolean(),
  participatesInCentralSwitch: z.boolean(),
  participatesInCentralOff: z.boolean(),
})

export const knxConfigurationSchema = z.object({
  generation: z.number().int().nonnegative(),
  enabled: z.boolean(),
  individualAddress: knxIndividualAddressSchema,
  startupTransmitDelayMs: z.number().int().min(0).max(60_000),
  minimumTelegramIntervalMs: z.number().int().min(20).max(1000),
  cyclicStatusIntervalMs: knxPeriodicIntervalSchema,
  heartbeatIntervalMs: knxPeriodicIntervalSchema,
  readSwitchObject: z.boolean(),
  heartbeatGroupAddress: knxGroupAddressSchema,
  centralSwitchGroupAddress: knxGroupAddressSchema,
  centralOffGroupAddress: knxGroupAddressSchema,
  deviceFaultGroupAddress: knxGroupAddressSchema,
  channels: z.array(knxChannelConfigurationSchema).length(6),
})

export const userSchema = z.object({
  id: z.number().int().nonnegative(),
  username: z.string(),
  role: z.enum(['administrator', 'guest']),
  enabled: z.boolean(),
})

export const sessionSchema = z.object({
  user: userSchema,
  expiresInMs: z.number().int().positive(),
  csrfToken: z.string(),
  permissions: z.array(z.string()),
})

export const otaStatusSchema = z.object({
  state: z.enum(['idle', 'checking', 'available', 'downloading', 'verifying', 'ready', 'installing', 'failed']),
  progressPercent: z.number().min(0).max(100),
  currentVersion: z.string(),
  buildEnvironment: z.string(),
  availableVersion: z.string().nullable(),
  error: z.string().nullable(),
})

export const diagnosticsSchema = z.object({
  device: z.object({
    uptimeMs: z.number().int().nonnegative(),
    bootCount: z.number().int().nonnegative(),
    resetReason: z.enum(['power-on', 'controlled-restart', 'brownout', 'watchdog', 'panic', 'repeated-boot', 'unknown']),
    lifecycle: z.enum(['operational', 'degraded']),
  }).optional(),
  persistentCounters: z.object({
    bootCount: z.number().int().nonnegative(),
    watchdogCount: z.number().int().nonnegative(),
    brownoutCount: z.number().int().nonnegative(),
    configErrorCount: z.number().int().nonnegative(),
    otaFailureCount: z.number().int().nonnegative(),
    networkFailureCount: z.number().int().nonnegative(),
    modbusErrorCount: z.number().int().nonnegative(),
    knxErrorCount: z.number().int().nonnegative(),
    storageErrorCount: z.number().int().nonnegative(),
  }).optional(),
  firmware: z.object({ version: z.string(), buildId: z.string() }).optional(),
  hardware: z.object({ model: z.string(), revision: z.string() }).optional(),
  memory: z.object({
    freeHeapBytes: z.number().int().nonnegative(),
    minimumFreeHeapBytes: z.number().int().nonnegative(),
    largestFreeHeapBlockBytes: z.number().int().nonnegative(),
    psram: z.object({
      available: z.boolean(),
      totalBytes: z.number().int().nonnegative(),
      freeBytes: z.number().int().nonnegative(),
      minimumFreeBytes: z.number().int().nonnegative(),
    }),
  }).optional(),
  cpu: z.object({ frequencyMhz: z.number().int().nonnegative(), coreCount: z.number().int().nonnegative() }).optional(),
  network: z.object({
    state: z.enum(['disabled', 'connecting', 'online', 'recovery-ap']),
    connected: z.boolean(),
    recoveryApActive: z.boolean(),
    wifiRssiDbm: z.number().int(),
    ipv4Address: z.string().nullable(),
  }).optional(),
  storage: z.object({
    filesystemAvailable: z.boolean(),
    settingsAvailable: z.boolean(),
    settingsHealthy: z.boolean(),
    configurationValid: z.boolean(),
    configurationGeneration: z.number().int().nonnegative(),
  }).optional(),
  faultState: z.object({ active: z.boolean(), activeCount: z.number().int().nonnegative() }).optional(),
  relays: z.array(z.object({
    id: z.number().int().nonnegative(),
    requestedState: relayStateSchema,
    appliedState: relayStateSchema,
    lockedOut: z.boolean(),
    faulted: z.boolean(),
    transitionSequence: z.number().int().nonnegative(),
  })).optional(),
  configurationValid: z.boolean(),
  persistenceHealthy: z.boolean(),
  taskWatchdogHealthy: z.boolean(),
  heapLowWaterMarkBytes: z.number().int().nonnegative(),
  commandCounters: z.object({
    accepted: z.number().int().nonnegative(),
    rejected: z.number().int().nonnegative(),
    queueFull: z.number().int().nonnegative(),
  }),
  faults: z.array(z.object({
    code: z.string(),
    severity: z.enum(['info', 'warning', 'critical']),
    summary: z.string(),
    occurrenceCount: z.number().int().positive(),
  })),
  protocols: z.object({
    modbus: z.object({
      available: z.boolean(),
      unitId: z.number().int(),
      baudRate: z.number().int(),
      validRequests: z.number().int().nonnegative(),
      errors: z.number().int().nonnegative(),
    }),
    knx: z.object({
      available: z.boolean(),
      enabled: z.boolean(),
      busOnline: z.boolean(),
      individualAddress: z.string().nullable(),
      validTelegrams: z.number().int().nonnegative(),
      errors: z.number().int().nonnegative(),
    }),
  }),
})

export const relayListSchema = z.object({
  bootId: z.string(),
  snapshotSequence: z.number().int().nonnegative(),
  relays: z.array(relaySchema),
})

export const commandResultSchema = z.object({
  correlationId: z.string(),
  result: z.enum(['queued', 'applied', 'idempotent', 'rejected', 'unknown']),
  channel: z.number().int().nonnegative().optional(),
  appliedState: relayStateSchema.optional(),
  sequence: z.number().int().nonnegative().optional(),
  reason: z.string().optional(),
})

export const operationSchema = z.object({
  operationId: z.string().regex(/^\d+$/),
  status: z.enum(['pending', 'applied', 'conflict', 'invalid', 'unavailable', 'rejected']),
})

export type Capabilities = z.infer<typeof capabilitiesSchema>
export type Device = z.infer<typeof deviceSchema>
export type NetworkStatus = z.infer<typeof networkSchema>
export type WifiManagement = z.infer<typeof wifiManagementSchema>
export type WifiProfile = z.infer<typeof wifiProfileSchema>
export type WifiRecoveryAp = WifiManagement['recoveryAp']
export type ModbusConfiguration = z.infer<typeof modbusConfigurationSchema>
export type ModbusConfigurationUpdate = Omit<ModbusConfiguration, 'generation' | 'role' | 'dataBits'> & {
  expectedGeneration: number
}
export type KnxConfiguration = z.infer<typeof knxConfigurationSchema>
export type KnxConfigurationUpdate = Omit<KnxConfiguration, 'generation'> & { expectedGeneration: number }
export type WifiProfileUpdate = Omit<WifiProfile, 'hasPassphrase'> & {
  expectedGeneration: number
  passphrase?: string
  clearPassphrase?: boolean
}
export type User = z.infer<typeof userSchema>
export type Session = z.infer<typeof sessionSchema>
export type OtaStatus = z.infer<typeof otaStatusSchema>
export type Diagnostics = z.infer<typeof diagnosticsSchema>
export type Relay = z.infer<typeof relaySchema>
export type RelayList = z.infer<typeof relayListSchema>
export type RelayState = z.infer<typeof relayStateSchema>
export type CommandResult = z.infer<typeof commandResultSchema>