import { describe, expect, it } from 'vitest'
import {
  deleteMockWifiProfile,
  mockKnxConfiguration,
  mockModbusConfiguration,
  mockWifiManagement,
  moveMockWifiProfile,
  saveMockWifiProfile,
  saveMockKnxConfiguration,
  saveMockModbusConfiguration,
} from './mock'

describe('Wi-Fi management contract', () => {
  it('preserves redacted secrets and advances generation across ordered mutations', async () => {
    const startingGeneration = mockWifiManagement.generation
    const saved = await saveMockWifiProfile({
      index: 0,
      enabled: true,
      ssid: 'Production-LAN',
      ipv4: { mode: 'dhcp', address: '', subnetMask: '', gateway: '', dns: '' },
      expectedGeneration: startingGeneration,
    })
    expect(saved.profiles[0].hasPassphrase).toBe(true)
    expect(saved.generation).toBe(startingGeneration + 1)

    await expect(saveMockWifiProfile({
      index: 0,
      enabled: true,
      ssid: 'Stale-Write',
      ipv4: saved.profiles[0].ipv4,
      expectedGeneration: startingGeneration,
    })).rejects.toThrow('configuration.generation_conflict')

    const moved = await moveMockWifiProfile(0, 1, saved.generation)
    expect(moved.profiles.map((profile) => profile.index)).toEqual([0, 1, 2])
    expect(moved.profiles[1].ssid).toBe('Production-LAN')

    const removed = await deleteMockWifiProfile(1, moved.generation)
    expect(removed.profiles).toHaveLength(3)
    expect(removed.profiles[2]).toMatchObject({ index: 2, enabled: false, ssid: '', hasPassphrase: false })
  })

  it('rejects stale writes across Modbus and Wi-Fi configuration surfaces', async () => {
    const generation = mockModbusConfiguration.generation
    const saved = await saveMockModbusConfiguration({
      unitId: 11,
      baudRate: 57600,
      parity: 'even',
      stopBits: 1,
      expectedGeneration: generation,
    })
    expect(saved.generation).toBe(generation + 1)
    expect(mockWifiManagement.generation).toBe(saved.generation)

    await expect(saveMockWifiProfile({
      index: 0,
      enabled: true,
      ssid: 'Stale-After-Modbus',
      ipv4: mockWifiManagement.profiles[0].ipv4,
      expectedGeneration: generation,
    })).rejects.toThrow('configuration.generation_conflict')
  })

  it('rejects stale writes across KNX and Wi-Fi configuration surfaces', async () => {
    const generation = mockKnxConfiguration.generation
    const { generation: snapshotGeneration, ...configuration } = mockKnxConfiguration
    expect(snapshotGeneration).toBe(generation)
    const saved = await saveMockKnxConfiguration({
      ...structuredClone(configuration),
      enabled: !configuration.enabled,
      expectedGeneration: generation,
    })
    expect(saved.generation).toBe(generation + 1)
    expect(mockWifiManagement.generation).toBe(saved.generation)

    await expect(saveMockWifiProfile({
      index: 0,
      enabled: true,
      ssid: 'Stale-After-KNX',
      ipv4: mockWifiManagement.profiles[0].ipv4,
      expectedGeneration: generation,
    })).rejects.toThrow('configuration.generation_conflict')
  })
})
