import { useEffect, useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { NavLink, Navigate, Route, Routes } from 'react-router-dom'
import { Activity, AlertTriangle, Cable, CircleUserRound, LogOut, PanelLeftClose, SlidersHorizontal, Wrench, Zap } from 'lucide-react'
import { api, previewMode, setSessionCsrfToken, setUnauthorizedHandler } from './api/client'
import { connectLiveEvents } from './api/live'
import { StatusBadge } from './components/StatusBadge'
import { RelayPage } from './features/relays/RelayPage'
import { SettingsPage } from './features/settings/SettingsPage'
import { MaintenancePage } from './features/maintenance/MaintenancePage'
import { ProtocolsPage } from './features/protocols/ProtocolsPage'
import { LoginPage } from './features/auth/LoginPage'
import './Dashboard.css'

const navigation = [
  { to: '/', label: 'Relays', icon: Zap, end: true },
  { to: '/protocols', label: 'Protocols', icon: Cable },
  { to: '/diagnostics', label: 'Diagnostics', icon: Activity },
  { to: '/settings', label: 'Settings', icon: SlidersHorizontal },
  { to: '/maintenance', label: 'Maintenance', icon: Wrench },
]

function useLiveEvents(enabled: boolean) {
  const queryClient = useQueryClient()
  useEffect(() => {
    if (!enabled || previewMode) return
    return connectLiveEvents(queryClient)
  }, [enabled, queryClient])
}

function DiagnosticsPage() {
  const diagnostics = useQuery({ queryKey: ['diagnostics'], queryFn: api.diagnostics })
  const device = useQuery({ queryKey: ['device'], queryFn: api.device })
  if (!diagnostics.data || !device.data) return <div className="page-state">Loading diagnostics...</div>
  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Runtime health</p><h1>Diagnostics</h1></div></header>
    <section className="definition-grid" aria-label="Device health"><div><span>Lifecycle</span><StatusBadge label={device.data.lifecycle} tone={device.data.lifecycle === 'operational' ? 'success' : 'warning'} /></div><div><span>Configuration</span><strong>{diagnostics.data.configurationValid ? 'Valid' : 'Invalid'}</strong></div><div><span>Persistence</span><strong>{diagnostics.data.persistenceHealthy ? 'Healthy' : 'Failure'}</strong></div><div><span>Task watchdog</span><strong>{diagnostics.data.taskWatchdogHealthy ? 'Healthy' : 'Fault'}</strong></div><div><span>Heap low-water mark</span><strong>{Math.round(diagnostics.data.heapLowWaterMarkBytes / 1024)} KiB</strong></div><div><span>Configuration generation</span><strong>#{device.data.configurationGeneration}</strong></div></section>
    <section className="data-section"><h2>Command counters</h2><div className="counter-row"><div><strong>{diagnostics.data.commandCounters.accepted.toLocaleString()}</strong><span>Accepted</span></div><div><strong>{diagnostics.data.commandCounters.rejected}</strong><span>Rejected</span></div><div><strong>{diagnostics.data.commandCounters.queueFull}</strong><span>Queue full</span></div></div></section>
    <section className="data-section"><h2>Active faults</h2>{diagnostics.data.faults.map((fault) => <div className="fault-row" key={fault.code}><AlertTriangle size={19} /><div><strong>{fault.summary}</strong><span>{fault.code} / occurred {fault.occurrenceCount} times</span></div><StatusBadge label={fault.severity} tone={fault.severity === 'critical' ? 'danger' : 'warning'} /></div>)}</section>
  </div>
}

function App() {
  const queryClient = useQueryClient()
  const [sessionEnded, setSessionEnded] = useState(false)
  const session = useQuery({ queryKey: ['session'], queryFn: api.session, retry: false, enabled: !previewMode && !sessionEnded })
  const authenticated = previewMode || (!sessionEnded && session.isSuccess)
  const capabilities = useQuery({ queryKey: ['capabilities'], queryFn: api.capabilities, retry: 1, enabled: authenticated })
  const device = useQuery({ queryKey: ['device'], queryFn: api.device, retry: 1, enabled: authenticated })
  const network = useQuery({ queryKey: ['network'], queryFn: api.network, refetchInterval: 30_000, retry: 1, enabled: authenticated })
  const logout = useMutation({ mutationFn: api.deleteSession, onSettled: () => {
    setSessionCsrfToken(undefined)
    setSessionEnded(true)
    queryClient.clear()
  } })
  useEffect(() => {
    setUnauthorizedHandler(() => {
      setSessionEnded(true)
      queryClient.clear()
    })
    return () => setUnauthorizedHandler(undefined)
  }, [queryClient])
  useEffect(() => {
    if (previewMode || sessionEnded || !session.data) return
    const timeout = window.setTimeout(() => {
      setSessionCsrfToken(undefined)
      setSessionEnded(true)
      queryClient.clear()
    }, session.data.expiresInMs)
    return () => window.clearTimeout(timeout)
  }, [queryClient, session.data, sessionEnded])
  useLiveEvents(authenticated && capabilities.isSuccess && device.isSuccess)
  if (!previewMode && session.isLoading) return <main className="boot-state">Checking secure session...</main>
  if (!authenticated) return <LoginPage sessionEnded={sessionEnded} onAuthenticated={(created) => {
    setSessionEnded(false)
    queryClient.setQueryData(['session'], created)
  }} />
  if (capabilities.isLoading || device.isLoading) return <main className="boot-state">Connecting to switch actuator...</main>
  if (!capabilities.data || !device.data) return <main className="boot-state boot-state--error"><AlertTriangle size={28} />Device API is unavailable. Relay controls are disabled.</main>

  return <div className="app-shell">
    <a className="skip-link" href="#main-content">Skip to content</a>
    <aside className="side-nav"><div className="brand-mark"><span><Zap size={20} fill="currentColor" /></span><strong>Switch<br />Actuator</strong></div><nav aria-label="Primary navigation">{navigation.map(({ to, label, icon: Icon, end }) => <NavLink key={to} to={to} end={end}><Icon size={20} /><span>{label}</span></NavLink>)}</nav><button className="collapse-control" type="button" aria-label="Collapse navigation"><PanelLeftClose size={19} /><span>Collapse</span></button></aside>
    <div className="app-frame">{previewMode && <div className="preview-banner"><AlertTriangle size={15} />Development preview data</div>}<header className="top-bar"><div className="device-identity"><strong>{device.data.name}</strong><span>{device.data.model} / ...{device.data.serialSuffix}</span></div><div className="top-status"><StatusBadge label={device.data.lifecycle} tone={device.data.lifecycle === 'operational' ? 'success' : 'warning'} /><StatusBadge label={network.data?.state === 'online' ? 'Live' : 'Offline'} tone={network.data?.state === 'online' ? 'success' : 'danger'} />{network.data?.ipv4Address && <span className="ip-address">{network.data.ipv4Address}</span>}</div><div className="account"><button className="icon-button" type="button" aria-label="Account menu" title="Account"><CircleUserRound size={21} /></button><button className="icon-button" type="button" aria-label="Sign out" title="Sign out" disabled={logout.isPending} onClick={() => logout.mutate()}><LogOut size={19} /></button></div></header>
      <main id="main-content"><Routes><Route path="/" element={<RelayPage />} /><Route path="/protocols" element={<ProtocolsPage />} /><Route path="/diagnostics" element={<DiagnosticsPage />} /><Route path="/settings" element={<SettingsPage />} /><Route path="/maintenance" element={<MaintenancePage />} /><Route path="*" element={<Navigate to="/" replace />} /></Routes></main>
      <nav className="bottom-nav" aria-label="Mobile navigation">{navigation.slice(0, 4).map(({ to, label, icon: Icon, end }) => <NavLink key={to} to={to} end={end}><Icon size={20} /><span>{label}</span></NavLink>)}</nav>
    </div>
  </div>
}

export default App
