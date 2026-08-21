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
  state: z.enum(['disabled', 'connecting', 'online', 'recovery-ap']),
  ipv4Address: z.string().nullable(),
  rssi: z.number().int(),
  activeProfileIndex: z.number().int().nullable(),
  recoveryApActive: z.boolean(),
  lastConnectedAgeMs: z.number().int().nonnegative().nullable(),
})

export const diagnosticsSchema = z.object({
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

export type Capabilities = z.infer<typeof capabilitiesSchema>
export type Device = z.infer<typeof deviceSchema>
export type NetworkStatus = z.infer<typeof networkSchema>
export type Diagnostics = z.infer<typeof diagnosticsSchema>
export type Relay = z.infer<typeof relaySchema>
export type RelayList = z.infer<typeof relayListSchema>
export type RelayState = z.infer<typeof relayStateSchema>
export type CommandResult = z.infer<typeof commandResultSchema>