import { useEffect } from 'react'
import { useQuery, useQueryClient } from '@tanstack/react-query'
import { NavLink, Navigate, Route, Routes } from 'react-router-dom'
import { Activity, AlertTriangle, Cable, CircleUserRound, Gauge, LogOut, PanelLeftClose, Radio, Settings, ShieldCheck, SlidersHorizontal, Wifi, Wrench, Zap } from 'lucide-react'
import { api, previewMode } from './api/client'
import { StatusBadge } from './components/StatusBadge'
import { RelayPage } from './features/relays/RelayPage'
import './Dashboard.css'

const navigation = [
  { to: '/', label: 'Relays', icon: Zap, end: true },
  { to: '/protocols', label: 'Protocols', icon: Cable },
  { to: '/diagnostics', label: 'Diagnostics', icon: Activity },
  { to: '/settings', label: 'Settings', icon: SlidersHorizontal },
  { to: '/maintenance', label: 'Maintenance', icon: Wrench },
]

function useLiveEvents() {
  const queryClient = useQueryClient()
  useEffect(() => {
    if (previewMode) return
    const stream = new EventSource('/api/v1/events', { withCredentials: true })
    const refreshRelays = () => queryClient.invalidateQueries({ queryKey: ['relays'] })
    stream.addEventListener('relay', refreshRelays)
    stream.addEventListener('relay-command', refreshRelays)
    stream.addEventListener('device', () => queryClient.invalidateQueries({ queryKey: ['device'] }))
    stream.addEventListener('resync', () => queryClient.invalidateQueries())
    return () => stream.close()
  }, [queryClient])
}

