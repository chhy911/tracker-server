import { useCallback, useEffect, useRef, useState } from 'react';
import './App.css';

const API_BASE = import.meta.env.VITE_API_URL || '';
const REFRESH_MS = 10000;
const HISTORY_LEN = 30;

async function fetchJson(path) {
  const res = await fetch(`${API_BASE}${path}`);
  const text = await res.text();
  const ct = res.headers.get('content-type') || '';
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  if (!ct.includes('json') && text.trimStart().startsWith('<')) {
    throw new Error('API 返回了 HTML，请通过 http://服务器IP:8888 访问');
  }
  try { return JSON.parse(text); }
  catch { throw new Error(`无效 JSON: ${text.slice(0, 80)}`); }
}

function useNow() {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(id);
  }, []);
  return now;
}

function Sparkline({ data, color = '#3b82f6', height = 48 }) {
  if (!data || data.length < 2) return <svg height={height} />;
  const w = 240, h = height;
  const max = Math.max(...data, 1);
  const pts = data.map((v, i) => {
    const x = (i / (data.length - 1)) * w;
    const y = h - (v / max) * (h - 4) - 2;
    return `${x},${y}`;
  }).join(' ');
  return (
    <svg viewBox={`0 0 ${w} ${h}`} height={h} preserveAspectRatio="none">
      <polyline points={pts} fill="none" stroke={color} strokeWidth="1.5" />
      <polygon
        points={`0,${h} ${pts} ${w},${h}`}
        fill={color}
        opacity="0.1"
      />
    </svg>
  );
}

