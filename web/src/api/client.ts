import type { ZodType } from 'zod'
import {
  capabilitiesSchema,
  commandResultSchema,
  deviceSchema,
  diagnosticsSchema,
  networkSchema,
  relayListSchema,
  type RelayState,
} from './types'
import {
  commandMockRelay,
  getMockRelays,
  mockCapabilities,
  mockDevice,
  mockDiagnostics,
  mockNetwork,
} from './mock'

const apiBase = '/api/v1'
export const previewMode = import.meta.env.DEV && import.meta.env.VITE_REAL_API !== 'true'

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
    headers: { Accept: 'application/json', ...(init?.headers ?? {}) },
    ...init,
  })
  const payload: unknown = response.status === 204 ? null : await response.json()
  if (!response.ok) {
    const error = payload as { error?: { code?: string; message?: string } }
    throw new ApiError(error.error?.code ?? 'request.failed', error.error?.message ?? 'The device rejected the request.', response.status)
  }
  return schema.parse(payload)
}

export const api = {
  capabilities: async () => previewMode ? mockCapabilities : request('/capabilities', capabilitiesSchema),
  device: async () => previewMode ? mockDevice : request('/device', deviceSchema),
  network: async () => previewMode ? mockNetwork : request('/network', networkSchema),
  relays: async () => previewMode ? getMockRelays() : request('/relays', relayListSchema),
  diagnostics: async () => previewMode ? mockDiagnostics : request('/diagnostics', diagnosticsSchema),
  commandRelay: async (channel: number, state: RelayState, expectedSequence: number) => {
    if (previewMode) return commandMockRelay(channel, state)
    return request(`/relays/${channel}/commands`, commandResultSchema, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Idempotency-Key': crypto.randomUUID() },
      body: JSON.stringify({ action: state === 'on' ? 'setOn' : 'setOff', expectedSequence }),
    })
  },
}