import { afterEach, describe, expect, it, vi } from 'vitest'
import { api, createApiRequestHeaders, setSessionCsrfToken, setUnauthorizedHandler } from './client'

describe('API request security headers', () => {
  afterEach(() => {
    setSessionCsrfToken(undefined)
    setUnauthorizedHandler(undefined)
    vi.unstubAllGlobals()
  })

  it('adds the in-memory CSRF token only to unsafe methods', () => {
    setSessionCsrfToken('session-csrf')

    expect(createApiRequestHeaders().has('X-CSRF-Token')).toBe(false)
    expect(createApiRequestHeaders({ method: 'GET' }).has('X-CSRF-Token')).toBe(false)
    expect(createApiRequestHeaders({ method: 'POST' }).get('X-CSRF-Token')).toBe('session-csrf')
    expect(createApiRequestHeaders({ method: 'DELETE' }).get('X-CSRF-Token')).toBe('session-csrf')
  })

  it('does not retain the token after the session is cleared', () => {
    setSessionCsrfToken('session-csrf')
    setSessionCsrfToken(undefined)

    expect(createApiRequestHeaders({ method: 'PUT' }).has('X-CSRF-Token')).toBe(false)
  })

  it('clears an authenticated session and notifies the application on 401', async () => {
    const unauthorized = vi.fn()
    setSessionCsrfToken('session-csrf')
    setUnauthorizedHandler(unauthorized)
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(new Response(JSON.stringify({
      error: { code: 'unauthorized', message: 'Authentication is required.' },
    }), { status: 401, headers: { 'Content-Type': 'application/json' } })))

    await expect(api.session()).rejects.toMatchObject({ status: 401 })

    expect(unauthorized).toHaveBeenCalledOnce()
    expect(createApiRequestHeaders({ method: 'POST' }).has('X-CSRF-Token')).toBe(false)
  })
})