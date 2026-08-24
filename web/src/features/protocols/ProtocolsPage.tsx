import { useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { AlertTriangle, Gauge, Radio, Save } from 'lucide-react'
import { ApiError, api } from '../../api/client'
import type { ModbusConfiguration } from '../../api/types'
import { StatusBadge } from '../../components/StatusBadge'
import { KnxConfigurationPanel } from './KnxConfigurationPanel'
import './ProtocolsPage.css'

const baudRates = [9600, 19200, 38400, 57600, 115200] as const

function sameModbusConfiguration(left: ModbusConfiguration, right: ModbusConfiguration) {
  return left.unitId === right.unitId && left.baudRate === right.baudRate && left.parity === right.parity &&
    left.stopBits === right.stopBits
}

export function ProtocolsPage() {
  const queryClient = useQueryClient()
  const capabilities = useQuery({ queryKey: ['capabilities'], queryFn: api.capabilities })
  const diagnostics = useQuery({ queryKey: ['diagnostics'], queryFn: api.diagnostics })
  const modbusSupported = capabilities.data?.features.modbus === true
  const knxSupported = capabilities.data?.features.knx === true
  const canWrite = capabilities.data?.permissions.includes('configuration:write') === true
  const configuration = useQuery({
    queryKey: ['modbus-configuration'],
    queryFn: api.modbusConfiguration,
    enabled: modbusSupported,
  })
  const [draft, setDraft] = useState<ModbusConfiguration | null>(null)
  const save = useMutation({
    mutationFn: (next: ModbusConfiguration) => api.saveModbusConfiguration({
      unitId: next.unitId,
      baudRate: next.baudRate,
      parity: next.parity,
      stopBits: next.stopBits,
      expectedGeneration: next.generation,
    }),
    onSuccess: (snapshot) => {
      queryClient.setQueryData(['modbus-configuration'], snapshot)
      void queryClient.invalidateQueries({ queryKey: ['diagnostics'] })
      void queryClient.invalidateQueries({ queryKey: ['device'] })
      setDraft(null)
    },
  })
  const switchRole = useMutation({
    mutationFn: (role: 'server' | 'client') => api.setModbusRole(role),
    onSuccess: (snapshot) => {
      queryClient.setQueryData(['modbus-configuration'], snapshot)
      setDraft((pending) => pending ? { ...pending, role: snapshot.role } : null)
    },
  })

  if (!capabilities.data || !diagnostics.data) return <div className="page-state">Loading protocol state...</div>
  const { modbus, knx } = diagnostics.data.protocols
  const current = configuration.data
  const form = draft ?? current
  const changed = Boolean(current && form && !sameModbusConfiguration(current, form))
  const errorLabel = save.error instanceof ApiError && save.error.status === 409
    ? 'Configuration changed elsewhere. Reload the page and try again.'
    : 'The device could not save the Modbus configuration.'

  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Fieldbus health and configuration</p><h1>Protocols</h1></div></header>
    <section className="data-section"><h2>Communication status</h2><div className="table-wrap"><table><caption className="sr-only">Protocol communication status</caption><thead><tr><th>Protocol</th><th>Status</th><th>Address</th><th>Valid traffic</th><th>Errors</th></tr></thead><tbody>
      <tr><td><span className="table-title"><Radio size={18} />Modbus RTU</span></td><td><StatusBadge label={modbus.available ? 'Available' : 'Unavailable'} tone={modbus.available ? 'success' : 'danger'} /></td><td>Unit {modbus.unitId} / {modbus.baudRate.toLocaleString()} baud</td><td>{modbus.validRequests.toLocaleString()}</td><td>{modbus.errors}</td></tr>
      <tr><td><span className="table-title"><Gauge size={18} />KNX/IP</span></td><td><StatusBadge label={knx.busOnline ? 'Bus online' : knx.enabled ? 'Degraded' : 'Disabled'} tone={knx.busOnline ? 'success' : 'warning'} /></td><td>{knx.individualAddress ?? 'Not configured'}</td><td>{knx.validTelegrams.toLocaleString()}</td><td>{knx.errors}</td></tr>
    </tbody></table></div></section>

    {modbusSupported && <section className="protocol-configuration" aria-labelledby="modbus-configuration-title">
      <div className="section-heading"><div><h2 id="modbus-configuration-title">Modbus RTU</h2><p>Switch roles immediately, or update serial framing and restart the controller.</p></div>{current && <span className="generation-label">Generation {current.generation}</span>}</div>
      {configuration.isLoading && <div className="page-state protocol-loading">Loading Modbus configuration...</div>}
      {configuration.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>Configuration unavailable</strong><span>The live protocol counters remain available, but settings could not be loaded.</span></div></div>}
      {form && <form className="modbus-form" onSubmit={(event) => { event.preventDefault(); save.mutate(form) }}>
        <fieldset className="role-control" disabled={!canWrite || switchRole.isPending}><legend>Operating role</legend><div className="segmented-control"><button type="button" aria-pressed={form.role === 'server'} onClick={() => switchRole.mutate('server')}>Server</button><button type="button" aria-pressed={form.role === 'client'} onClick={() => switchRole.mutate('client')}>Client</button></div><small>Role changes take effect immediately without restarting.</small></fieldset>
        <label><span>Unit ID</span><input type="number" inputMode="numeric" min={1} max={247} required disabled={!canWrite || save.isPending} value={form.unitId} onChange={(event) => setDraft({ ...form, unitId: Number(event.target.value) })} /><small>Unique address from 1 through 247.</small></label>
        <label><span>Baud rate</span><select disabled={!canWrite || save.isPending} value={form.baudRate} onChange={(event) => setDraft({ ...form, baudRate: Number(event.target.value) as ModbusConfiguration['baudRate'] })}>{baudRates.map((baudRate) => <option key={baudRate} value={baudRate}>{baudRate.toLocaleString()} baud</option>)}</select></label>
        <label><span>Parity</span><select disabled={!canWrite || save.isPending} value={form.parity} onChange={(event) => setDraft({ ...form, parity: event.target.value as ModbusConfiguration['parity'] })}><option value="none">None</option><option value="even">Even</option><option value="odd">Odd</option></select></label>
        <label><span>Stop bits</span><select disabled={!canWrite || save.isPending} value={form.stopBits} onChange={(event) => setDraft({ ...form, stopBits: Number(event.target.value) as ModbusConfiguration['stopBits'] })}><option value={1}>1 stop bit</option><option value={2}>2 stop bits</option></select></label>
        <div className="serial-summary"><span>Data bits</span><strong>8</strong><span>Mode</span><strong>RTU {form.role}</strong></div>
        <div className="form-actions"><button className="primary-button button-with-icon" disabled={!canWrite || !changed || save.isPending}><Save size={16} />{save.isPending ? 'Saving...' : 'Save and restart'}</button>{!canWrite && <StatusBadge label="Read only" tone="neutral" />}{save.isSuccess && <StatusBadge label="Saved" tone="success" />}</div>
        {save.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>Modbus settings were not applied</strong><span>{errorLabel}</span></div></div>}
        {switchRole.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>Modbus role was not changed</strong><span>The device could not switch its active Modbus role.</span></div></div>}
      </form>}
    </section>}

    {knxSupported && <KnxConfigurationPanel canWrite={canWrite} channelLabels={capabilities.data.channels.map((channel) => channel.physicalLabel)} />}

    {!modbusSupported && <div className="notice-band"><AlertTriangle size={18} /><div><strong>Modbus RTU is unavailable</strong><span>This firmware does not advertise Modbus support.</span></div></div>}
    {!knxSupported && <div className="notice-band"><AlertTriangle size={18} /><div><strong>KNX/IP is unavailable</strong><span>This firmware does not advertise KNX support.</span></div></div>}
    {!knx.busOnline && <div className="notice-band"><AlertTriangle size={18} /><div><strong>KNX bus communication is degraded</strong><span>Relay operation remains available through other configured sources.</span></div></div>}
  </div>
}
