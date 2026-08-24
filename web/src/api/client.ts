import { z, type ZodType } from 'zod'
import {
  capabilitiesSchema,
  commandResultSchema,
  deviceSchema,
  diagnosticsSchema,
  knxConfigurationSchema,
  modbusConfigurationSchema,
  networkSchema,
  operationSchema,
  otaStatusSchema,
  relayListSchema,
  sessionSchema,
  userSchema,
  wifiManagementSchema,
  type WifiProfileUpdate,
  type WifiRecoveryAp,
  type KnxConfigurationUpdate,
  type ModbusConfigurationUpdate,
  type RelayState,
  type Session,
} from './types'
import {
  commandMockRelay,
  getMockRelays,
  mockCapabilities,
  mockDevice,
  mockDiagnostics,
  mockKnxConfiguration,
  mockModbusConfiguration,
  mockNetwork,
  mockOtaStatus,
  mockUsers,
  mockWifiManagement,
  saveMockWifiProfile,
  connectMockWifiProfile,
  deleteMockWifiProfile,
  moveMockWifiProfile,
  saveMockKnxConfiguration,
  saveMockModbusConfiguration,
  setMockModbusRole,
  saveMockRecoveryAp,
  saveMockUser,
} from './mock'

const apiBase = '/api/v1'
export const previewMode = import.meta.env.DEV && import.meta.env.VITE_REAL_API !== 'true'
let sessionCsrfToken: string | undefined
let unauthorizedHandler: (() => void) | undefined

export function setSessionCsrfToken(token: string | undefined) {
  sessionCsrfToken = token
}

export function setUnauthorizedHandler(handler: (() => void) | undefined) {
  unauthorizedHandler = handler
}

export function createApiRequestHeaders(init?: RequestInit): Headers {
  const headers = new Headers(init?.headers)
  headers.set('Accept', 'application/json')
  const method = (init?.method ?? 'GET').toUpperCase()
  if (!['GET', 'HEAD', 'OPTIONS'].includes(method) && sessionCsrfToken !== undefined) {
    headers.set('X-CSRF-Token', sessionCsrfToken)
  }
  return headers
}

export class ApiError extends Error {
  readonly code: string
  readonly status: number

  constructor(code: string, message: string, status: number) {
    super(message)
    this.code = code
    this.status = status
  }
}

async function request<T>(path: string, schema: ZodType<T>, init?: RequestInit): Promise<T> {
  const response = await fetch(`${apiBase}${path}`, {
    credentials: 'same-origin',
    ...init,
    headers: createApiRequestHeaders(init),
  })
  const payload: unknown = response.status === 204 ? null : await response.json()
  if (!response.ok) {
    const error = payload as { error?: { code?: string; message?: string } }
    if (response.status === 401 && sessionCsrfToken !== undefined) {
      setSessionCsrfToken(undefined)
      unauthorizedHandler?.()
    }
    throw new ApiError(error.error?.code ?? 'request.failed', error.error?.message ?? 'The device rejected the request.', response.status)
  }
  return schema.parse(payload)
}

async function submitOperation(path: string, init: RequestInit): Promise<void> {
  const operation = await request(path, operationSchema, init)
  for (let attempt = 0; attempt < 50; attempt += 1) {
    const current = attempt === 0 ? operation : await request(`/operations/${operation.operationId}`, operationSchema)
    if (current.status === 'applied') return
    if (current.status !== 'pending') {
      const status = current.status === 'conflict' ? 409 : current.status === 'invalid' ? 422 : 503
      throw new ApiError(`operation.${current.status}`, 'The device could not apply the operation.', status)
    }
    await new Promise((resolve) => setTimeout(resolve, 100))
  }
  throw new ApiError('operation.timeout', 'The operation outcome is not yet known.', 504)
}

async function submitWifiOperation(path: string, init: RequestInit) {
  await submitOperation(path, init)
  return request('/network/wifi', wifiManagementSchema)
}

async function submitModbusOperation(path: string, init: RequestInit) {
  await submitOperation(path, init)
  return request('/protocols/modbus', modbusConfigurationSchema)
}

async function submitKnxOperation(path: string, init: RequestInit) {
  await submitOperation(path, init)
  return request('/protocols/knx', knxConfigurationSchema)
}

