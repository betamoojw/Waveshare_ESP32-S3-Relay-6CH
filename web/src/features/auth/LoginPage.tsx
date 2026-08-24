import { useState } from 'react'
import { useMutation } from '@tanstack/react-query'
import { AlertTriangle, Eye, EyeOff, Zap } from 'lucide-react'
import { ApiError, api } from '../../api/client'
import type { Session } from '../../api/types'

type Props = {
  onAuthenticated: (session: Session) => void
  sessionEnded?: boolean
}

function loginError(error: Error | null) {
  if (error instanceof ApiError && error.status === 429) {
    return { title: 'Too many attempts', detail: 'Sign-in is temporarily locked. Wait five minutes before trying again.' }
  }
  if (error instanceof ApiError && error.status >= 500) {
    return { title: 'Device unavailable', detail: 'The controller cannot create a secure session right now. Try again shortly.' }
  }
  return { title: 'Sign-in failed', detail: 'The username or password is incorrect.' }
}

export function LoginPage({ onAuthenticated, sessionEnded = false }: Props) {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [passwordVisible, setPasswordVisible] = useState(false)
  const login = useMutation({
    mutationFn: () => api.createSession(username, password),
    onSuccess: (session) => {
      setPassword('')
      onAuthenticated(session)
    },
  })
  const error = loginError(login.error)

  return <main className="login-page">
    <form className="login-form" aria-labelledby="login-title" onSubmit={(event) => { event.preventDefault(); login.mutate() }}>
      <div className="brand-mark"><span><Zap size={20} fill="currentColor" /></span><strong>Switch<br />Actuator</strong></div>
      <div><p className="eyebrow">Secure management</p><h1 id="login-title">Sign in</h1></div>
      {sessionEnded && <div className="session-ended" role="status">Your secure session ended. Sign in again to continue.</div>}
      <label><span>Username</span><input required autoFocus maxLength={63} autoCapitalize="none" spellCheck={false} autoComplete="username" disabled={login.isPending} value={username} onChange={(event) => setUsername(event.target.value)} /></label>
      <label><span>Password</span><span className="password-input"><input required maxLength={128} type={passwordVisible ? 'text' : 'password'} autoComplete="current-password" disabled={login.isPending} value={password} onChange={(event) => setPassword(event.target.value)} /><button type="button" className="password-visibility" aria-label={passwordVisible ? 'Hide password' : 'Show password'} title={passwordVisible ? 'Hide password' : 'Show password'} disabled={login.isPending} onClick={() => setPasswordVisible((visible) => !visible)}>{passwordVisible ? <EyeOff size={18} /> : <Eye size={18} />}</button></span></label>
      <button className="primary-button" disabled={login.isPending}>{login.isPending ? 'Signing in...' : 'Sign in'}</button>
      {login.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>{error.title}</strong><span>{error.detail}</span></div></div>}
    </form>
  </main>
}