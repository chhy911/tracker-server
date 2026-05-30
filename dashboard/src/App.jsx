import { useCallback, useEffect, useState } from 'react';

const API_BASE = import.meta.env.VITE_API_URL || 'http://localhost:8080';

async function fetchJson(path) {
  const res = await fetch(`${API_BASE}${path}`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

export default function App() {
  const [stats, setStats] = useState(null);
  const [metrics, setMetrics] = useState(null);
  const [health, setHealth] = useState(null);
  const [error, setError] = useState(null);
  const [loading, setLoading] = useState(true);

  const refresh = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const [s, m, h] = await Promise.all([
        fetchJson('/api/stats'),
        fetchJson('/api/metrics'),
        fetchJson('/api/health'),
      ]);
      setStats(s);
      setMetrics(m);
      setHealth(h);
    } catch (e) {
      setError(e.message || '无法连接 API');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 5000);
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
          <span className="hint">请确认 tracker-server 已在 8080 端口启动</span>
        </div>
      )}

      <section className="grid">
        <Card title="服务状态" value={health?.status === 'ok' ? '正常' : '未知'} accent="green" />
        <Card title="活跃连接" value={metrics?.active_connections ?? '—'} />
        <Card title="总请求数" value={metrics?.total_requests ?? '—'} />
        <Card title="QPS" value={metrics?.requests_per_second?.toFixed?.(2) ?? metrics?.requests_per_second ?? '—'} />
      </section>

      <section className="grid">
        <Card title="做种 (complete)" value={stats?.complete ?? '—'} />
        <Card title="下载中 (incomplete)" value={stats?.incomplete ?? '—'} />
        <Card title="已完成下载" value={stats?.downloaded ?? '—'} />
      </section>

      <footer>
        <p>API: {API_BASE} · 每 5 秒自动刷新</p>
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
