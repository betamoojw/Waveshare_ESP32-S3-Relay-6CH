import { useState } from 'react'
import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { AlertCircle, Clock3, Filter, LockKeyhole, MoreVertical, Power } from 'lucide-react'
import { api } from '../../api/client'
import type { Relay, RelayState } from '../../api/types'
import { StatusBadge } from '../../components/StatusBadge'

type FilterValue = 'all' | 'on' | 'off' | 'attention'

function RelayItem({ relay }: { relay: Relay }) {
  const queryClient = useQueryClient()
  const mutation = useMutation({
    mutationFn: (state: RelayState) => api.commandRelay(relay.id, state, relay.transitionSequence),
    onSuccess: async () => queryClient.invalidateQueries({ queryKey: ['relays'] }),
  })
  const unavailable = !relay.enabled || Boolean(relay.fault)
  const stateLabel = relay.fault ? 'Fault' : relay.lockedOut ? 'Locked' : relay.appliedState === 'on' ? 'On' : 'Off'
  const tone = relay.fault ? 'danger' : relay.lockedOut ? 'warning' : relay.appliedState === 'on' ? 'success' : 'neutral'

  return (
    <article className={`relay-item relay-item--${tone}`} aria-labelledby={`relay-${relay.id}-name`}>
      <div className="relay-item__head">
        <div><span className="channel-label">{relay.physicalLabel}</span><h2 id={`relay-${relay.id}-name`}>{relay.name ?? `Channel ${relay.id + 1}`}</h2></div>
        <button className="icon-button" type="button" aria-label={`Open ${relay.physicalLabel} details`} title="Channel details"><MoreVertical size={19} /></button>
      </div>
      <div className="relay-state-row">
        <div className={`relay-state relay-state--${tone}`}><Power size={25} aria-hidden="true" /><strong>{stateLabel}</strong></div>
        <span className="verification">GPIO applied</span>
      </div>
      <div className="relay-meta"><span><Clock3 size={14} />{Math.max(1, Math.round(relay.lastTransitionAgeMs / 1000))}s ago</span><span>via {relay.lastSource}</span></div>
      {relay.lockedOut && <div className="inline-notice"><LockKeyhole size={15} />Safety lockout active</div>}
      {relay.fault && <div className="inline-notice inline-notice--danger"><AlertCircle size={15} />{relay.fault}</div>}
      {mutation.isError && <div className="inline-notice inline-notice--danger" role="alert">Command rejected</div>}
      {mutation.isSuccess && <span className="sr-only" role="status">{relay.physicalLabel} command {mutation.data.result}.</span>}
      <div className="segmented-control" aria-label={`${relay.physicalLabel} applied state`} aria-busy={mutation.isPending}>
        {(['off', 'on'] as const).map((state) => <button key={state} type="button" className={relay.appliedState === state ? 'is-active' : ''} disabled={unavailable || mutation.isPending || (relay.lockedOut && state === 'on')} aria-pressed={relay.appliedState === state} onClick={() => mutation.mutate(state)}>{mutation.isPending && mutation.variables === state ? 'Pending' : state === 'on' ? 'On' : 'Off'}</button>)}
      </div>
    </article>
  )
}

export function RelayPage() {
  const [filter, setFilter] = useState<FilterValue>('all')
  const query = useQuery({ queryKey: ['relays'], queryFn: api.relays, refetchInterval: 30_000 })
  const queryClient = useQueryClient()
  if (query.isLoading) return <div className="page-state">Loading relay state...</div>
  if (!query.data) return <div className="page-state page-state--error" role="alert">Relay state is unavailable. Controls remain disabled.</div>

  const needsAttention = (relay: Relay) => relay.lockedOut || Boolean(relay.fault) || !relay.enabled
  const filtered = query.data.relays.filter((relay) => filter === 'all' || (filter === 'attention' ? needsAttention(relay) : relay.appliedState === filter))
  const onCount = query.data.relays.filter((relay) => relay.appliedState === 'on').length

  return <div className="route-content">
    <header className="route-header"><div><p className="eyebrow">Applied output state</p><h1>Relays</h1></div><button className="secondary-button" type="button" onClick={() => queryClient.invalidateQueries({ queryKey: ['relays'] })}>Refresh state</button></header>
    <section className="summary-band" aria-label="Relay summary"><div><strong>{onCount}</strong><span>On</span></div><div><strong>{query.data.relays.length - onCount}</strong><span>Off</span></div><div><strong>{query.data.relays.filter(needsAttention).length}</strong><span>Attention</span></div><div className="summary-sequence"><span>Snapshot</span><strong>#{query.data.snapshotSequence}</strong></div></section>
    <div className="relay-toolbar"><div className="filter-control" aria-label="Filter relays"><Filter size={17} />{(['all', 'on', 'off', 'attention'] as const).map((value) => <button key={value} type="button" className={filter === value ? 'is-active' : ''} onClick={() => setFilter(value)}>{value[0].toUpperCase() + value.slice(1)}</button>)}</div><StatusBadge label="Authoritative state" tone="success" /></div>
    <section className="relay-grid" aria-label="Relay channels">{filtered.map((relay) => <RelayItem key={relay.id} relay={relay} />)}</section>
  </div>
}