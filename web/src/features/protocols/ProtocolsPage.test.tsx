import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { Capabilities, Diagnostics, KnxConfiguration, ModbusConfiguration } from '../../api/types'
import { ProtocolsPage } from './ProtocolsPage'

const apiMocks = vi.hoisted(() => ({
  capabilities: vi.fn(),
  diagnostics: vi.fn(),
  modbusConfiguration: vi.fn(),
  saveModbusConfiguration: vi.fn(),
  setModbusRole: vi.fn(),
  knxConfiguration: vi.fn(),
  saveKnxConfiguration: vi.fn(),
}))

vi.mock('../../api/client', () => ({
  api: apiMocks,
  ApiError: class ApiError extends Error {
    readonly status: number
    constructor(message: string, status: number) { super(message); this.status = status }
  },
}))

const capabilities: Capabilities = {
  apiVersion: '1.0', minimumUiVersion: '1.0.0', versions: { hardware: 'HW-A01', firmware: 'FW-1.4.0', configuration: 'CFG-4', api: 'API-v1', modbus: 'MODBUS-v1', knxApplication: 'KNX-APP-v1', filesystem: 'FS-v1' }, deviceId: 'device', bootId: 'boot', model: 'test', channels: [],
  features: { wifi: true, ethernet: false, modbus: true, knx: true, scenes: false, timers: false, remoteRestart: true, remoteFactoryReset: false, firmwareUpdate: false },
  permissions: ['configuration:read', 'configuration:write'],
}

const diagnostics: Diagnostics = {
  configurationValid: true,
  persistenceHealthy: true,
  taskWatchdogHealthy: true,
  heapLowWaterMarkBytes: 100_000,
  commandCounters: { accepted: 1, rejected: 0, queueFull: 0 },
  faults: [],
  protocols: {
    modbus: { available: true, unitId: 10, baudRate: 115200, validRequests: 20, errors: 0 },
    knx: { available: true, enabled: true, busOnline: true, individualAddress: '1.1.1', validTelegrams: 10, errors: 0 },
  },
}

const configuration: ModbusConfiguration = {
  generation: 12,
  role: 'server',
  unitId: 10,
  baudRate: 115200,
  parity: 'none',
  dataBits: 8,
  stopBits: 1,
}

const knxConfiguration: KnxConfiguration = {
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

function renderPage() {
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false }, mutations: { retry: false } } })
  return render(<QueryClientProvider client={queryClient}><ProtocolsPage /></QueryClientProvider>)
}

describe('ProtocolsPage Modbus configuration', () => {
  afterEach(() => cleanup())

  beforeEach(() => {
    vi.clearAllMocks()
    apiMocks.capabilities.mockResolvedValue(structuredClone(capabilities))
    apiMocks.diagnostics.mockResolvedValue(structuredClone(diagnostics))
    apiMocks.modbusConfiguration.mockResolvedValue(structuredClone(configuration))
    apiMocks.saveModbusConfiguration.mockResolvedValue({ ...configuration, generation: 13, unitId: 11 })
    apiMocks.setModbusRole.mockResolvedValue({ ...configuration, role: 'client' })
    apiMocks.knxConfiguration.mockResolvedValue(structuredClone(knxConfiguration))
    apiMocks.saveKnxConfiguration.mockResolvedValue({ ...knxConfiguration, generation: 13, individualAddress: '1.1.21' })
  })

  it('submits changed server settings with the authoritative generation', async () => {
    renderPage()

    const unitId = await screen.findByRole('spinbutton', { name: /Unit ID/ })
    const save = screen.getByRole('button', { name: 'Save and restart' })
    expect(save).toBeDisabled()

    fireEvent.change(unitId, { target: { value: '11' } })
    expect(save).toBeEnabled()
    fireEvent.click(save)

    await waitFor(() => expect(apiMocks.saveModbusConfiguration).toHaveBeenCalledWith({
      unitId: 11,
      baudRate: 115200,
      parity: 'none',
      stopBits: 1,
      expectedGeneration: 12,
    }))
    expect(await screen.findByText('Saved')).toBeInTheDocument()
  })

  it('switches the active role without coupling it to the configuration generation', async () => {
    renderPage()

    const client = await screen.findByRole('button', { name: 'Client' })
    fireEvent.click(client)

    await waitFor(() => expect(apiMocks.setModbusRole).toHaveBeenCalledWith('client'))
    expect(client).toHaveAttribute('aria-pressed', 'true')
    expect(screen.getAllByText('Generation 12')).toHaveLength(2)
  })

  it('submits the complete KNX configuration with the authoritative generation', async () => {
    renderPage()

    const individualAddress = await screen.findByRole('textbox', { name: /Individual address/ })
    fireEvent.change(individualAddress, { target: { value: '1.1.21' } })
    fireEvent.click(screen.getByRole('button', { name: 'Save KNX and restart' }))

    await waitFor(() => expect(apiMocks.saveKnxConfiguration).toHaveBeenCalledWith(expect.objectContaining({
      enabled: true,
      individualAddress: '1.1.21',
      expectedGeneration: 12,
      channels: knxConfiguration.channels,
    })))
  })

  it('renders configuration as read only without write permission', async () => {
    apiMocks.capabilities.mockResolvedValue({ ...capabilities, permissions: ['configuration:read'] })
    renderPage()

    expect(await screen.findByRole('spinbutton', { name: /Unit ID/ })).toBeDisabled()
    expect(screen.getByRole('button', { name: 'Client' })).toBeDisabled()
    expect(screen.getByRole('button', { name: 'Save and restart' })).toBeDisabled()
    expect(screen.getByRole('button', { name: 'Save KNX and restart' })).toBeDisabled()
    expect(screen.getAllByText('Read only')).toHaveLength(2)
  })
})
