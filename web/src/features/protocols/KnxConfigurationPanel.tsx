import { useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { AlertTriangle, Save } from 'lucide-react'
import { ApiError, api } from '../../api/client'
import type { KnxConfiguration } from '../../api/types'
import { StatusBadge } from '../../components/StatusBadge'

const individualAddressPattern = '(?:[0-9]|1[0-5])\\.(?:[0-9]|1[0-5])\\.(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])'
const groupAddressPattern = '(?:[0-9]|[12][0-9]|3[01])/[0-7]/(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])'

type Props = {
  canWrite: boolean
  channelLabels: string[]
}

export function KnxConfigurationPanel({ canWrite, channelLabels }: Props) {
  const queryClient = useQueryClient()
  const configuration = useQuery({ queryKey: ['knx-configuration'], queryFn: api.knxConfiguration })
  const [draft, setDraft] = useState<KnxConfiguration | null>(null)
  const save = useMutation({
    mutationFn: (next: KnxConfiguration) => {
      const { generation, ...configurationUpdate } = next
      return api.saveKnxConfiguration({ ...configurationUpdate, expectedGeneration: generation })
    },
    onSuccess: (snapshot) => {
      queryClient.setQueryData(['knx-configuration'], snapshot)
      void queryClient.invalidateQueries({ queryKey: ['diagnostics'] })
      void queryClient.invalidateQueries({ queryKey: ['device'] })
      void queryClient.invalidateQueries({ queryKey: ['modbus-configuration'] })
      setDraft(null)
    },
  })
  const current = configuration.data
  const form = draft ?? current
  const changed = Boolean(current && form && JSON.stringify(current) !== JSON.stringify(form))
  const updateChannel = (index: number, replacement: KnxConfiguration['channels'][number]) => {
    if (!form) return
    const channels = form.channels.map((channel, channelIndex) => channelIndex === index ? replacement : channel)
    setDraft({ ...form, channels })
  }
  const errorLabel = save.error instanceof ApiError && save.error.status === 409
    ? 'Configuration changed elsewhere. Reload the page and try again.'
    : save.error instanceof ApiError && save.error.status === 422
      ? 'Check address formats, required heartbeat binding, and duplicate command addresses.'
      : 'The device could not save the KNX configuration.'

  return <section className="protocol-configuration" aria-labelledby="knx-configuration-title">
    <div className="section-heading"><div><h2 id="knx-configuration-title">KNX/IP</h2><p>Commission the routing endpoint, device objects, and channel group-address bindings.</p></div>{current && <span className="generation-label">Generation {current.generation}</span>}</div>
    {configuration.isLoading && <div className="page-state protocol-loading">Loading KNX configuration...</div>}
    {configuration.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>Configuration unavailable</strong><span>KNX settings could not be loaded from the controller.</span></div></div>}
    {form && <form className="knx-form" onSubmit={(event) => { event.preventDefault(); save.mutate(form) }}>
      <div className="knx-enable-row"><label className="check-control"><input type="checkbox" disabled={!canWrite || save.isPending} checked={form.enabled} onChange={(event) => setDraft({ ...form, enabled: event.target.checked })} /><span><strong>Enable KNX/IP routing</strong><small>Requires a commissioned individual address and restarts the controller.</small></span></label></div>

      <fieldset className="knx-fieldset"><legend>Device and transport</legend><div className="knx-grid">
        <label><span>Individual address</span><input required={form.enabled} pattern={individualAddressPattern} placeholder="1.1.20" disabled={!canWrite || save.isPending} value={form.individualAddress} onChange={(event) => setDraft({ ...form, individualAddress: event.target.value })} /><small>Area.line.device</small></label>
        <label><span>Startup delay</span><div className="unit-input"><input type="number" min={0} max={60_000} required disabled={!canWrite || save.isPending} value={form.startupTransmitDelayMs} onChange={(event) => setDraft({ ...form, startupTransmitDelayMs: Number(event.target.value) })} /><span>ms</span></div></label>
        <label><span>Telegram interval</span><div className="unit-input"><input type="number" min={20} max={1000} required disabled={!canWrite || save.isPending} value={form.minimumTelegramIntervalMs} onChange={(event) => setDraft({ ...form, minimumTelegramIntervalMs: Number(event.target.value) })} /><span>ms</span></div></label>
        <label><span>Cyclic status interval</span><div className="unit-input"><input type="number" min={0} max={86_400_000} required disabled={!canWrite || save.isPending} value={form.cyclicStatusIntervalMs} onChange={(event) => setDraft({ ...form, cyclicStatusIntervalMs: Number(event.target.value) })} /><span>ms</span></div><small>0 disables; otherwise at least 10000.</small></label>
        <label><span>Heartbeat interval</span><div className="unit-input"><input type="number" min={0} max={86_400_000} required disabled={!canWrite || save.isPending} value={form.heartbeatIntervalMs} onChange={(event) => setDraft({ ...form, heartbeatIntervalMs: Number(event.target.value) })} /><span>ms</span></div><small>0 disables; otherwise at least 10000.</small></label>
        <label className="check-control compact"><input type="checkbox" disabled={!canWrite || save.isPending} checked={form.readSwitchObject} onChange={(event) => setDraft({ ...form, readSwitchObject: event.target.checked })} /><span><strong>Answer switch reads</strong><small>Respond with the applied relay state.</small></span></label>
      </div></fieldset>

      <fieldset className="knx-fieldset"><legend>Device-wide group objects</legend><div className="knx-grid object-grid">
        {([['heartbeatGroupAddress', 'Heartbeat'], ['centralSwitchGroupAddress', 'Central switch'], ['centralOffGroupAddress', 'Central off'], ['deviceFaultGroupAddress', 'Device fault']] as const).map(([key, label]) => <label key={key}><span>{label}</span><input pattern={groupAddressPattern} placeholder="0/0/1" disabled={!canWrite || save.isPending} value={form[key]} onChange={(event) => setDraft({ ...form, [key]: event.target.value })} /><small>Blank means unassigned.</small></label>)}
      </div></fieldset>

      <fieldset className="knx-fieldset channel-bindings"><legend>Channel bindings</legend><div className="knx-channel-list">
        {form.channels.map((channel, index) => <details key={channel.index} open={index === 0}><summary><span>{channelLabels[index] ?? `Channel ${index + 1}`}</span><span>{channel.switchGroupAddress || 'Unassigned'}</span></summary><div className="knx-channel-content">
          <div className="knx-grid channel-addresses">{([['switchGroupAddress', 'Switch command'], ['statusGroupAddress', 'Applied status'], ['faultGroupAddress', 'Channel fault']] as const).map(([key, label]) => <label key={key}><span>{label}</span><input pattern={groupAddressPattern} placeholder="1/0/1" disabled={!canWrite || save.isPending} value={channel[key]} onChange={(event) => updateChannel(index, { ...channel, [key]: event.target.value })} /></label>)}</div>
          <div className="channel-options">
            {([['commandPolarityInverted', 'Invert command polarity'], ['statusPolarityInverted', 'Invert status polarity'], ['sendStatusAfterStartup', 'Send status after startup'], ['participatesInCentralSwitch', 'Central switch participant'], ['participatesInCentralOff', 'Central off participant']] as const).map(([key, label]) => <label className="check-control compact" key={key}><input type="checkbox" disabled={!canWrite || save.isPending} checked={channel[key]} onChange={(event) => updateChannel(index, { ...channel, [key]: event.target.checked })} /><span><strong>{label}</strong></span></label>)}
          </div>
        </div></details>)}
      </div></fieldset>

      <div className="form-actions"><button className="primary-button button-with-icon" disabled={!canWrite || !changed || save.isPending}><Save size={16} />{save.isPending ? 'Saving...' : 'Save KNX and restart'}</button>{!canWrite && <StatusBadge label="Read only" tone="neutral" />}{save.isSuccess && <StatusBadge label="Saved" tone="success" />}</div>
      {save.isError && <div className="notice-band" role="alert"><AlertTriangle size={18} /><div><strong>KNX settings were not applied</strong><span>{errorLabel}</span></div></div>}
    </form>}
  </section>
}