import { useCallback, useEffect, useState } from 'react';

const API_BASE = import.meta.env.VITE_API_URL || '';

async function fetchJson(path) {
  const url = `${API_BASE}${path}`;
  const res = await fetch(url);
  const text = await res.text();
  const ct = res.headers.get('content-type') || '';
  if (!res.ok) {
    throw new Error(`HTTP ${res.status} ${url}`);
  }
  if (!ct.includes('json') && text.trimStart().startsWith('<')) {
    throw new Error(
      'API 返回了网页而非 JSON。请用 http://服务器IP:8888 打开（不要只用 :3000）'
    );
  }
  try {
    return JSON.parse(text);
  } catch {
    throw new Error(`无效 JSON: ${text.slice(0, 80)}`);
  }
}

export default function App() {
  const [stats, setStats] = useState(null);
  const [metrics, setMetrics] = useState(null);
  const [health, setHealth] = useState(null);
  const [torrents, setTorrents] = useState([]);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const [s, m, h, t] = await Promise.all([
        fetchJson('/api/stats'),
        fetchJson('/api/metrics'),
        fetchJson('/api/health'),
        fetchJson('/api/torrents'),
      ]);
      setStats(s);
      setMetrics(m);
      setHealth(h);
      setTorrents(t.torrents || []);
    } catch (e) {
      setError(e.message || '无法连接 API');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 10000);
    return () => clearInterval(id);
  }, [refresh]);

  return (
    <div className="app">
      <header>
        <h1>BitTorrent Tracker</h1>
        <p className="subtitle">实时监控仪表板</p>
        <button type="button" onClick={refresh} disabled={loading}>
          {loading ? '刷新中…' : '立即刷新'}
        </button>
      </header>

      {error && (
        <div className="banner error">
          <strong>API 错误：</strong> {error}
          <span className="hint">请使用 http://服务器IP:8888 打开</span>
        </div>
      )}

      <section className="grid">
        <Card title="服务状态" value={health?.status === 'ok' ? '正常' : '未知'} accent="green" />
        <Card title="种子数量" value={stats?.torrent_count ?? '—'} />
        <Card title="活跃 Peer" value={stats?.peer_count ?? '—'} />
        <Card title="总请求数" value={metrics?.total_requests ?? '—'} />
      </section>

      <section className="grid">
        <Card title="做种 (2h内)" value={stats?.active_seeders ?? stats?.complete ?? '—'} />
        <Card title="下载中 (2h内)" value={stats?.active_leechers ?? stats?.incomplete ?? '—'} />
        <Card title="QPS" value={metrics?.requests_per_second?.toFixed?.(2) ?? metrics?.requests_per_second ?? '—'} />
        <Card title="连接数" value={metrics?.active_connections ?? '—'} />
      </section>

      <section className="torrent-list">
        <h2>种子列表</h2>
        {torrents.length === 0 ? (
          <p className="empty">暂无种子记录。请确认 qBittorrent 已添加 Tracker：<code>http://IP:6969/announce</code></p>
        ) : (
          <table>
            <thead>
              <tr>
                <th>Info Hash</th>
                <th>做种</th>
                <th>下载</th>
                <th>完成次数</th>
              </tr>
            </thead>
            <tbody>
              {torrents.map((t) => (
                <tr key={t.info_hash}>
                  <td className="hash" title={t.info_hash}>{t.info_hash.slice(0, 16)}…</td>
                  <td>{t.complete}</td>
                  <td>{t.incomplete}</td>
                  <td>{t.downloaded}</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </section>

      <footer>
        <p>统计为近 2 小时活跃 Peer · 每 5 秒刷新</p>
      </footer>
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
