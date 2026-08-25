import { describe, expect, it } from 'vitest'
import { networkSchema } from './types'

const ethernetNetwork = {
  state: 'online-ethernet',
  activeTransport: 'ethernet',
  ipv4Address: '192.168.10.42',
  rssi: 0,
  activeProfileIndex: null,
  recoveryApActive: false,
  lastConnectedAgeMs: 1000,
}

describe('network transport contract', () => {
  it('accepts a future Ethernet adapter status', () => {
    expect(networkSchema.parse(ethernetNetwork)).toEqual(ethernetNetwork)
    expect(networkSchema.parse({
      ...ethernetNetwork,
      state: 'connecting-ethernet',
      activeTransport: 'none',
      ipv4Address: null,
      lastConnectedAgeMs: null,
    }).state).toBe('connecting-ethernet')
  })

  it('rejects an unknown transport', () => {
    expect(networkSchema.safeParse({ ...ethernetNetwork, activeTransport: 'usb' }).success).toBe(false)
  })
})