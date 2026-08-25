import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { cleanup, render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { Session } from '../../api/types'
import { LoginPage } from './LoginPage'

const apiMocks = vi.hoisted(() => ({ createSession: vi.fn() }))

vi.mock('../../api/client', async (importOriginal) => {
  const original = await importOriginal<typeof import('../../api/client')>()
  return { ...original, api: { ...original.api, createSession: apiMocks.createSession } }
})

const session: Session = {
  user: { id: 1, username: 'admin', role: 'administrator', enabled: true },
  expiresInMs: 900_000,
  csrfToken: 'csrf-token',
  permissions: ['relay:read', 'configuration:write'],
}

function renderLogin(onAuthenticated = vi.fn(), sessionEnded = false) {
  const queryClient = new QueryClient({ defaultOptions: { mutations: { retry: false } } })
  render(<QueryClientProvider client={queryClient}><LoginPage onAuthenticated={onAuthenticated} sessionEnded={sessionEnded} /></QueryClientProvider>)
  return onAuthenticated
}

describe('LoginPage', () => {
  afterEach(() => cleanup())

  beforeEach(() => vi.clearAllMocks())

  it('submits credentials and returns the authoritative session', async () => {
    apiMocks.createSession.mockResolvedValue(session)
    const onAuthenticated = renderLogin()
    const user = userEvent.setup()

    await user.type(screen.getByRole('textbox', { name: 'Username' }), 'admin')
    await user.type(screen.getByLabelText('Password'), 'correct horse battery staple')
    await user.click(screen.getByRole('button', { name: 'Sign in' }))

    await waitFor(() => expect(apiMocks.createSession).toHaveBeenCalledWith('admin', 'correct horse battery staple'))
    expect(onAuthenticated).toHaveBeenCalledWith(session)
  })

  it('reveals the password only through the explicit visibility control', async () => {
    renderLogin()
    const user = userEvent.setup()
    const password = screen.getByLabelText('Password')
    expect(password).toHaveAttribute('type', 'password')

    await user.click(screen.getByRole('button', { name: 'Show password' }))
    expect(password).toHaveAttribute('type', 'text')
    expect(screen.getByRole('button', { name: 'Hide password' })).toBeInTheDocument()
  })

  it('shows recovery-oriented rate-limit and expired-session messages', async () => {
    const { ApiError } = await import('../../api/client')
    apiMocks.createSession.mockRejectedValue(new ApiError('busy', 'The device is busy; retry later.', 429))
    renderLogin(vi.fn(), true)
    const user = userEvent.setup()

    expect(screen.getByRole('status')).toHaveTextContent('secure session ended')
    await user.type(screen.getByRole('textbox', { name: 'Username' }), 'unknown')
    await user.type(screen.getByLabelText('Password'), 'not-the-password')
    await user.click(screen.getByRole('button', { name: 'Sign in' }))

    expect(await screen.findByRole('alert')).toHaveTextContent('Too many attempts')
    expect(screen.getByRole('alert')).not.toHaveTextContent('unknown')
  })
})