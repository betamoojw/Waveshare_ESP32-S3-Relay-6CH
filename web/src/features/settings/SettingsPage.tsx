import { useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { ArrowDown, ArrowUp, Eye, EyeOff, LockKeyhole, PlugZap, Plus, RefreshCw, Router, Save, ShieldCheck, Trash2, Wifi } from 'lucide-react'
import { ApiError, api } from '../../api/client'
import type { WifiProfile, WifiRecoveryAp } from '../../api/types'
import { StatusBadge } from '../../components/StatusBadge'

type Section = 'network' | 'users'

const emptyProfile: WifiProfile = {
  index: 0, enabled: false, ssid: '', hasPassphrase: false,
  ipv4: { mode: 'dhcp', address: '', subnetMask: '', gateway: '', dns: '' },
}

export function SettingsPage() {
  const [section, setSection] = useState<Section>('network')
  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Generation-protected configuration</p><h1>Settings</h1></div><StatusBadge label="Administrator" tone="info" /></header>
    <div className="settings-layout">
      <nav aria-label="Settings sections">
        <button className={section === 'network' ? 'is-active' : ''} onClick={() => setSection('network')}><Wifi size={17} />Network</button>
        <button className={section === 'users' ? 'is-active' : ''} onClick={() => setSection('users')}><ShieldCheck size={17} />Users</button>
      </nav>
      {section === 'network' ? <NetworkSettings /> : <UserSettings />}
    </div>
  </div>
}

function NetworkSettings() {
  const queryClient = useQueryClient()
  const management = useQuery({ queryKey: ['wifi-management'], queryFn: api.wifiManagement, refetchInterval: (query) => query.state.data?.scan.state === 'scanning' ? 1000 : false })
  const [selectedIndex, setSelectedIndex] = useState(0)
  const [profileDraft, setProfileDraft] = useState<WifiProfile | null>(null)
	const [recoveryDraft, setRecoveryDraft] = useState<WifiRecoveryAp | null>(null)
  const [passphrase, setPassphrase] = useState('')
	const [clearPassphrase, setClearPassphrase] = useState(false)
  const [showPassphrase, setShowPassphrase] = useState(false)

  const scan = useMutation({ mutationFn: api.scanWifi, onSuccess: (data) => queryClient.setQueryData(['wifi-management'], data) })
	const acceptSnapshot = (data: Awaited<ReturnType<typeof api.wifiManagement>>) => queryClient.setQueryData(['wifi-management'], data)
	const resetProfileDraft = () => { setProfileDraft(null); setPassphrase(''); setClearPassphrase(false) }
	const save = useMutation({ mutationFn: (profile: WifiProfile) => api.saveWifiProfile({
		index: profile.index, enabled: profile.enabled, ssid: profile.ssid, ipv4: profile.ipv4,
		expectedGeneration: management.data!.generation, ...(passphrase ? { passphrase } : {}),
		...(clearPassphrase ? { clearPassphrase: true } : {}),
	}), onSuccess: (data) => { acceptSnapshot(data); resetProfileDraft() } })
	const remove = useMutation({ mutationFn: (index: number) => api.deleteWifiProfile(index, management.data!.generation), onSuccess: (data) => { acceptSnapshot(data); setSelectedIndex(0); resetProfileDraft() } })
  const move = useMutation({ mutationFn: ({ index, toIndex }: { index: number; toIndex: number }) => api.moveWifiProfile(index, toIndex, management.data!.generation), onSuccess: (data, variables) => { acceptSnapshot(data); setSelectedIndex(variables.toIndex); resetProfileDraft() } })
	const connect = useMutation({ mutationFn: api.connectWifiProfile, onSuccess: acceptSnapshot })
	const saveRecovery = useMutation({ mutationFn: (recoveryAp: WifiRecoveryAp) => api.saveRecoveryAp({ enabled: recoveryAp.enabled, ssidPrefix: recoveryAp.ssidPrefix, channel: recoveryAp.channel, timeoutMs: recoveryAp.timeoutMs, remainActiveWhileOffline: recoveryAp.remainActiveWhileOffline }, management.data!.generation), onSuccess: (data) => { acceptSnapshot(data); setRecoveryDraft(null) } })
  if (!management.data) return <section className="settings-panel">Loading network configuration...</section>
  const { scan: scanState } = management.data
  const profile = profileDraft?.index === selectedIndex ? profileDraft : management.data.profiles.find((candidate) => candidate.index === selectedIndex) ?? { ...emptyProfile, index: selectedIndex }
	const recoveryAp = recoveryDraft ?? management.data.recoveryAp
  const updateProfile = (replacement: WifiProfile) => setProfileDraft(replacement)
	const mutationError = save.error ?? remove.error ?? move.error ?? connect.error ?? saveRecovery.error
	const mutationErrorLabel = mutationError instanceof ApiError && mutationError.status === 409 ? 'Settings changed elsewhere. Reload and retry.' : 'Operation failed'

  return <section className="settings-panel settings-panel--wide">
    <div className="section-heading"><div><h2>Wi-Fi profiles</h2><p>Profiles are attempted in slot order. Stored passphrases are never returned by the API.</p></div><button className="secondary-button button-with-icon" onClick={() => scan.mutate()} disabled={scan.isPending || scanState.state === 'scanning'}><RefreshCw size={16} className={scanState.state === 'scanning' ? 'spin' : ''} />Scan</button></div>
    <div className="network-overview"><StatusBadge label={management.data.recoveryAp.active ? 'Recovery AP active' : 'Infrastructure mode'} tone={management.data.recoveryAp.active ? 'warning' : 'success'} /><span>{scanState.results.length} networks found</span><span>Generation {management.data.generation}</span></div>
    {scanState.results.length > 0 && <div className="network-results" role="list" aria-label="Available Wi-Fi networks">{scanState.results.map((network) => <button type="button" role="listitem" key={`${network.ssid}-${network.channel}`} onClick={() => updateProfile({ ...profile, enabled: true, ssid: network.ssid })}><Wifi size={16} /><strong>{network.ssid || 'Hidden network'}</strong><span>{network.rssi} dBm / ch {network.channel}</span>{network.secured && <LockKeyhole size={14} />}</button>)}</div>}
		<div className="profile-tabs" role="tablist" aria-label="Wi-Fi profile slots">{management.data.profiles.map((candidate) => <button role="tab" aria-selected={candidate.index === selectedIndex} className={candidate.index === selectedIndex ? 'is-active' : ''} key={candidate.index} onClick={() => { setSelectedIndex(candidate.index); resetProfileDraft() }}>Profile {candidate.index + 1}{management.data.activeProfileIndex === candidate.index && <StatusBadge label="Active" tone="success" />}<span>{candidate.ssid || 'Unused'}</span></button>)}</div>
    <form className="settings-form" onSubmit={(event) => { event.preventDefault(); save.mutate(profile) }}>
      <label className="toggle-row"><input type="checkbox" checked={profile.enabled} onChange={(event) => updateProfile({ ...profile, enabled: event.target.checked })} /><span>Enable this profile</span></label>
      <label><span>SSID</span><input maxLength={32} required={profile.enabled} value={profile.ssid} onChange={(event) => updateProfile({ ...profile, ssid: event.target.value })} /></label>
			<label><span>Passphrase {profile.hasPassphrase && <small>Leave blank to keep stored value</small>}</span><div className="input-action"><input disabled={clearPassphrase} type={showPassphrase ? 'text' : 'password'} minLength={8} maxLength={63} value={passphrase} onChange={(event) => setPassphrase(event.target.value)} /><button type="button" aria-label={showPassphrase ? 'Hide passphrase' : 'Show passphrase'} onClick={() => setShowPassphrase(!showPassphrase)}>{showPassphrase ? <EyeOff size={17} /> : <Eye size={17} />}</button></div>{profile.hasPassphrase && <label className="inline-check"><input type="checkbox" checked={clearPassphrase} onChange={(event) => { setClearPassphrase(event.target.checked); setPassphrase('') }} />Clear stored passphrase</label>}</label>
      <label><span>Address assignment</span><select value={profile.ipv4.mode} onChange={(event) => updateProfile({ ...profile, ipv4: { ...profile.ipv4, mode: event.target.value as 'dhcp' | 'static' } })}><option value="dhcp">DHCP</option><option value="static">Static IPv4</option></select></label>
      {profile.ipv4.mode === 'static' && <div className="ipv4-grid">{(['address', 'subnetMask', 'gateway', 'dns'] as const).map((field) => <label key={field}><span>{{ address: 'Address', subnetMask: 'Subnet mask', gateway: 'Gateway', dns: 'DNS server' }[field]}</span><input inputMode="decimal" required value={profile.ipv4[field]} onChange={(event) => updateProfile({ ...profile, ipv4: { ...profile.ipv4, [field]: event.target.value } })} /></label>)}</div>}
			<div className="profile-actions"><button type="button" className="icon-button" aria-label="Move profile up" title="Move profile up" disabled={selectedIndex === 0 || move.isPending} onClick={() => move.mutate({ index: selectedIndex, toIndex: selectedIndex - 1 })}><ArrowUp size={17} /></button><button type="button" className="icon-button" aria-label="Move profile down" title="Move profile down" disabled={selectedIndex === 2 || move.isPending} onClick={() => move.mutate({ index: selectedIndex, toIndex: selectedIndex + 1 })}><ArrowDown size={17} /></button><button type="button" className="secondary-button button-with-icon" disabled={!profile.enabled || connect.isPending} onClick={() => connect.mutate(selectedIndex)}><PlugZap size={16} />Connect now</button><button type="button" className="danger-button button-with-icon" disabled={!profile.ssid || remove.isPending} onClick={() => window.confirm(`Delete profile ${selectedIndex + 1}?`) && remove.mutate(selectedIndex)}><Trash2 size={16} />Delete</button></div>
			<div className="form-actions"><button className="primary-button button-with-icon" disabled={save.isPending}><Save size={16} />{save.isPending ? 'Saving...' : 'Save profile'}</button>{save.isSuccess && <StatusBadge label="Saved" tone="success" />}{mutationError && <StatusBadge label={mutationErrorLabel} tone="danger" />}</div>
    </form>
		<form className="settings-form recovery-ap-form" onSubmit={(event) => { event.preventDefault(); saveRecovery.mutate(recoveryAp) }}><div className="section-heading form-heading"><div><h3><Router size={17} />Recovery access point</h3><p>Starts for local provisioning when infrastructure profiles cannot connect.</p></div><StatusBadge label={recoveryAp.active ? 'Active' : 'Standby'} tone={recoveryAp.active ? 'warning' : 'neutral'} /></div><label className="toggle-row"><input type="checkbox" checked={recoveryAp.enabled} onChange={(event) => setRecoveryDraft({ ...recoveryAp, enabled: event.target.checked })} /><span>Enable fallback access point</span></label><label><span>SSID prefix</span><input required maxLength={23} value={recoveryAp.ssidPrefix} onChange={(event) => setRecoveryDraft({ ...recoveryAp, ssidPrefix: event.target.value })} /></label><label><span>Channel</span><input type="number" min={1} max={13} required value={recoveryAp.channel} onChange={(event) => setRecoveryDraft({ ...recoveryAp, channel: Number(event.target.value) })} /></label><label><span>Automatic stop (milliseconds; 0 disables)</span><input type="number" min={0} step={1000} value={recoveryAp.timeoutMs} onChange={(event) => setRecoveryDraft({ ...recoveryAp, timeoutMs: Number(event.target.value) })} /></label><label className="toggle-row"><input type="checkbox" checked={recoveryAp.remainActiveWhileOffline} onChange={(event) => setRecoveryDraft({ ...recoveryAp, remainActiveWhileOffline: event.target.checked })} /><span>Keep active while infrastructure is offline</span></label><div className="form-actions"><button className="primary-button button-with-icon" disabled={saveRecovery.isPending}><Save size={16} />Save recovery AP</button></div></form>
  </section>
}

function UserSettings() {
  const queryClient = useQueryClient()
  const users = useQuery({ queryKey: ['users'], queryFn: api.users })
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [role, setRole] = useState<'administrator' | 'guest'>('guest')
  const save = useMutation({ mutationFn: () => api.saveUser({ username, password, role, enabled: true }), onSuccess: (data) => { queryClient.setQueryData(['users'], data); setUsername(''); setPassword('') } })
  return <section className="settings-panel settings-panel--wide">
    <div className="section-heading"><div><h2>User profiles</h2><p>Administrators can change configuration and firmware. Guests have read-only dashboard access.</p></div></div>
    <div className="user-list">{users.data?.map((user) => <div key={user.id}><span className="user-avatar">{user.username.slice(0, 1).toUpperCase()}</span><div><strong>{user.username}</strong><span>{user.role}</span></div><StatusBadge label={user.enabled ? 'Enabled' : 'Disabled'} tone={user.enabled ? 'success' : 'neutral'} /></div>)}</div>
    <form className="settings-form user-form" onSubmit={(event) => { event.preventDefault(); save.mutate() }}><h3>Add user</h3><label><span>Username</span><input required minLength={3} maxLength={24} autoComplete="off" value={username} onChange={(event) => setUsername(event.target.value)} /></label><label><span>Temporary password</span><input required minLength={12} maxLength={64} type="password" autoComplete="new-password" value={password} onChange={(event) => setPassword(event.target.value)} /></label><label><span>Authorization level</span><select value={role} onChange={(event) => setRole(event.target.value as 'administrator' | 'guest')}><option value="guest">Guest / read only</option><option value="administrator">Administrator</option></select></label><div className="form-actions"><button className="primary-button button-with-icon" disabled={save.isPending}><Plus size={16} />Add user</button>{save.isError && <StatusBadge label="Unable to add user" tone="danger" />}</div></form>
  </section>
}