function PeerModal({ infoHash, onClose }) {
  const [peers, setPeers] = useState(null);
  const [err, setErr] = useState(null);

  useEffect(() => {
    fetchJson(`/api/peers/${infoHash}?limit=100`)
      .then(d => setPeers(d.peers || []))
      .catch(e => setErr(e.message));
  }, [infoHash]);

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal" onClick={e => e.stopPropagation()}>
        <div className="modal-header">
          <h3>Peers — <span className="hash">{infoHash.slice(0, 16)}…</span></h3>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>
        {err && <p style={{ color: 'var(--red)' }}>{err}</p>}
        {!peers && !err && <p className="loading">加载中…</p>}
        {peers && peers.length === 0 && <p className="empty">暂无活跃 Peer</p>}
        {peers && peers.length > 0 && (
          <table>
            <thead>
              <tr><th>IP</th><th>Port</th><th>Peer ID</th></tr>
            </thead>
            <tbody>
              {peers.map((p, i) => (
                <tr key={i}>
                  <td>{p.ip}</td>
                  <td>{p.port}</td>
                  <td className="hash">{p.peer_id.slice(0, 20)}…</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}

export default function App() {
  const [stats, setStats]     = useState(null);
  const [metrics, setMetrics] = useState(null);
  const [health, setHealth]   = useState(null);
  const [torrents, setTorrents] = useState([]);
  const [total, setTotal]     = useState(0);
  const [page, setPage]       = useState(1);
  const [error, setError]     = useState(null);
  const [loading, setLoading] = useState(true);
  const [lastUpdated, setLastUpdated] = useState(null);
  const [selectedHash, setSelectedHash] = useState(null);

  // Rolling history for sparklines
  const history = useRef({ peers: [], seeders: [], leechers: [], rps: [] });

  const now = useNow();
  const PAGE_SIZE = 20;

  const refresh = useCallback(async (p = page) => {
    setLoading(true);
    setError(null);
    try {
      const [s, m, h, t] = await Promise.all([
        fetchJson('/api/stats'),
        fetchJson('/api/metrics'),
        fetchJson('/api/health'),
        fetchJson(`/api/torrents?limit=${PAGE_SIZE}&page=${p}`),
      ]);
      setStats(s);
      setMetrics(m);
      setHealth(h);
      setTorrents(t.torrents || []);
      setTotal(t.total || 0);
      setLastUpdated(new Date());

      // Append to history
      const hist = history.current;
      const push = (arr, val) => {
        arr.push(val ?? 0);
        if (arr.length > HISTORY_LEN) arr.shift();
      };
      push(hist.peers,    s?.peer_count ?? 0);
      push(hist.seeders,  s?.active_seeders ?? 0);
      push(hist.leechers, s?.active_leechers ?? 0);
      push(hist.rps,      m?.requests_per_second ?? 0);
    } catch (e) {
      setError(e.message || '无法连接 API');
    } finally {
      setLoading(false);
    }
  }, [page]);

  useEffect(() => {
    refresh(page);
  }, [page]);   // re-fetch on page change

  useEffect(() => {
    const id = setInterval(() => refresh(page), REFRESH_MS);
    return () => clearInterval(id);
  }, [refresh, page]);

  const totalPages = Math.max(1, Math.ceil(total / PAGE_SIZE));

  const dbOk    = health?.db === 'ok';
  const svcOk   = health?.status === 'ok';

  return (
    <div className="app">
      {/* Header */}
      <header>
        <div className="header-left">
          <h1>BitTorrent Tracker</h1>
          <p className="subtitle">实时监控仪表板</p>
        </div>
        <div className="header-right">
          <span className="clock">{now.toLocaleTimeString('zh-CN', { hour12: false })}</span>
          {lastUpdated && (
            <span className="last-updated">
              更新于 {lastUpdated.toLocaleTimeString('zh-CN', { hour12: false })}
            </span>
          )}
          <button onClick={() => refresh(page)} disabled={loading}>
            {loading ? '刷新中…' : '立即刷新'}
          </button>
        </div>
      </header>

      {/* Status banner */}
      {error ? (
        <div className="banner error">
          <strong>错误：</strong>{error}
          <span className="hint">请确认通过 http://服务器IP:8888 访问</span>
        </div>
      ) : health && (
        <div className={`banner ${svcOk && dbOk ? 'ok' : 'error'}`}>
          服务{svcOk ? '正常' : '异常'} · 数据库{dbOk ? '在线' : '离线'}
        </div>
      )}

      {/* Stat cards row 1 */}
      <section className="grid">
        <Card title="种子数量"  value={stats?.torrent_count ?? '—'} />
        <Card title="活跃 Peer" value={stats?.peer_count ?? '—'} />
        <Card title="总请求数"  value={metrics?.total_requests ?? '—'} />
        <Card title="QPS"       value={metrics?.requests_per_second?.toFixed(2) ?? '—'} />
      </section>

      {/* Stat cards row 2 */}
      <section className="grid">
        <Card title="做种中 (2h)"  value={stats?.active_seeders  ?? '—'} accent="green" />
        <Card title="下载中 (2h)"  value={stats?.active_leechers ?? '—'} accent="yellow" />
        <Card title="完成次数"      value={stats?.downloaded ?? '—'} accent="orange" />
        <Card title="连接数"        value={metrics?.active_connections ?? '—'} />
      </section>

      {/* Sparkline charts */}
      <section className="chart-section">
        <h2>趋势（最近 {HISTORY_LEN} 次采样）</h2>
        <div className="sparklines">
          <SparkItem label="活跃 Peer" data={[...history.current.peers]} color="#3b82f6" />
          <SparkItem label="做种"      data={[...history.current.seeders]} color="#22c55e" />
          <SparkItem label="下载中"    data={[...history.current.leechers]} color="#eab308" />
          <SparkItem label="QPS"       data={[...history.current.rps]} color="#f97316" />
        </div>
      </section>

      {/* Torrent table */}
      <section className="torrent-list">
        <div className="torrent-list-header">
          <h2>种子列表（共 {total} 条）</h2>
          <div className="pagination">
            <button onClick={() => setPage(p => Math.max(1, p - 1))}
                    disabled={page <= 1 || loading}>«</button>
            <span>{page} / {totalPages}</span>
            <button onClick={() => setPage(p => Math.min(totalPages, p + 1))}
                    disabled={page >= totalPages || loading}>»</button>
          </div>
        </div>

        {torrents.length === 0 ? (
          <p className="empty">
            暂无种子记录。请在 qBittorrent 中添加 Tracker：
            <code>http://服务器IP:6969/announce</code>
          </p>
        ) : (
          <table>
            <thead>
              <tr>
                <th>Info Hash</th>
                <th>名称</th>
                <th>做种</th>
                <th>下载</th>
                <th>完成</th>
                <th>更新时间</th>
              </tr>
            </thead>
            <tbody>
              {torrents.map(t => (
                <tr key={t.info_hash} onClick={() => setSelectedHash(t.info_hash)}>
                  <td className="hash" title={t.info_hash}>{t.info_hash.slice(0, 12)}…</td>
                  <td className="name-cell" title={t.name || ''}>{t.name || <span style={{color:'var(--muted)'}}>—</span>}</td>
                  <td><span className="badge seed">{t.complete}</span></td>
                  <td><span className="badge leech">{t.incomplete}</span></td>
                  <td><span className="badge done">{t.downloaded}</span></td>
                  <td style={{color:'var(--muted)',fontSize:'0.78rem'}}>{t.updated_at || '—'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </section>

      {/* Footer */}
      <footer>
        <p>统计为近 2 小时活跃 Peer</p>
        <p>每 {REFRESH_MS / 1000} 秒自动刷新</p>
        <p>点击行查看 Peer 详情</p>
      </footer>

      {/* Peer detail modal */}
      {selectedHash && (
        <PeerModal infoHash={selectedHash} onClose={() => setSelectedHash(null)} />
      )}
    </div>
  );
}

function Card({ title, value, accent }) {
  return (
    <div className={`card ${accent || ''}`}>
      <h3>{title}</h3>
      <p className="value">{value}</p>
    </div>
  );
}

function SparkItem({ label, data, color }) {
  return (
    <div className="sparkline-item">
      <label>{label} <strong style={{ color }}>{data[data.length - 1] ?? 0}</strong></label>
      <Sparkline data={data} color={color} />
    </div>
  );
}
