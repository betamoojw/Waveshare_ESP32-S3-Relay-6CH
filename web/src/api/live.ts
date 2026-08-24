import type { QueryClient } from '@tanstack/react-query'

const protocolVersion = 1
const heartbeatIntervalMs = 20_000
const fallbackPollIntervalMs = 15_000
const maximumReconnectDelayMs = 30_000

type LiveEnvelope = {
  version: number
  type: string
  sequence: number
  payload?: {
    bootId?: string
    code?: string
    resource?: string
    resourceSequence?: number
  }
}

function parseEnvelope(data: unknown): LiveEnvelope | undefined {
  if (typeof data !== 'string') return undefined
  try {
    const value = JSON.parse(data) as Partial<LiveEnvelope>
    if (value.version !== protocolVersion || typeof value.type !== 'string' ||
      typeof value.sequence !== 'number' || !Number.isSafeInteger(value.sequence) || value.sequence < 0) return undefined
    const payload = value.payload as LiveEnvelope['payload']
    if (payload?.resourceSequence !== undefined &&
      (!Number.isSafeInteger(payload.resourceSequence) || payload.resourceSequence < 0 || typeof payload.resource !== 'string')) return undefined
    return value as LiveEnvelope
  } catch {
    return undefined
  }
}

function invalidateForEvent(queryClient: QueryClient, type: string) {
  if (type === 'relay.snapshot' || type === 'relay.stateChanged' || type === 'relay.commandResult') {
    void queryClient.invalidateQueries({ queryKey: ['relays'] })
  } else if (type === 'network.snapshot' || type === 'network.stateChanged') {
    void queryClient.invalidateQueries({ queryKey: ['network'] })
  } else if (type === 'wifi.scanStarted' || type === 'wifi.scanCompleted') {
    void queryClient.invalidateQueries({ queryKey: ['wifi-management'] })
  } else if (type === 'configuration.changed') {
    void queryClient.invalidateQueries({ queryKey: ['device'] })
    void queryClient.invalidateQueries({ queryKey: ['wifi-management'] })
  } else if (type === 'diagnostics.changed') {
    void queryClient.invalidateQueries({ queryKey: ['diagnostics'] })
  } else if (type === 'ota.progress') {
    void queryClient.invalidateQueries({ queryKey: ['ota-status'] })
  }
}

export function connectLiveEvents(queryClient: QueryClient): () => void {
  let socket: WebSocket | undefined
  let stopped = false
  let retryCount = 0
  let latestSequence = 0
  let bootId: string | undefined
  const resourceSequences = new Map<string, number>()
  let heartbeatTimer: ReturnType<typeof setInterval> | undefined
  let reconnectTimer: ReturnType<typeof setTimeout> | undefined
  let fallbackTimer: ReturnType<typeof setInterval> | undefined

  const stopHeartbeat = () => {
    if (heartbeatTimer !== undefined) clearInterval(heartbeatTimer)
    heartbeatTimer = undefined
  }
  const stopFallback = () => {
    if (fallbackTimer !== undefined) clearInterval(fallbackTimer)
    fallbackTimer = undefined
  }
  const resynchronize = () => {
    latestSequence = 0
    resourceSequences.clear()
    void queryClient.invalidateQueries()
  }
  const startFallback = () => {
    if (fallbackTimer !== undefined) return
    fallbackTimer = setInterval(() => void queryClient.invalidateQueries(), fallbackPollIntervalMs)
  }
  const scheduleReconnect = () => {
    if (stopped || reconnectTimer !== undefined) return
    startFallback()
    const exponentialDelay = Math.min(1_000 * 2 ** retryCount, maximumReconnectDelayMs)
    const delay = Math.round(exponentialDelay * (0.75 + Math.random() * 0.5))
    retryCount += 1
    reconnectTimer = setTimeout(() => {
      reconnectTimer = undefined
      open()
    }, delay)
  }
  const open = () => {
    if (stopped) return
    const scheme = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    socket = new WebSocket(`${scheme}//${window.location.host}/api/v1/ws`)
    socket.addEventListener('open', () => {
      retryCount = 0
      stopFallback()
      heartbeatTimer = setInterval(() => {
        if (socket?.readyState === WebSocket.OPEN) {
          socket.send(JSON.stringify({ version: protocolVersion, type: 'ping', sequence: latestSequence, payload: {} }))
        }
      }, heartbeatIntervalMs)
    })
    socket.addEventListener('message', (event) => {
      const envelope = parseEnvelope(event.data)
      if (envelope === undefined) {
        socket?.close(1002, 'Invalid protocol frame')
        return
      }
      if (envelope.type === 'protocol.error') {
        if (envelope.payload?.code === 'protocol.version_mismatch') stopped = true
        return
      }
      if (envelope.type === 'session.ready' && envelope.payload?.bootId !== undefined) {
        if (bootId !== undefined && bootId !== envelope.payload.bootId) resynchronize()
        bootId = envelope.payload.bootId
      }
      const resource = envelope.payload?.resource
      const resourceSequence = envelope.payload?.resourceSequence
      const previousResourceSequence = resource === undefined ? undefined : resourceSequences.get(resource)
      const hasResourceGap = resourceSequence !== undefined && previousResourceSequence !== undefined &&
        resourceSequence !== previousResourceSequence + 1
      if (envelope.type === 'resync.required' ||
        (envelope.sequence > 0 && latestSequence > 0 && envelope.sequence !== latestSequence + 1) || hasResourceGap) {
        resynchronize()
      }
      if (envelope.sequence > 0) latestSequence = envelope.sequence
      if (resource !== undefined && resourceSequence !== undefined) resourceSequences.set(resource, resourceSequence)
      invalidateForEvent(queryClient, envelope.type)
    })
    socket.addEventListener('close', (event) => {
      stopHeartbeat()
      socket = undefined
      if ([1002, 1008, 4001, 4003, 4401, 4403, 4406].includes(event.code)) stopped = true
      else scheduleReconnect()
    })
    socket.addEventListener('error', () => socket?.close())
  }

  open()
  return () => {
    stopped = true
    stopHeartbeat()
    stopFallback()
    if (reconnectTimer !== undefined) clearTimeout(reconnectTimer)
    socket?.close(1000, 'Page closed')
  }
}