function ProtocolsPage() {
  const query = useQuery({ queryKey: ['diagnostics'], queryFn: api.diagnostics })
  if (!query.data) return <div className="page-state">Loading protocol state...</div>
  const { modbus, knx } = query.data.protocols
  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Fieldbus health</p><h1>Protocols</h1></div></header>
    <section className="data-section"><h2>Communication status</h2><div className="table-wrap"><table><caption className="sr-only">Protocol communication status</caption><thead><tr><th>Protocol</th><th>Status</th><th>Address</th><th>Valid traffic</th><th>Errors</th></tr></thead><tbody>
      <tr><td><span className="table-title"><Radio size={18} />Modbus RTU</span></td><td><StatusBadge label={modbus.available ? 'Available' : 'Unavailable'} tone={modbus.available ? 'success' : 'danger'} /></td><td>Unit {modbus.unitId} / {modbus.baudRate.toLocaleString()} baud</td><td>{modbus.validRequests.toLocaleString()}</td><td>{modbus.errors}</td></tr>
      <tr><td><span className="table-title"><Gauge size={18} />KNX/IP</span></td><td><StatusBadge label={knx.busOnline ? 'Bus online' : knx.enabled ? 'Degraded' : 'Disabled'} tone={knx.busOnline ? 'success' : 'warning'} /></td><td>{knx.individualAddress ?? 'Not configured'}</td><td>{knx.validTelegrams.toLocaleString()}</td><td>{knx.errors}</td></tr>
    </tbody></table></div></section>
    {!knx.busOnline && <div className="notice-band"><AlertTriangle size={18} /><div><strong>KNX bus communication is degraded</strong><span>Relay operation remains available through other configured sources.</span></div></div>}
  </div>
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

function SettingsPage() {
  return <div className="route-content"><header className="route-header"><div><p className="eyebrow">Generation-protected configuration</p><h1>Settings</h1></div><StatusBadge label="No staged changes" /></header><div className="settings-layout"><nav aria-label="Settings sections"><button className="is-active"><Settings size={17} />Device</button><button><Zap size={17} />Channels</button><button><ShieldCheck size={17} />Safety</button><button><Wifi size={17} />Network</button></nav><section className="settings-panel"><h2>Device</h2><dl><div><dt>Device name</dt><dd>Plant room actuator</dd></div><div><dt>Model</dt><dd>Waveshare ESP32-S3 Relay 6CH</dd></div><div><dt>Firmware</dt><dd>1.00</dd></div><div><dt>Configuration generation</dt><dd>12</dd></div></dl><button className="primary-button" type="button" disabled>Edit settings</button><p className="control-reason">Configuration writes require the firmware configuration API.</p></section></div></div>
}

function MaintenancePage() {
  return <div className="route-content"><header className="route-header"><div><p className="eyebrow">Controlled recovery</p><h1>Maintenance</h1></div></header><section className="maintenance-list"><div><div><h2>Restart device</h2><p>Relay startup behavior follows the configured restore policy.</p></div><button className="secondary-button" disabled>Restart</button></div><div><div><h2>Factory reset</h2><p>Requires the physical BOOT-button gesture on this hardware.</p></div><StatusBadge label="Physical presence required" tone="warning" /></div><div><div><h2>Firmware update</h2><p>Signed browser updates are not available in this firmware build.</p></div><StatusBadge label="Unavailable" /></div></section></div>
}

function App() {
  useLiveEvents()
  const capabilities = useQuery({ queryKey: ['capabilities'], queryFn: api.capabilities, retry: 1 })
  const device = useQuery({ queryKey: ['device'], queryFn: api.device, retry: 1 })
  const network = useQuery({ queryKey: ['network'], queryFn: api.network, refetchInterval: 30_000, retry: 1 })
  if (capabilities.isLoading || device.isLoading) return <main className="boot-state">Connecting to switch actuator...</main>
  if (!capabilities.data || !device.data) return <main className="boot-state boot-state--error"><AlertTriangle size={28} />Device API is unavailable. Relay controls are disabled.</main>

  return <div className="app-shell">
    <a className="skip-link" href="#main-content">Skip to content</a>
    <aside className="side-nav"><div className="brand-mark"><span><Zap size={20} fill="currentColor" /></span><strong>Switch<br />Actuator</strong></div><nav aria-label="Primary navigation">{navigation.map(({ to, label, icon: Icon, end }) => <NavLink key={to} to={to} end={end}><Icon size={20} /><span>{label}</span></NavLink>)}</nav><button className="collapse-control" type="button" aria-label="Collapse navigation"><PanelLeftClose size={19} /><span>Collapse</span></button></aside>
    <div className="app-frame">{previewMode && <div className="preview-banner"><AlertTriangle size={15} />Development preview data</div>}<header className="top-bar"><div className="device-identity"><strong>{device.data.name}</strong><span>{device.data.model} / ...{device.data.serialSuffix}</span></div><div className="top-status"><StatusBadge label={device.data.lifecycle} tone={device.data.lifecycle === 'operational' ? 'success' : 'warning'} /><StatusBadge label={network.data?.state === 'online' ? 'Live' : 'Offline'} tone={network.data?.state === 'online' ? 'success' : 'danger'} />{network.data?.ipv4Address && <span className="ip-address">{network.data.ipv4Address}</span>}</div><div className="account"><button className="icon-button" type="button" aria-label="Account menu" title="Account"><CircleUserRound size={21} /></button><button className="icon-button" type="button" aria-label="Sign out" title="Sign out"><LogOut size={19} /></button></div></header>
      <main id="main-content"><Routes><Route path="/" element={<RelayPage />} /><Route path="/protocols" element={<ProtocolsPage />} /><Route path="/diagnostics" element={<DiagnosticsPage />} /><Route path="/settings" element={<SettingsPage />} /><Route path="/maintenance" element={<MaintenancePage />} /><Route path="*" element={<Navigate to="/" replace />} /></Routes></main>
      <nav className="bottom-nav" aria-label="Mobile navigation">{navigation.slice(0, 4).map(({ to, label, icon: Icon, end }) => <NavLink key={to} to={to} end={end}><Icon size={20} /><span>{label}</span></NavLink>)}</nav>
    </div>
  </div>
}

export default App
