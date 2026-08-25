import { QueryClient } from '@tanstack/react-query'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { connectLiveEvents } from './live'

class FakeWebSocket {
  static readonly OPEN = 1
  static instances: FakeWebSocket[] = []

  readonly url: string
  readyState = FakeWebSocket.OPEN
  readonly sent: string[] = []
  private readonly listeners = new Map<string, Array<(event: Event | MessageEvent | CloseEvent) => void>>()

  constructor(url: string) {
    this.url = url
    FakeWebSocket.instances.push(this)
  }

  addEventListener(type: string, listener: (event: Event | MessageEvent | CloseEvent) => void) {
    const listeners = this.listeners.get(type) ?? []
    listeners.push(listener)
    this.listeners.set(type, listeners)
  }

  send(data: string) {
    this.sent.push(data)
  }

  close(code = 1000, reason = '') {
    this.readyState = 3
    this.emit('close', new CloseEvent('close', { code, reason }))
  }

  emit(type: string, event: Event | MessageEvent | CloseEvent) {
    for (const listener of this.listeners.get(type) ?? []) listener(event)
  }
}

function frame(type: string, sequence: number, payload: object = {}) {
  return new MessageEvent('message', { data: JSON.stringify({ version: 1, type, sequence, payload }) })
}

describe('WebSocket live events', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    vi.spyOn(Math, 'random').mockReturnValue(0.5)
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
  })

  afterEach(() => {
    vi.useRealTimers()
    vi.unstubAllGlobals()
  })

  it('invalidates relay state, detects sequence gaps, and sends heartbeats', () => {
    const queryClient = new QueryClient()
    const invalidate = vi.spyOn(queryClient, 'invalidateQueries')
    const disconnect = connectLiveEvents(queryClient)
    const socket = FakeWebSocket.instances[0]

    expect(socket.url).toBe('ws://localhost:3000/api/v1/ws')
    socket.emit('open', new Event('open'))
    socket.emit('message', frame('relay.commandResult', 1))
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ['relays'] })

    socket.emit('message', frame('relay.stateChanged', 3))
    expect(invalidate).toHaveBeenCalledWith()

    vi.advanceTimersByTime(20_000)
    expect(JSON.parse(socket.sent[0])).toMatchObject({ version: 1, type: 'ping', sequence: 3 })
    disconnect()
  })

  it('does not reconnect after a terminal protocol-version error', () => {
    const queryClient = new QueryClient()
    connectLiveEvents(queryClient)
    const socket = FakeWebSocket.instances[0]

    socket.emit('message', frame('protocol.error', 0, { code: 'protocol_error' }))
    socket.close(1006)
    vi.advanceTimersByTime(60_000)

    expect(FakeWebSocket.instances).toHaveLength(1)
  })

  it('resynchronizes after a per-resource sequence gap', () => {
    const queryClient = new QueryClient()
    const invalidate = vi.spyOn(queryClient, 'invalidateQueries')
    connectLiveEvents(queryClient)
    const socket = FakeWebSocket.instances[0]

    socket.emit('message', frame('session.ready', 0, { bootId: 'boot-a' }))
    socket.emit('message', frame('relay.stateChanged', 1, { resource: 'relay:0', resourceSequence: 4 }))
    invalidate.mockClear()
    socket.emit('message', frame('relay.stateChanged', 2, { resource: 'relay:0', resourceSequence: 6 }))

    expect(invalidate).toHaveBeenCalledWith()
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ['relays'] })
  })

  it('resynchronizes when the device boot identity changes', () => {
    const queryClient = new QueryClient()
    const invalidate = vi.spyOn(queryClient, 'invalidateQueries')
    connectLiveEvents(queryClient)
    const socket = FakeWebSocket.instances[0]

    socket.emit('message', frame('session.ready', 0, { bootId: 'boot-a' }))
    invalidate.mockClear()
    socket.emit('message', frame('session.ready', 0, { bootId: 'boot-b' }))

    expect(invalidate).toHaveBeenCalledWith()
  })
})
