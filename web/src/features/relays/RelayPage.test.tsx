import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { fireEvent, render, screen, within } from '@testing-library/react'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import type { CommandResult, RelayList } from '../../api/types'
import { RelayPage } from './RelayPage'

const apiMocks = vi.hoisted(() => ({
  relays: vi.fn(),
  commandRelay: vi.fn(),
}))

vi.mock('../../api/client', () => ({ api: apiMocks }))

const relayList: RelayList = {
  bootId: 'test-boot',
  snapshotSequence: 1,
  relays: [{
    id: 0,
    physicalLabel: 'CH1',
    name: 'Test relay',
    requestedState: 'off',
    appliedState: 'off',
    verification: 'gpio-write',
    lastSource: 'web',
    transitionSequence: 4,
    lastTransitionAgeMs: 1000,
    fault: null,
    lockedOut: false,
    enabled: true,
  }],
}

function renderPage() {
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false }, mutations: { retry: false } } })
  return render(<QueryClientProvider client={queryClient}><RelayPage /></QueryClientProvider>)
}

describe('RelayPage', () => {
  beforeEach(() => {
    relayList.relays[0].requestedState = 'off'
    relayList.relays[0].appliedState = 'off'
    relayList.relays[0].transitionSequence = 4
    relayList.snapshotSequence = 1
    apiMocks.relays.mockImplementation(async () => structuredClone(relayList))
  })

  it('keeps the applied state off until the command result resolves', async () => {
    let resolveCommand: ((result: CommandResult) => void) | undefined
    apiMocks.commandRelay.mockImplementation(() => new Promise<CommandResult>((resolve) => { resolveCommand = resolve }))
    renderPage()

    const relay = await screen.findByRole('article', { name: 'Test relay' })
    fireEvent.click(within(relay).getByRole('button', { name: 'On' }))

    expect(within(relay).getByRole('strong')).toHaveTextContent('Off')
    expect(await within(relay).findByRole('button', { name: 'Pending' })).toBeDisabled()

    relayList.relays[0].requestedState = 'on'
    relayList.relays[0].appliedState = 'on'
    relayList.relays[0].transitionSequence = 5
    relayList.snapshotSequence = 2
    resolveCommand?.({ correlationId: 'web-1', result: 'applied', channel: 0, appliedState: 'on', sequence: 5 })

    expect(await within(relay).findByText('On', { selector: 'strong' })).toBeInTheDocument()
  })
})