export const api = {
  session: async (): Promise<Session> => {
    const session = await request('/session', sessionSchema)
    setSessionCsrfToken(session.csrfToken)
    return session
  },
  createSession: async (username: string, password: string): Promise<Session> => {
    const session = await request('/session', sessionSchema, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ username, password }),
    })
    setSessionCsrfToken(session.csrfToken)
    return session
  },
  deleteSession: async () => {
    await request('/session', z.null(), { method: 'DELETE' })
    setSessionCsrfToken(undefined)
  },
  capabilities: async () => previewMode ? mockCapabilities : request('/capabilities', capabilitiesSchema),
  device: async () => previewMode ? mockDevice : request('/device', deviceSchema),
  network: async () => previewMode ? mockNetwork : request('/network', networkSchema),
  relays: async () => previewMode ? getMockRelays() : request('/relays', relayListSchema),
  diagnostics: async () => previewMode ? mockDiagnostics : request('/diagnostics', diagnosticsSchema),
  modbusConfiguration: async () => previewMode ? structuredClone(mockModbusConfiguration) : request('/protocols/modbus', modbusConfigurationSchema),
  saveModbusConfiguration: async (configuration: ModbusConfigurationUpdate) => {
    if (previewMode) return saveMockModbusConfiguration(configuration)
    return submitModbusOperation('/protocols/modbus', {
      method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(configuration),
    })
  },
  setModbusRole: async (role: 'server' | 'client') => {
    if (previewMode) return setMockModbusRole(role)
    return submitModbusOperation('/protocols/modbus/role', {
      method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ role }),
    })
  },
  knxConfiguration: async () => previewMode ? structuredClone(mockKnxConfiguration) : request('/protocols/knx', knxConfigurationSchema),
  saveKnxConfiguration: async (configuration: KnxConfigurationUpdate) => {
    if (previewMode) return saveMockKnxConfiguration(configuration)
    return submitKnxOperation('/protocols/knx', {
      method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(configuration),
    })
  },
  wifiManagement: async () => previewMode ? structuredClone(mockWifiManagement) : request('/network/wifi', wifiManagementSchema),
  scanWifi: async () => {
    if (previewMode) return structuredClone(mockWifiManagement)
    return submitWifiOperation('/network/wifi/scan', { method: 'POST' })
  },
  saveWifiProfile: async (profile: WifiProfileUpdate) => {
    if (previewMode) return saveMockWifiProfile(profile)
    return submitWifiOperation(`/network/wifi/profiles/${profile.index}`, {
      method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(profile),
    })
  },
  deleteWifiProfile: async (index: number, expectedGeneration: number) => {
    if (previewMode) return deleteMockWifiProfile(index, expectedGeneration)
    return submitWifiOperation(`/network/wifi/profiles/${index}`, {
      method: 'DELETE', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ expectedGeneration }),
    })
  },
  moveWifiProfile: async (index: number, toIndex: number, expectedGeneration: number) => {
    if (previewMode) return moveMockWifiProfile(index, toIndex, expectedGeneration)
    return submitWifiOperation(`/network/wifi/profiles/${index}/move`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ toIndex, expectedGeneration }),
    })
  },
  connectWifiProfile: async (index: number) => {
    if (previewMode) return connectMockWifiProfile(index)
    return submitWifiOperation(`/network/wifi/profiles/${index}/connect`, { method: 'POST' })
  },
  saveRecoveryAp: async (recoveryAp: Omit<WifiRecoveryAp, 'active'>, expectedGeneration: number) => {
    if (previewMode) return saveMockRecoveryAp(recoveryAp, expectedGeneration)
    return submitWifiOperation('/network/wifi/recovery-ap', {
      method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ...recoveryAp, expectedGeneration }),
    })
  },
  users: async () => previewMode ? structuredClone(mockUsers) : request('/users', userSchema.array()),
  saveUser: async (user: { id?: number; username: string; role: 'administrator' | 'guest'; enabled: boolean; password?: string }) => {
    if (previewMode) return saveMockUser(user)
    await submitOperation(user.id === undefined ? '/users' : `/users/${user.id}`, {
      method: user.id === undefined ? 'POST' : 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(user),
    })
    return request('/users', userSchema.array())
  },
  otaStatus: async () => previewMode ? structuredClone(mockOtaStatus) : request('/maintenance/firmware', otaStatusSchema),
  checkForUpdate: async () => {
    if (previewMode) return { ...mockOtaStatus, state: 'available' as const, availableVersion: '1.01' }
    return request('/maintenance/firmware/check', otaStatusSchema, { method: 'POST' })
  },
  installRemoteUpdate: async (releaseUrl: string) => {
    if (previewMode) return { ...mockOtaStatus, state: 'downloading' as const, progressPercent: 8, availableVersion: '1.01' }
    return request('/maintenance/firmware/remote', otaStatusSchema, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ releaseUrl }),
    })
  },
  uploadFirmware: async (firmware: File) => {
    if (previewMode) return { ...mockOtaStatus, state: 'verifying' as const, progressPercent: 100 }
    return request('/maintenance/firmware/upload', otaStatusSchema, {
      method: 'POST', headers: { 'Content-Type': 'application/octet-stream', 'X-Firmware-Filename': firmware.name }, body: firmware,
    })
  },
  restart: async () => submitOperation('/maintenance/restart', { method: 'POST' }),
  commandRelay: async (channel: number, state: RelayState, expectedSequence: number) => {
    if (previewMode) return commandMockRelay(channel, state)
    return request(`/relays/${channel}/commands`, commandResultSchema, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Idempotency-Key': crypto.randomUUID() },
      body: JSON.stringify({ action: state === 'on' ? 'setOn' : 'setOff', expectedSequence }),
    })
  },
}