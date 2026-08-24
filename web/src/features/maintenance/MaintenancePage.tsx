import { useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { CloudDownload, FileUp, Power, RefreshCw } from 'lucide-react'
import { api } from '../../api/client'
import { StatusBadge } from '../../components/StatusBadge'

export function MaintenancePage() {
  const queryClient = useQueryClient()
  const capabilities = useQuery({ queryKey: ['capabilities'], queryFn: api.capabilities })
  const firmwareUpdate = capabilities.data?.features.firmwareUpdate === true
  const status = useQuery({ queryKey: ['ota-status'], queryFn: api.otaStatus, enabled: firmwareUpdate, refetchInterval: (query) => ['downloading', 'verifying', 'installing'].includes(query.state.data?.state ?? '') ? 1000 : 10_000 })
  const [releaseUrl, setReleaseUrl] = useState('https://github.com/owner/project/releases/latest')
  const [firmware, setFirmware] = useState<File | null>(null)
  const updateStatus = (data: Awaited<ReturnType<typeof api.otaStatus>>) => queryClient.setQueryData(['ota-status'], data)
  const check = useMutation({ mutationFn: api.checkForUpdate, onSuccess: updateStatus })
  const remote = useMutation({ mutationFn: () => api.installRemoteUpdate(releaseUrl), onSuccess: updateStatus })
  const upload = useMutation({ mutationFn: () => {
    if (!firmware || !firmware.name.endsWith('.bin')) throw new Error('Select a .bin firmware image.')
    return api.uploadFirmware(firmware)
  }, onSuccess: updateStatus })
  const restart = useMutation({ mutationFn: api.restart })
  const update = status.data
  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Signed, rollback-capable updates</p><h1>Maintenance</h1></div>{update && <StatusBadge label={update.state} tone={update.state === 'failed' ? 'danger' : update.state === 'idle' ? 'neutral' : 'info'} />}</header>
    {firmwareUpdate && <section className="maintenance-summary"><div><span>Installed version</span><strong>{update?.currentVersion ?? 'Loading...'}</strong></div><div><span>Build environment</span><strong>{update?.buildEnvironment ?? 'Loading...'}</strong></div><div><span>Available version</span><strong>{update?.availableVersion ?? 'Not checked'}</strong></div></section>}
    {update && update.progressPercent > 0 && <div className="update-progress"><div><span>Update progress</span><strong>{update.progressPercent}%</strong></div><progress max="100" value={update.progressPercent}>{update.progressPercent}%</progress></div>}
    {firmwareUpdate ? <section className="update-workflows">
      <form onSubmit={(event) => { event.preventDefault(); remote.mutate() }}><div className="workflow-icon"><CloudDownload size={22} /></div><div><h2>Remote release</h2><p>Fetch release metadata over HTTPS and select the signed binary matching this build environment.</p></div><label><span>Approved release URL</span><input type="url" required value={releaseUrl} onChange={(event) => setReleaseUrl(event.target.value)} /></label><div className="form-actions"><button type="button" className="secondary-button button-with-icon" onClick={() => check.mutate()} disabled={check.isPending}><RefreshCw size={16} />Check</button><button className="primary-button" disabled={remote.isPending || !update?.availableVersion}>Download and verify</button></div></form>
      <form onSubmit={(event) => { event.preventDefault(); upload.mutate() }}><div className="workflow-icon"><FileUp size={22} /></div><div><h2>Browser upload</h2><p>Stream a signed ESP32 application image to the inactive OTA slot. Relay outputs are not changed during transfer.</p></div><label className="file-control"><span>Firmware image</span><input type="file" accept=".bin,application/octet-stream" required onChange={(event) => setFirmware(event.target.files?.[0] ?? null)} /><small>{firmware ? `${firmware.name} / ${Math.ceil(firmware.size / 1024)} KiB` : 'ESP32 .bin file'}</small></label><div className="form-actions"><button className="primary-button button-with-icon" disabled={upload.isPending || !firmware}><FileUp size={16} />Upload and verify</button></div></form>
    </section> : <div className="notice-band"><div><strong>Firmware update unavailable</strong><span>This firmware does not advertise a complete signed OTA service.</span></div></div>}
    {(remote.isError || upload.isError || update?.error) && <div className="notice-band"><div><strong>Firmware update was rejected</strong><span>{update?.error ?? (remote.error || upload.error)?.message}</span></div></div>}
    <section className="maintenance-list">{capabilities.data?.features.remoteRestart && <div><div><h2>Restart device</h2><p>Restart services through the controlled lifecycle executor.</p></div><button className="secondary-button button-with-icon" disabled={restart.isPending} onClick={() => restart.mutate()}><Power size={16} />{restart.isPending ? 'Restarting...' : 'Restart'}</button></div>}<div><div><h2>Factory reset</h2><p>Remote reset is intentionally unavailable. Use the physical BOOT-button gesture.</p></div><StatusBadge label="Physical presence required" tone="warning" /></div></section>
  </div>
}