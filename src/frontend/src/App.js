import React, { useState, useEffect, useRef, useMemo } from 'react';
import { Cpu, Wifi, Play, Pause, Bot, RotateCw, ChevronDown, Battery, Clock, Footprints, Gauge, RefreshCw, Zap, Timer, Download, CheckCircle2, XCircle, Calendar, Square, ChevronRight, Activity} from 'lucide-react';
import BluetoothPanel from './BluetoothPanel';
import { useMazeSimulator } from './useMazeSimulator';
import { CELL_MM, DX as DXR, DY as DYR, mmToCell } from './utils/maze';
import { useWebSocket } from './useWebSocket';
import { getHistorico, postTelemetria, postComando, batteryVoltsToPercent, getCorrida, parseTimeToSeconds, deleteHistorico, getSerialPortas, postSerialConectar, postSerialDesconectar, getSerialStatus } from './services/api';
import { sendConnectCommand } from './services/espService';

const ReplayCanvas = ({ pathMm, mazeSize, knownWalls }) => {
  const total = pathMm?.length ?? 0;
  const [idx, setIdx] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [speedMs, setSpeedMs] = useState(150);

  useEffect(() => { setIdx(0); }, [pathMm]);

  useEffect(() => {
    if (!playing || idx >= total - 1) return;
    const t = setTimeout(() => setIdx(i => Math.min(i + 1, total - 1)), speedMs);
    return () => clearTimeout(t);
  }, [playing, idx, total, speedMs]);

  useEffect(() => {
    if (idx >= total - 1) setPlaying(false);
  }, [idx, total]);

  const visited = new Set();
  const passable = new Set();
  for (let i = 0; i <= idx && i < total; i++) {
    const ax = mmToCell(pathMm[i].x), ay = mmToCell(pathMm[i].y);
    visited.add(`${ax},${ay}`);
    if (i > 0) {
      const bx = mmToCell(pathMm[i - 1].x), by = mmToCell(pathMm[i - 1].y);
      passable.add(`${ax},${ay}->${bx},${by}`);
      passable.add(`${bx},${by}->${ax},${ay}`);
    }
  }
  const cur = pathMm[idx] || pathMm[0];
  const rx = cur ? mmToCell(cur.x) : 0;
  const ry = cur ? mmToCell(cur.y) : 0;

  const hasPreciseWalls = Array.isArray(knownWalls) && knownWalls.length === mazeSize;
  const hasWallReplay = (cx, cy, d) => {
    if (hasPreciseWalls) {
      const cell = knownWalls[cx]?.[cy];
      return Array.isArray(cell) ? Boolean(cell[d]) : false;
    }
    const nx = cx + DXR[d];
    const ny = cy + DYR[d];
    if (nx < 0 || nx >= mazeSize || ny < 0 || ny >= mazeSize) return true;
    if (!visited.has(`${cx},${cy}`)) return false;
    if (!visited.has(`${nx},${ny}`)) return false;
    return !passable.has(`${cx},${cy}->${nx},${ny}`);
  };

  let robotDir = 0;
  for (let i = idx; i > 0; i--) {
    const prev = pathMm[i - 1], curP = pathMm[i];
    const dxCell = mmToCell(curP.x) - mmToCell(prev.x);
    const dyCell = mmToCell(curP.y) - mmToCell(prev.y);
    if (dxCell !== 0 || dyCell !== 0) {
      if (dyCell < 0) robotDir = 0;
      else if (dxCell > 0) robotDir = 1;
      else if (dyCell > 0) robotDir = 2;
      else robotDir = 3;
      break;
    }
  }

  const mid = Math.floor(mazeSize / 2);
  const goals = [
    { x: mid - 1, y: mid - 1 }, { x: mid, y: mid - 1 },
    { x: mid - 1, y: mid }, { x: mid, y: mid },
  ];

  return (
    <div className="bg-app-bg rounded-xl border border-border-rule p-3 h-full flex flex-col shadow-card">
      <div className="text-label mb-2 flex items-center justify-between">
        <span>Replay do Trajeto</span>
        <span className="text-brand-h1 font-mono normal-case text-xs">{idx + 1}/{total}</span>
      </div>
      <div
        className="aspect-square w-full max-w-[260px] mx-auto border-2 border-brand-purple-light rounded-md overflow-hidden bg-app-bg"
        style={{
          display: 'grid',
          gridTemplateColumns: `repeat(${mazeSize}, minmax(0, 1fr))`,
          gridTemplateRows: `repeat(${mazeSize}, minmax(0, 1fr))`,
        }}
      >
        {Array.from({ length: mazeSize * mazeSize }).map((_, i) => {
          const cy = Math.floor(i / mazeSize);
          const cx = i % mazeSize;
          const isVisited = visited.has(`${cx},${cy}`);
          const isGoal = goals.some(g => g.x === cx && g.y === cy);
          const isRobot = cx === rx && cy === ry;
          let bg = 'var(--fundo-nao-explorado)';
          if (isVisited) bg = 'var(--fundo-explorado)';
          if (isGoal) bg = 'var(--fundo-centro)';
          const faint = '1px solid var(--cor-parede)';
          const wall = '2.5px solid var(--cor-parede-conhecida)';
          return (
            <div key={i} style={{
              backgroundColor: bg,
              borderTop: hasWallReplay(cx, cy, 0) ? wall : faint,
              borderRight: hasWallReplay(cx, cy, 1) ? wall : faint,
              borderBottom: hasWallReplay(cx, cy, 2) ? wall : faint,
              borderLeft: hasWallReplay(cx, cy, 3) ? wall : faint,
              boxSizing: 'border-box',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}>
              {isRobot && (
                <div style={{
                  width: 0, height: 0,
                  borderLeft: '4px solid transparent',
                  borderRight: '4px solid transparent',
                  borderBottom: '8px solid var(--primary-2)',
                  transform: `rotate(${robotDir * 90}deg)`,
                }} />
              )}
            </div>
          );
        })}
      </div>
      <div className="mt-3 space-y-2">
        <input
          type="range" min={0} max={Math.max(0, total - 1)} value={idx}
          onChange={(e) => { setPlaying(false); setIdx(parseInt(e.target.value, 10)); }}
          className="w-full"
        />
        <div className="flex items-center justify-between">
          <div className="flex gap-1.5">
            <button onClick={() => setPlaying(p => !p)} className="bg-app-raised border border-border-rule text-brand-h1 px-3 py-1 rounded-lg text-xs font-semibold flex items-center gap-1 transition-all hover:bg-app-hover hover:border-border-accent">
              {playing ? <><Pause size={11} /><span>Pausar</span></> : <><Play size={11} /><span>Reproduzir</span></>}
            </button>
            <button onClick={() => { setIdx(0); setPlaying(true); }} className="bg-app-raised border border-border-rule text-brand-h1 px-3 py-1 rounded-lg text-xs font-semibold flex items-center gap-1 transition-all hover:bg-app-hover hover:border-border-accent">
              <RotateCw size={11} /><span>Reiniciar</span>
            </button>
          </div>
          <CustomSelect
            value={speedMs}
            onChange={(val) => setSpeedMs(parseInt(val, 10))}
            options={[
              { value: 400, label: '0.5×' },
              { value: 150, label: '1×' },
              { value: 75, label: '2×' },
              { value: 30, label: '5×' }
            ]}
            className="bg-app-raised border border-border-rule text-brand-h2 text-xs font-medium px-2 py-1 rounded-[16px] cursor-pointer min-w-[70px]"
            dropdownWidth="w-auto"
          />
        </div>
      </div>
    </div>
  );
};

const MiniMap = ({ snapshot }) => {
  const { gridSize, knownWalls, explored, goals, robot } = snapshot;

  return (
    <div className="w-full h-full flex flex-col items-center justify-center p-2 bg-app-bg relative rounded-xl border border-border-rule shadow-card">
      <h4 className="absolute top-3 left-0 right-0 text-center text-label z-10 pointer-events-none drop-shadow-md">Trajeto Mapeado</h4>
      <div
        className="aspect-square w-full max-w-[200px] border-2 border-brand-purple-light rounded-md overflow-hidden box-border bg-app-bg mt-6"
        style={{
          display: 'grid',
          gridTemplateColumns: `repeat(${gridSize}, minmax(0, 1fr))`,
          gridTemplateRows: `repeat(${gridSize}, minmax(0, 1fr))`
        }}
      >
        {Array.from({ length: gridSize * gridSize }).map((_, i) => {
          const y = Math.floor(i / gridSize);
          const x = i % gridSize;

          let style = {
            borderTop: knownWalls[x][y][0] ? '2px solid var(--cor-parede-conhecida)' : '1px solid var(--cor-parede)',
            borderRight: knownWalls[x][y][1] ? '2px solid var(--cor-parede-conhecida)' : '1px solid var(--cor-parede)',
            borderBottom: knownWalls[x][y][2] ? '2px solid var(--cor-parede-conhecida)' : '1px solid var(--cor-parede)',
            borderLeft: knownWalls[x][y][3] ? '2px solid var(--cor-parede-conhecida)' : '1px solid var(--cor-parede)',
            backgroundColor: 'var(--fundo-nao-explorado)',
            boxSizing: 'border-box'
          };

          if (explored[x][y]) style.backgroundColor = 'var(--fundo-explorado)';
          const isGoal = goals.some(g => g.x === x && g.y === y);
          if (isGoal) style.backgroundColor = 'var(--fundo-centro)';
          const isRobot = robot && robot.x === x && robot.y === y;

          return (
            <div key={i} style={style} className="relative flex items-center justify-center">
              {isRobot && (
                <div style={{
                  width: 0, height: 0,
                  borderLeft: '3px solid transparent',
                  borderRight: '3px solid transparent',
                  borderBottom: '6px solid var(--primary-2)',
                  transform: `rotate(${robot.direction * 90}deg)`,
                }} />
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};

const SettingsView = ({ wsUrl, setWsUrl, wsStatus, refreshHistory }) => {
  const [inputUrl, setInputUrl] = useState(wsUrl);
  const [showDeleteModal, setShowDeleteModal] = useState(false);
  const [deleteInput, setDeleteInput] = useState('');
  const [feedbackMessage, setFeedbackMessage] = useState(null);

  const [battMin, setBattMin] = useState(() => localStorage.getItem('BATT_VMIN') || '6.0');
  const [battMax, setBattMax] = useState(() => localStorage.getItem('BATT_VMAX') || '8.4');
  const [latencyLimit, setLatencyLimit] = useState(() => localStorage.getItem('LATENCY_THRESHOLD') || '500');

  // ── Conexão com o robô: ponte serial gerida pelo backend ──
  const [serialPorts, setSerialPorts] = useState([]);
  const [serialPort, setSerialPort] = useState('');
  const [serialBaud, setSerialBaud] = useState('921600');
  const [serialStatus, setSerialStatus] = useState(null);
  const [serialBusy, setSerialBusy] = useState(false);

  const loadSerialPorts = async () => {
    try {
      const ports = await getSerialPortas();
      setSerialPorts(ports);
      const sugerida = ports.find(p => p.suggested) || ports.find(p => p.bluetooth);
      setSerialPort(prev => prev || sugerida?.device || ports[0]?.device || '');
    } catch (e) {
      setFeedbackMessage({ title: 'Erro', text: 'Não foi possível listar as portas: ' + e.message, type: 'error' });
    }
  };

  useEffect(() => {
    loadSerialPorts();
    let alive = true;
    const tick = async () => {
      try { const st = await getSerialStatus(); if (alive) setSerialStatus(st); } catch { /* backend offline: ignora */ }
    };
    tick();
    const id = setInterval(tick, 3000);
    return () => { alive = false; clearInterval(id); };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const handleSerialConnect = async () => {
    if (!serialPort) {
      setFeedbackMessage({ title: 'Aviso', text: 'Selecione uma porta primeiro.', type: 'warning' });
      return;
    }
    setSerialBusy(true);
    try {
      const res = await postSerialConectar(serialPort, parseInt(serialBaud, 10));
      setSerialStatus(res.data ?? await getSerialStatus());
    } catch (e) {
      setFeedbackMessage({ title: 'Falha ao conectar', text: e.message, type: 'error' });
      try { setSerialStatus(await getSerialStatus()); } catch { /* ignora */ }
    } finally {
      setSerialBusy(false);
    }
  };

  const handleSerialDisconnect = async () => {
    setSerialBusy(true);
    try {
      const res = await postSerialDesconectar();
      setSerialStatus(res.data ?? await getSerialStatus());
    } catch (e) {
      setFeedbackMessage({ title: 'Erro', text: e.message, type: 'error' });
    } finally {
      setSerialBusy(false);
    }
  };

  const handleSaveWs = () => {
    localStorage.setItem('WS_URL', inputUrl);
    setWsUrl(inputUrl);
    setFeedbackMessage({ title: 'Sucesso', text: 'URL do WebSocket salva com sucesso!', type: 'success' });
  };

  const handleSaveBattery = () => {
    localStorage.setItem('BATT_VMIN', battMin);
    localStorage.setItem('BATT_VMAX', battMax);
    refreshHistory('Todos');
    setFeedbackMessage({ title: 'Sucesso', text: 'Calibração de bateria salva! Os percentuais foram recalculados.', type: 'success' });
  };

  const handleSaveLatency = () => {
    localStorage.setItem('LATENCY_THRESHOLD', latencyLimit);
    setFeedbackMessage({ title: 'Sucesso', text: 'Sensibilidade de conexão atualizada.', type: 'success' });
  };

  const handleExportCsv = async () => {
    try {
      const data = await getHistorico('Todos');
      if (!data || data.length === 0) {
        setFeedbackMessage({ title: 'Aviso', text: 'Não há dados para exportar.', type: 'warning' });
        return;
      }
      const headers = ['ID', 'Data', 'Origem', 'Labirinto', 'Status', 'Passos', 'Tempo', 'Velocidade', 'Bateria'];
      const rows = data.map(r => [r.id, r.date, r.source, r.maze, r.status, r.steps, r.time, r.speed, r.battery]);
      const csvContent = [headers.join(','), ...rows.map(e => e.join(','))].join('\n');
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.setAttribute('href', url);
      link.setAttribute('download', 'micromouse_historico.csv');
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    } catch (e) {
      setFeedbackMessage({ title: 'Erro', text: 'Erro ao exportar CSV: ' + e.message, type: 'error' });
    }
  };

  const handleClearDatabase = async () => {
    try {
      await deleteHistorico();
      setFeedbackMessage({ title: 'Sucesso', text: 'Banco de dados apagado com sucesso!', type: 'success' });
      refreshHistory('Todos');
      setShowDeleteModal(false);
      setDeleteInput('');
    } catch (e) {
      const text = e.message?.includes('403')
        ? 'Ação restrita: o histórico só pode ser apagado no computador que executa o backend (RNF-10).'
        : 'Erro ao apagar banco: ' + e.message;
      setFeedbackMessage({ title: 'Erro', text, type: 'error' });
      setShowDeleteModal(false);
      setDeleteInput('');
    }
  };

  const serialConnected = !!serialStatus?.connected;
  const portOptions = serialPorts.length
    ? serialPorts.map(p => ({
      value: p.device,
      label: `${p.device}${p.bluetooth ? ' (BT)' : ''}${p.description ? ' \u2014 ' + p.description : ''}`,
    }))
    : [{ value: '', label: 'Nenhuma porta encontrada' }];

  return (
    <div className="flex-1 bg-app-surface w-full h-full overflow-y-auto p-6 box-border">
      <div className="max-w-3xl mx-auto space-y-6">

        {/* Conexão com o robô — ponte serial gerida pelo backend */}
        <div className="bg-app-bg rounded-xl border border-border-rule p-6 shadow-card">
          <h2 className="text-xl font-bold text-brand-h1 mb-4">Conexão com o Robô</h2>
          <p className="text-brand-h3 text-sm mb-4">Selecione a porta serial (COM) do Bluetooth e conecte. O backend abre a porta diretamente — não é mais necessário rodar a ponte num terminal separado.</p>
          <div className="flex flex-col sm:flex-row gap-4 items-end">
            <div className="flex-1 w-full">
              <label className="block text-brand-h3 text-xs mb-1">Porta serial</label>
              <CustomSelect
                className="w-full bg-app-surface border border-border-rule rounded-[16px] px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple"
                value={serialPort}
                onChange={(val) => setSerialPort(val)}
                options={portOptions}
              />
            </div>
            <div className="w-full sm:w-32">
              <label className="block text-brand-h3 text-xs mb-1">Baud</label>
              <input
                type="number"
                className="w-full bg-app-surface border border-border-rule rounded-xl px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple"
                value={serialBaud}
                onChange={e => setSerialBaud(e.target.value)}
                placeholder="921600"
              />
            </div>
            <button
              onClick={loadSerialPorts}
              title="Atualizar lista de portas"
              className="h-[42px] px-3 flex items-center justify-center bg-app-surface hover:bg-app-hover border border-border-rule text-brand-h1 rounded-xl transition-colors"
            >
              <RefreshCw size={16} />
            </button>
            {serialConnected ? (
              <button
                onClick={handleSerialDisconnect}
                disabled={serialBusy}
                className="w-40 h-[42px] flex items-center justify-center bg-brand-danger hover:opacity-90 text-white font-medium text-sm rounded-xl transition-colors disabled:opacity-50"
              >
                Desconectar
              </button>
            ) : (
              <button
                onClick={handleSerialConnect}
                disabled={serialBusy}
                className="w-40 h-[42px] flex items-center justify-center bg-brand-purple hover:bg-brand-purple-light text-white font-medium text-sm rounded-xl transition-colors disabled:opacity-50"
              >
                {serialBusy ? 'Conectando...' : 'Conectar'}
              </button>
            )}
          </div>
          <div className="mt-4 flex items-center gap-2 flex-wrap">
            <span className="text-brand-h3 text-xs">Status:</span>
            <span className={`text-xs font-bold ${serialConnected ? 'text-brand-green' : serialBusy ? 'text-brand-accent' : 'text-brand-danger'}`}>
              {serialBusy ? 'Conectando...' : serialConnected ? `Conectado — ${serialStatus.port} (${serialStatus.packets_rx} pacotes)` : 'Desconectado'}
            </span>
            {!serialConnected && !serialBusy && serialStatus?.last_error && (
              <span className="text-xs text-brand-danger">— última falha: {serialStatus.last_error}</span>
            )}
          </div>
        </div>

        <div className="bg-app-bg rounded-xl border border-border-rule p-6 shadow-card">
          <h2 className="text-xl font-bold text-brand-h1 mb-4">Conexão WebSocket</h2>
          <p className="text-brand-h3 text-sm mb-4">Configure o endereço IP ou a URL do servidor de telemetria do robô.</p>
          <div className="flex flex-col sm:flex-row gap-4 items-end">
            <div className="flex-1 w-full">
              <label className="block text-brand-h3 text-xs mb-1">URL do WebSocket</label>
              <input type="text" className="w-full bg-app-surface border border-border-rule rounded-xl px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple" value={inputUrl} onChange={e => setInputUrl(e.target.value)} placeholder="ws://localhost:8000/ws/dashboard" />
            </div>
            <button onClick={handleSaveWs} className="w-48 h-[42px] flex items-center justify-center bg-brand-purple hover:bg-brand-purple-light text-white font-medium text-sm rounded-xl transition-colors whitespace-nowrap">Salvar Conexão</button>
          </div>
          <div className="mt-4 flex items-center gap-2">
            <span className="text-brand-h3 text-xs">Status atual:</span>
            <span className={`text-xs font-bold ${wsStatus === 'Conectado' ? 'text-brand-green' : wsStatus === 'Conectando...' || wsStatus === 'Reconectando...' ? 'text-brand-accent' : 'text-brand-danger'}`}>{wsStatus}</span>
          </div>
        </div>

        <div className="bg-app-bg rounded-xl border border-border-rule p-6 shadow-card">
          <h2 className="text-xl font-bold text-brand-h1 mb-4">Calibração da Bateria</h2>
          <p className="text-brand-h3 text-sm mb-4">Ajuste os valores mínimo e máximo de tensão (Volts) da bateria do seu robô para que o dashboard calcule corretamente a porcentagem de carga.</p>
          <div className="flex flex-col sm:flex-row gap-4 items-end">
            <div className="flex-1 w-full">
              <label className="block text-brand-h3 text-xs mb-1">Tensão Mínima (V) - 0%</label>
              <input type="number" step="0.1" className="w-full bg-app-surface border border-border-rule rounded-xl px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple" value={battMin} onChange={e => setBattMin(e.target.value)} placeholder="6.0" />
            </div>
            <div className="flex-1 w-full">
              <label className="block text-brand-h3 text-xs mb-1">Tensão Máxima (V) - 100%</label>
              <input type="number" step="0.1" className="w-full bg-app-surface border border-border-rule rounded-xl px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple" value={battMax} onChange={e => setBattMax(e.target.value)} placeholder="8.4" />
            </div>
            <button onClick={handleSaveBattery} className="w-48 h-[42px] flex items-center justify-center bg-brand-purple hover:bg-brand-purple-light text-white font-medium text-sm rounded-xl transition-colors whitespace-nowrap">Salvar Calibração</button>
          </div>
        </div>

        <div className="bg-app-bg rounded-xl border border-border-rule p-6 shadow-card">
          <h2 className="text-xl font-bold text-brand-h1 mb-4">Alertas de Conexão</h2>
          <p className="text-brand-h3 text-sm mb-4">Configure o limite de latência aceitável para o indicador de ping emitir avisos (Padrão RNF: 500ms).</p>
          <div className="flex flex-col sm:flex-row gap-4 items-end">
            <div className="flex-1 w-full">
              <label className="block text-brand-h3 text-xs mb-1">Sensibilidade do Ping (ms)</label>
              <CustomSelect
                className="w-full bg-app-surface border border-border-rule rounded-[16px] px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-purple"
                value={latencyLimit.toString()}
                onChange={(val) => setLatencyLimit(val)}
                options={[
                  { value: "300", label: "Rigoroso (300ms)" },
                  { value: "500", label: "Padrão (500ms)" },
                  { value: "1000", label: "Tolerante (1000ms)" },
                  { value: "2000", label: "Muito Tolerante (2000ms)" }
                ]}
              />
            </div>
            <button onClick={handleSaveLatency} className="w-48 h-[42px] flex items-center justify-center bg-brand-purple hover:bg-brand-purple-light text-white font-medium text-sm rounded-xl transition-colors whitespace-nowrap">Salvar Alerta</button>
          </div>
        </div>

        <div className="bg-app-bg rounded-xl border border-border-rule p-6 shadow-card">
          <h2 className="text-xl font-bold text-brand-h1 mb-4">Gerenciamento de Dados</h2>
          <p className="text-brand-h3 text-sm mb-6">Exporte as corridas consolidadas para planilhas ou apague o histórico de testes para iniciar uma nova sessão limpa na competição.</p>
          <div className="flex flex-col sm:flex-row gap-4">
            <button onClick={handleExportCsv} className="flex-1 bg-app-surface hover:bg-app-hover border border-border-rule text-brand-h1 font-medium text-sm px-6 py-3 rounded-xl transition-colors flex items-center justify-center gap-2">
              <Download size={16} />Exportar Histórico (CSV)
            </button>
            <button onClick={() => { setShowDeleteModal(true); setDeleteInput(''); }} className="flex-1 bg-app-surface hover:bg-brand-danger/20 border border-brand-danger/30 text-brand-danger font-medium text-sm px-6 py-3 rounded-xl transition-colors flex items-center justify-center gap-2">
              <XCircle size={16} />Limpar Banco de Dados
            </button>
          </div>
        </div>
      </div>

      {showDeleteModal && (
        <div className="absolute inset-0 z-50 bg-app-bg/80 backdrop-blur-sm flex items-center justify-center p-6" onClick={() => setShowDeleteModal(false)}>
          <div className="bg-panel w-full max-w-md shadow-pop flex flex-col p-6 rounded-xl border border-brand-danger/30" onClick={e => e.stopPropagation()}>
            <div className="flex justify-between items-start mb-4">
              <h3 className="text-xl font-bold text-brand-danger flex items-center gap-2"><XCircle size={24} />Atenção!</h3>
            </div>
            <p className="text-brand-h1 text-sm mb-4">Você está prestes a apagar permanentemente todo o histórico de corridas. Essa ação não pode ser desfeita.</p>
            <p className="text-brand-h3 text-sm mb-6">Para confirmar, digite a palavra <strong className="text-brand-h1 font-mono">deletar</strong> no campo abaixo:</p>
            <input type="text" className="w-full bg-app-surface border border-brand-danger/50 rounded-xl px-3 py-2 text-brand-h1 text-sm focus:outline-none focus:border-brand-danger mb-6 font-mono text-center" value={deleteInput} onChange={e => setDeleteInput(e.target.value)} placeholder="deletar" />
            <div className="flex gap-4">
              <button onClick={() => setShowDeleteModal(false)} className="flex-1 bg-app-surface hover:bg-app-hover border border-border-rule text-brand-h1 font-medium text-sm px-4 py-2 rounded-xl transition-colors">Cancelar</button>
              <button onClick={handleClearDatabase} disabled={deleteInput !== 'deletar'} className="flex-1 bg-brand-danger hover:bg-red-600 disabled:opacity-50 disabled:cursor-not-allowed text-white font-medium text-sm px-4 py-2 rounded-xl transition-colors">Sim, Apagar Histórico</button>
            </div>
          </div>
        </div>
      )}

      {feedbackMessage && (
        <div className="absolute inset-0 z-50 bg-app-bg/80 backdrop-blur-sm flex items-center justify-center p-6" onClick={() => setFeedbackMessage(null)}>
          <div className="bg-panel w-full max-w-sm shadow-pop flex flex-col p-6 rounded-xl border border-border-rule text-center" onClick={e => e.stopPropagation()}>
            <div className="mb-4 flex justify-center">
              {feedbackMessage.type === 'success' ? (
                <div className="w-12 h-12 bg-brand-green/20 rounded-full flex items-center justify-center text-brand-green"><CheckCircle2 size={24} /></div>
              ) : feedbackMessage.type === 'error' ? (
                <div className="w-12 h-12 bg-brand-danger/20 rounded-full flex items-center justify-center text-brand-danger"><XCircle size={24} /></div>
              ) : (
                <div className="w-12 h-12 bg-brand-accent/20 rounded-full flex items-center justify-center text-brand-accent"><CheckCircle2 size={24} /></div>
              )}
            </div>
            <h3 className="text-xl font-bold text-brand-h1 mb-2">{feedbackMessage.title}</h3>
            <p className="text-brand-h3 text-sm mb-6">{feedbackMessage.text}</p>
            <button onClick={() => setFeedbackMessage(null)} className="w-full bg-brand-purple hover:bg-brand-purple-light text-white font-medium text-sm px-4 py-2.5 rounded-xl transition-colors">Entendi</button>
          </div>
        </div>
      )}
    </div>
  );
};

const App = () => {
  const [activeTab, setActiveTab] = useState('Mapa');
  const sim = useMazeSimulator();

  const [wsUrl, setWsUrl] = useState(() => localStorage.getItem('WS_URL') || 'ws://localhost:8000/ws/dashboard');
  const { status: wsStatus, lastMessage } = useWebSocket(wsUrl);

  // ── Status do ESP32 ───────────────────────────────────────────────────
  const [espStatus, setEspStatus] = useState('idle');
  const [esp32Online, setEsp32Online] = useState(false);
  const lastMsgTimeRef = useRef(null);

  // Detecta mensagem real do ESP32 e marca online
  useEffect(() => {
    if (!lastMessage || typeof lastMessage !== 'object') return;
    if (lastMessage.source === 'simulator') return;
    lastMsgTimeRef.current = Date.now();
    setEsp32Online(true);
  }, [lastMessage]);

  // Timeout: se passar 5s sem mensagem real, marca offline
  useEffect(() => {
    const interval = setInterval(() => {
      if (lastMsgTimeRef.current && Date.now() - lastMsgTimeRef.current > 5000) {
        setEsp32Online(false);
      }
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  const handleConnectEsp = async () => {
    if (espStatus === 'connecting' || espStatus === 'success') return;
    setEspStatus('connecting');
    try {
      await sendConnectCommand();
      setEspStatus('success');
      setTimeout(() => setEspStatus('idle'), 3000);
    } catch (err) {
      console.error('Falha ao conectar no ESP:', err);
      setEspStatus('error');
      setTimeout(() => setEspStatus('idle'), 4000);
    }
  };

  // ── Telemetria viva derivada do WebSocket ─────────────────────────────
  const [liveTelemetry, setLiveTelemetry] = useState(null);
  const [latencyMs, setLatencyMs] = useState(null);
  const [packetsRx, setPacketsRx] = useState(0);

  // Paredes descobertas pelo robô real (chegam em pacotes esparsos; persiste a última)
  const [liveWalls, setLiveWalls] = useState(null);

  // Reiniciar no modo Corrida: ESP32 volta a aguardar 'start'; limpa a vista ao vivo
  const handleResetReal = async () => {
    try {
      await postComando('reset');
    } catch (err) {
      console.error('Falha ao enviar reset ao robô:', err);
    }
    setLiveTelemetry(null);
    setLiveWalls(null);
  };

  useEffect(() => {
    if (!lastMessage || typeof lastMessage !== 'object') return;
    setLiveTelemetry(lastMessage);
    setPacketsRx(prev => prev + 1);
    if (lastMessage.event === 'start_race') {
      setLiveWalls(lastMessage.known_walls ?? null);  // nova corrida: zera o mapa
    } else if (lastMessage.known_walls) {
      setLiveWalls(lastMessage.known_walls);
    }
    if (lastMessage.timestamp) {
      const t = Date.parse(lastMessage.timestamp);
      if (!Number.isNaN(t)) {
        setLatencyMs(Math.max(0, Date.now() - t));
      }
    }
  }, [lastMessage]);

  // ── Histórico ─────────────────────────────────────────────────────────
  const [apiHistory, setApiHistory] = useState([]);
  const [simHistory, setSimHistory] = useState([]);
  const [historyFilter, setHistoryFilter] = useState('Todos');
  const [historyLoading, setHistoryLoading] = useState(false);
  const [historyError, setHistoryError] = useState(null);

  const refreshHistory = React.useCallback(async (filter) => {
    setHistoryLoading(true);
    setHistoryError(null);
    try {
      const items = await getHistorico(filter);
      setApiHistory(items);
    } catch (err) {
      setHistoryError(err.message || 'Erro ao carregar histórico');
      setApiHistory([]);
    } finally {
      setHistoryLoading(false);
    }
  }, []);

  useEffect(() => {
    if (activeTab === 'Histórico') {
      refreshHistory(historyFilter);
    }
  }, [activeTab, historyFilter, refreshHistory]);

  useEffect(() => {
    if (lastMessage && lastMessage.race_status === 'finished') {
      const t = setTimeout(() => refreshHistory(historyFilter), 200);
      return () => clearTimeout(t);
    }
  }, [lastMessage, historyFilter, refreshHistory]);

  // Sessão do simulador → POSTa em /telemetria com source='simulator'.
  const lastSimRunRef = useRef(null);
  useEffect(() => {
    const status = sim.memory.status;
    if (status !== 'Centro Alcançado!' && status !== 'Preso!') return;
    if (sim.memory.timeMs <= 0) return;
    const runId = `sim-${sim.memory.bfsCount}-${sim.memory.steps}-${sim.memory.timeMs}`;
    if (lastSimRunRef.current === runId) return;
    lastSimRunRef.current = runId;

    const mem = sim.memory;
    const success = status === 'Centro Alcançado!';
    const robotPathPoint = {
      x: mem.robot.x * CELL_MM + CELL_MM / 2,
      y: mem.robot.y * CELL_MM + CELL_MM / 2,
      z: 9.81,
    };
    const path = (mem.pathHistory && mem.pathHistory.length > 0) ? mem.pathHistory : [robotPathPoint];

    const size = sim.gridSize;
    const walls = (mem.truthWalls && mem.truthWalls.length === size)
      ? Array.from({ length: size }, (_, x) =>
        Array.from({ length: size }, (_, y) => {
          const w = mem.truthWalls[x][y];
          return [Boolean(w[0]), Boolean(w[1]), Boolean(w[2]), Boolean(w[3])];
        })
      )
      : null;

    const payload = {
      robot_id: 'sim_browser',
      timestamp: new Date().toISOString(),
      maze_type: `${sim.gridSize}x${sim.gridSize}`,
      current_position: { ...robotPathPoint, orientation: mem.robot.dir * 90 },
      path_traversed: path,
      battery_voltage_v: mem.batteryEndV ?? 7.1,
      speed_mm_s: 0.0,
      elapsed_time_ms: Math.max(1, Math.round(mem.timeMs)),
      race_status: 'finished',
      event: success ? 'race_ended' : 'simulator_stuck',
      message: success ? 'Simulador: centro alcançado.' : 'Simulador: robô preso.',
      source: 'simulator',
      success,
      known_walls: walls,
    };

    postTelemetria(payload)
      .then(() => {
        if (activeTab === 'Histórico') refreshHistory(historyFilter);
      })
      .catch((err) => {
        console.warn('Falha ao persistir corrida simulada — mantendo em memória local.', err);
        const fallbackRun = {
          id: runId,
          source: 'simulator',
          success,
          date: new Date().toLocaleString('pt-BR'),
          maze: `${sim.gridSize}x${sim.gridSize}`,
          status,
          time: `${Math.floor(mem.timeMs / 60000)}m ${((mem.timeMs % 60000) / 1000).toFixed(1)}s`,
          speed: mem.timeMs > 0 ? ((mem.steps * 18) / (mem.timeMs / 1000)).toFixed(1) + ' cm/s' : '0.0 cm/s',
          battery: '100%',
          steps: mem.steps,
          mapSnapshot: {
            gridSize: sim.gridSize,
            knownWalls: JSON.parse(JSON.stringify(mem.knownWalls)),
            explored: JSON.parse(JSON.stringify(mem.explored)),
            goals: JSON.parse(JSON.stringify(mem.goals)),
            robot: JSON.parse(JSON.stringify(mem.robot)),
          },
        };
        setSimHistory(prev => [fallbackRun, ...prev]);
      });
  }, [sim.memory.status, sim.memory.timeMs, sim.memory.steps, sim.memory.bfsCount, sim.gridSize, sim.memory.knownWalls, sim.memory.explored, sim.memory.goals, sim.memory.robot, sim.memory, activeTab, historyFilter, refreshHistory]);

  const combinedHistory = useMemo(() => {
    const simFiltered = historyFilter === 'Todos' ? simHistory : simHistory.filter(h => h.maze === historyFilter);
    return [...simFiltered, ...apiHistory];
  }, [simHistory, apiHistory, historyFilter]);

  const batteryPct = liveTelemetry?.battery_voltage_v != null
    ? batteryVoltsToPercent(liveTelemetry.battery_voltage_v)
    : null;

  const [mockMode, setMockMode] = useState('simulator');

  const dataMode = useMemo(() => mockMode, [mockMode]);

  const runStatus = useMemo(() => {
    if (dataMode === 'real' && liveTelemetry) {
      if (liveTelemetry.event === 'objective_found') return 'Objetivo localizado!';
      if (liveTelemetry.event === 'race_ended' || liveTelemetry.race_status === 'finished') {
        return liveTelemetry.success === false ? 'Preso!' : 'Centro Alcançado!';
      }
      if (liveTelemetry.race_status === 'error' || liveTelemetry.event === 'error_occurred') return 'Erro!';
      if (liveTelemetry.event === 'start_race' || liveTelemetry.race_status === 'running') return 'Mapeando...';
      if (liveTelemetry.race_status === 'paused') return 'Pausado';
      if (liveTelemetry.race_status === 'ready') return 'Pronto';
    }
    return sim.memory.status;
  }, [dataMode, liveTelemetry, sim.memory.status]);

  const liveRobot = useMemo(() => {
    if (dataMode !== 'real' || !liveTelemetry?.current_position) return null;
    const p = liveTelemetry.current_position;
    const dir = Math.round(((p.orientation ?? 0) / 90)) % 4;
    return { x: mmToCell(p.x), y: mmToCell(p.y), dir: (dir + 4) % 4 };
  }, [dataMode, liveTelemetry]);

  const liveExplored = useMemo(() => {
    if (dataMode !== 'real' || !liveTelemetry?.path_traversed) return null;
    const size = sim.gridSize;
    const arr = Array(size).fill(null).map(() => Array(size).fill(false));
    for (const p of liveTelemetry.path_traversed) {
      const cx = mmToCell(p.x), cy = mmToCell(p.y);
      if (cx >= 0 && cx < size && cy >= 0 && cy < size) arr[cx][cy] = true;
    }
    return arr;
  }, [dataMode, liveTelemetry, sim.gridSize]);

  return (
    <div className="font-sans h-screen flex flex-col overflow-hidden">
      <Header activeTab={activeTab} setActiveTab={setActiveTab} wsStatus={wsStatus} sim={sim} espStatus={espStatus} onConnectEsp={handleConnectEsp} />
      <div className="w-full flex-grow flex flex-col overflow-hidden relative">
        <main className="flex-grow flex flex-col lg:flex-row gap-4 h-full min-h-0 overflow-hidden">
          {activeTab === 'Histórico' ? (
            <div className="w-full h-full flex flex-col overflow-hidden min-h-0">
              <HistoryView historyData={combinedHistory} filter={historyFilter} setFilter={setHistoryFilter} loading={historyLoading} error={historyError} onRefresh={() => refreshHistory(historyFilter)} />
            </div>
          ) : activeTab === 'Configurações' ? (
            <SettingsView wsUrl={wsUrl} setWsUrl={setWsUrl} wsStatus={wsStatus} refreshHistory={refreshHistory} />
          ) : activeTab === 'Bluetooth' ? (
            <div className="w-full h-full flex flex-col overflow-hidden min-h-0 bg-app-bg"><BluetoothPanel /></div>
          ) : (
            <>
              <section className="flex-grow bg-app-surface border-r border-border-rule p-5 flex flex-col relative overflow-hidden min-h-0">
                <MazeCanvas sim={sim} liveRobot={liveRobot} liveExplored={liveExplored} liveWalls={liveWalls} dataMode={dataMode} mockMode={mockMode} setMockMode={setMockMode} liveRaceStatus={liveTelemetry?.race_status} onResetReal={handleResetReal} />
              </section>
              <aside className="w-full lg:w-[372px] flex flex-col gap-2 overflow-y-auto shrink-0 my-4 mr-4 pr-1 custom-scrollbar">
                <TelemetrySidebar
                  sim={sim}
                  wsStatus={wsStatus}
                  batteryPct={batteryPct}
                  latencyMs={latencyMs}
                  packetsRx={packetsRx}
                  liveTelemetry={liveTelemetry}
                  dataMode={dataMode}
                  runStatus={runStatus}
                  esp32Online={esp32Online}
                />
              </aside>
            </>
          )}
        </main>
      </div>
    </div>
  );
};

/* ============================================================
   CABEÇALHO
   ============================================================ */
const Header = ({ activeTab, setActiveTab, wsStatus, sim, espStatus = 'idle', onConnectEsp }) => {
  const tabs = ['Mapa', 'Telemetria', 'Histórico', 'Configurações', 'Bluetooth'];

  const btnConfig = {
    idle: { label: 'Conectar no ESP', icon: <Wifi size={14} />, cls: 'bg-brand-purple hover:bg-brand-purple-light text-white border-transparent' },
    connecting: { label: 'Conectando...', icon: <RefreshCw size={14} className="animate-spin" />, cls: 'bg-app-raised border-border-dim text-brand-h3 cursor-not-allowed opacity-70' },
    success: { label: 'Conectado!', icon: <CheckCircle2 size={14} />, cls: 'bg-brand-green/20 border-brand-green/30 text-brand-green cursor-default' },
    error: { label: 'Falhou — Tentar novamente', icon: <XCircle size={14} />, cls: 'bg-red-500/20 border-red-500/30 text-red-400 hover:bg-red-500/30' },
  };

  const btn = btnConfig[espStatus] ?? btnConfig.idle;

  return (
    <header className="relative flex items-center justify-center h-16 px-5 shrink-0 w-full border-b border-border-subtle bg-[#080614]" data-screen-label="Header">
      <div className="absolute left-5 flex items-center gap-3">
        <div className="w-10 h-10 rounded-xl grid place-items-center text-white bg-brand-purple shrink-0">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <rect x="4" y="4" width="16" height="16" rx="2" />
            <path d="M9 2v2M15 2v2M9 20v2M15 20v2M2 9h2M2 15h2M20 9h2M20 15h2" />
            <circle cx="12" cy="12" r="3" />
          </svg>
        </div>
        <div className="font-bold text-lg tracking-tight text-brand-h1 leading-none flex items-center">
          micro<span className="text-brand-purple-light">mouse</span><span className="text-brand-h3">.</span>
        </div>
      </div>

      <nav data-testid="pill-container" className="hidden md:flex pill-container">
        {tabs.map((tab) => (
          <button key={tab} data-testid="pill-item" onClick={() => setActiveTab(tab)}
            className={`pill-item font-medium text-sm transition-all ${activeTab === tab ? 'text-white bg-brand-purple' : 'text-brand-h3 hover:text-brand-h1 hover:bg-white/[0.03]'}`}
            {...(activeTab === tab ? { 'aria-current': 'page' } : {})}>
            {tab}
          </button>
        ))}
      </nav>

      <div className="absolute right-5 flex items-center gap-3">
        <button data-testid="btn-conectar-esp" onClick={onConnectEsp} disabled={espStatus === 'connecting' || espStatus === 'success'}
          className={`flex items-center gap-2 h-9 px-4 rounded-xl border text-sm font-medium transition-all ${btn.cls}`}>
          {btn.icon}
          <span>{btn.label}</span>
        </button>
      </div>
    </header>
  );
};

const CustomSelect = ({ value, onChange, options, className = "", dropdownWidth = "w-full", "data-testid": dataTestId }) => {
  const [isOpen, setIsOpen] = useState(false);
  const dropdownRef = useRef(null);

  useEffect(() => {
    const handleClick = (e) => {
      if (dropdownRef.current && !dropdownRef.current.contains(e.target)) setIsOpen(false);
    };
    document.addEventListener('mousedown', handleClick);
    return () => document.removeEventListener('mousedown', handleClick);
  }, []);

  const selectedLabel = options.find(o => o.value === value)?.label || value;

  return (
    <div data-testid={dataTestId} className={`relative flex items-center justify-between cursor-pointer group ${className}`} ref={dropdownRef} onClick={() => setIsOpen(!isOpen)}>
      <span className="truncate pr-2 select-none pointer-events-none">{selectedLabel}</span>
      <ChevronDown size={14} className={`pointer-events-none transition-transform duration-200 text-brand-h3 group-hover:text-brand-h1 ${isOpen ? 'rotate-180' : ''}`} />
      {isOpen && (
        <div className={`absolute top-[calc(100%+8px)] left-1/2 -translate-x-1/2 ${dropdownWidth} min-w-[160px] bg-app-surface border border-border-rule rounded-xl shadow-[0_8px_30px_rgb(0,0,0,0.5)] overflow-hidden z-[100] animate-in fade-in zoom-in-95 duration-200 origin-top`}>
          {options.map((opt) => (
            <div key={opt.value}
              className={`px-4 py-2.5 text-sm font-medium transition-colors select-none ${value === opt.value ? 'bg-brand-purple/15 text-brand-purple-light' : 'text-brand-h1 hover:bg-white/[0.04]'}`}
              onClick={(e) => { e.stopPropagation(); onChange(opt.value); setIsOpen(false); }}>
              {opt.label}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

/* ============================================================
   CANVAS DO LABIRINTO
   ============================================================ */
const MazeCanvas = ({ sim, liveRobot, liveExplored, liveWalls, dataMode, mockMode, setMockMode, liveRaceStatus, onResetReal }) => {
  const { memory, isRunning, setIsRunning, speed, setSpeed, showTruth, setShowTruth, resetSimulation, gridSize, changeGridSize } = sim;
  const mem = memory;
  const robotShown = liveRobot ?? mem.robot;
  const exploredShown = liveExplored ?? mem.explored;
  const isRealMode = dataMode === 'real';

  // Modo Corrida: "Iniciar" envia o comando ao robô real via backend/ponte
  const liveRunning = isRealMode && liveRaceStatus === 'running';
  const [startPending, setStartPending] = useState(false);
  const handleStartReal = async () => {
    setStartPending(true);
    try {
      // O seletor de matriz define o labirinto em que o robô vai operar
      await postComando(`start ${gridSize}`);
    } catch (err) {
      console.error('Falha ao enviar comando de início ao robô:', err);
    } finally {
      // Libera o botão após alguns segundos caso a telemetria não comece
      setTimeout(() => setStartPending(false), 5000);
    }
  };

  // Modo Corrida: flood fill recalculado das paredes publicadas pelo robô —
  // mesmo algoritmo do simulador, logo mesmos valores/cores nas células.
  const liveDistances = useMemo(() => {
    if (!isRealMode || !Array.isArray(liveWalls) || liveWalls.length === 0) return null;
    const size = liveWalls.length;
    const dist = Array(size).fill(null).map(() => Array(size).fill(255));
    const mid = Math.floor(size / 2);
    const queue = [];
    for (const [gx, gy] of [[mid - 1, mid - 1], [mid, mid - 1], [mid - 1, mid], [mid, mid]]) {
      if (dist[gx]?.[gy] !== undefined) { dist[gx][gy] = 0; queue.push([gx, gy]); }
    }
    while (queue.length > 0) {
      const [cx, cy] = queue.shift();
      for (let d = 0; d < 4; d++) {
        if (liveWalls[cx][cy][d]) continue;
        const nx = cx + DXR[d], ny = cy + DYR[d];
        if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
        if (dist[nx][ny] === 255) {
          dist[nx][ny] = dist[cx][cy] + 1;
          queue.push([nx, ny]);
        }
      }
    }
    return dist;
  }, [isRealMode, liveWalls]);

  if (!mem.truthWalls || mem.truthWalls.length === 0) {
    return (
      <div className="flex-grow flex items-center justify-center relative p-4 bg-app-bg rounded-xl border border-border-rule overflow-hidden">
        <div className="text-brand-h3 font-medium text-sm">Carregando simulador...</div>
      </div>
    );
  }

  // Fonte das distâncias: robô real (flood fill das paredes recebidas) ou simulador
  const getDist = (x, y) => {
    const d = isRealMode ? liveDistances?.[x]?.[y] : mem.distances[x][y];
    return d === undefined || d === null ? 255 : d;
  };

  let maxD = 0;
  for (let x = 0; x < gridSize; x++)
    for (let y = 0; y < gridSize; y++)
      if (getDist(x, y) !== 255 && getDist(x, y) > maxD) maxD = getDist(x, y);

  const GOALS = mem.goals || [];

  return (
    <>
      <div className="flex flex-col xl:flex-row justify-between items-center mb-4 z-10 gap-4 shrink-0 w-full">
        <div className="flex items-center gap-4 h-10 px-4 rounded-xl bg-app-inset border border-border-rule box-border">
          {/* Matrix selector */}
          <div className="flex items-center gap-2">
            <span className="text-label">Matriz</span>
            <CustomSelect
              aria-label="Tamanho da matriz"
              className="h-[28px] bg-app-raised border border-border-subtle hover:bg-app-hover hover:border-border-accent rounded-[16px] transition-all px-3 text-brand-h1 font-sans font-semibold text-sm"
              value={gridSize}
              onChange={(val) => changeGridSize(parseInt(val))}
              dropdownWidth="w-auto"
              options={[
                { value: 4, label: '4x4' },
                { value: 8, label: '8x8' },
                { value: 16, label: '16x16' }
              ]}
            />
          </div>

          <div className="w-px h-5 bg-border-rule" />

          {/* Controle de velocidade (só no Simulador; no modo Corrida quem dita o ritmo é o robô) */}
          <div className={`flex items-center gap-3 h-full ${isRealMode ? 'opacity-40' : ''}`}
            title={isRealMode ? 'Disponível só no modo Simulador' : undefined}>
            <span className="text-label">Velocidade</span>
            <div className="flex items-center h-full">
              <input
                type="range" min="10" max="500"
                value={510 - speed}
                disabled={isRealMode}
                onChange={(e) => setSpeed(510 - parseInt(e.target.value))}
                className={`w-24 ${isRealMode ? 'cursor-not-allowed' : ''}`}
                style={{ '--fill': `${((510 - speed - 10) / 490) * 100}%` }}
              />
            </div>
          </div>

          <div className="w-px h-5 bg-border-rule" />

          {/* Alternador Modo */}
          <label className="flex items-center gap-2 h-full cursor-pointer group" title="Mock de Modo (Simulador vs Corrida)">
            <input type="checkbox" className="sr-only" checked={mockMode === 'real'} onChange={(e) => setMockMode(e.target.checked ? 'real' : 'simulator')} aria-label="Alternar Modo de Operação" />
            <div className={`toggle-switch ${mockMode === 'real' ? 'active' : ''}`} />
            <span className="text-label w-[75px] text-left">{mockMode === 'real' ? 'Corrida' : 'Simulador'}</span>
          </label>

          <div className="w-px h-5 bg-border-rule" />

          {/* Alternador Raio-X */}
          <label className={`flex items-center gap-2 h-full ${isRealMode ? 'opacity-40 cursor-not-allowed' : 'cursor-pointer'}`} title={isRealMode ? 'Raio-X só disponível no modo Simulador' : ''}>
            <input type="checkbox" className="sr-only" checked={showTruth && !isRealMode} disabled={isRealMode} onChange={(e) => setShowTruth(e.target.checked)} />
            <div className={`toggle-switch ${showTruth && !isRealMode ? 'active' : ''}`} />
            <span className="text-label">Raio-X</span>
          </label>
        </div>

        {/* Botões de ação */}
        <div data-testid="pill-container" className="pill-container">
          <button
            data-testid="pill-item"
            onClick={() => (isRealMode ? handleStartReal() : setIsRunning(!isRunning))}
            disabled={isRealMode && (liveRunning || startPending)}
            title={isRealMode ? 'Dispara uma nova corrida no robô real' : undefined}
            className={`group pill-item gap-2 font-semibold text-sm transition-all w-[100px] ${isRealMode && (liveRunning || startPending)
              ? 'bg-app-raised border border-border-subtle text-brand-h3 opacity-60 cursor-not-allowed'
              : (!isRealMode && isRunning)
                ? 'bg-app-raised border border-border-subtle text-brand-h2 hover:text-brand-h1 hover:border-border-accent'
                : 'text-white border border-transparent bg-brand-purple'
              }`}
          >
            {isRealMode ? (
              liveRunning
                ? <><Activity size={16} className="animate-pulse" /><span>Correndo</span></>
                : startPending
                  ? <><Activity size={16} className="animate-pulse" /><span>Enviando</span></>
                  : <><Play size={16} className="transition-transform group-hover:scale-110 group-active:scale-90" /><span>Iniciar</span></>
            ) : isRunning ? (
              <><Square size={16} className="transition-transform group-active:scale-90" /><span>Pausar</span></>
            ) : (
              <><Play size={16} className="transition-transform group-hover:scale-110 group-active:scale-90" /><span>Iniciar</span></>
            )}
          </button>
          <button
            data-testid="pill-item"
            onClick={() => (isRealMode ? onResetReal() : resetSimulation(false))}
            title={isRealMode ? 'Aborta a corrida e coloca o robô em espera por um novo start' : undefined}
            className="group pill-item gap-2 font-semibold text-sm bg-app-raised border border-border-subtle text-brand-h1 transition-all hover:bg-app-hover hover:border-border-accent"
          >
            <Bot size={16} className="transition-transform group-hover:-translate-y-0.5 group-active:scale-90" /><span>Reiniciar</span>
          </button>
          <button
            data-testid="pill-item"
            onClick={() => resetSimulation(true)}
            disabled={isRealMode}
            title={isRealMode ? 'Disponível só no modo Simulador' : undefined}
            className={`group pill-item gap-2 font-semibold text-sm bg-app-raised border border-border-subtle text-brand-h1 transition-all ${isRealMode ? 'opacity-40 cursor-not-allowed' : 'hover:bg-app-hover hover:border-border-accent'}`}
          >
            <RotateCw size={16} className="transition-transform duration-300 group-hover:rotate-180 group-active:scale-90" /><span>Novo</span>
          </button>
        </div>
        <div className="w-px h-5 bg-border-rule" />
        <label className="flex items-center gap-2 h-full cursor-pointer group" title="Mock de Modo (Simulador vs Corrida)">
          <input type="checkbox" className="sr-only" checked={mockMode === 'real'} onChange={(e) => setMockMode(e.target.checked ? 'real' : 'simulator')} aria-label="Alternar Modo de Operação" />
          <div className={`toggle-switch ${mockMode === 'real' ? 'active' : ''}`} />
          <span className="text-label w-[75px] text-left">{mockMode === 'real' ? 'Corrida' : 'Simulador'}</span>
        </label>
        <div className="w-px h-5 bg-border-rule" />
        <label className={`flex items-center gap-2 h-full ${isRealMode ? 'opacity-40 cursor-not-allowed' : 'cursor-pointer'}`} title={isRealMode ? 'Raio-X só disponível no modo Simulador' : ''}>
          <input type="checkbox" className="sr-only" checked={showTruth && !isRealMode} disabled={isRealMode} onChange={(e) => setShowTruth(e.target.checked)} />
          <div className={`toggle-switch ${showTruth && !isRealMode ? 'active' : ''}`} />
          <span className="text-label">Raio-X</span>
        </label>
      </div>


      <div className="flex-1 flex items-center justify-center relative p-4 bg-app-bg rounded-xl border border-border-rule overflow-hidden min-h-0" style={{ containerType: 'size' }}>
        {(() => {
          // Lattice (2N+1)x(2N+1): piso, parede e quina são objetos independentes.
          const L = 2 * gridSize + 1;
          // Trilhas em px inteiro (ver #maze-container): parede/poste = var(--tw),
          // piso = var(--tf). Garante espessura igual em todo item do mesmo grupo.
          const tmpl = Array.from({ length: L }, (_, k) => (k % 2 === 0 ? 'var(--tw)' : 'var(--tf)')).join(' ');

          const expl = (xx, yy) => !!exploredShown[xx]?.[yy];
          const wallsSrc = isRealMode ? liveWalls : mem.knownWalls;
          const wallAt = (xx, yy, dd) => !!wallsSrc?.[xx]?.[yy]?.[dd];
          const truthAt = (xx, yy, dd) => !!mem.truthWalls?.[xx]?.[yy]?.[dd];

          // Uma parede do lattice é "conhecida" se for borda externa ou já descoberta.
          const hWallKnown = (rr, cc) => {
            if (rr < 0 || rr > 2 * gridSize || cc < 1 || cc > 2 * gridSize - 1) return false;
            const x = (cc - 1) / 2, yBelow = rr / 2;
            if (yBelow === 0 || yBelow === gridSize) return true;
            return wallAt(x, yBelow, 0) && (expl(x, yBelow) || expl(x, yBelow - 1));
          };
          const vWallKnown = (rr, cc) => {
            if (cc < 0 || cc > 2 * gridSize || rr < 1 || rr > 2 * gridSize - 1) return false;
            const y = (rr - 1) / 2, xRight = cc / 2;
            if (xRight === 0 || xRight === gridSize) return true;
            return wallAt(xRight, y, 3) && (expl(xRight, y) || expl(xRight - 1, y));
          };
          // Parede real (Raio-X): só conta para acender a quina no modo Simulador.
          const hWallTruth = (rr, cc) => {
            if (rr < 0 || rr > 2 * gridSize || cc < 1 || cc > 2 * gridSize - 1) return false;
            const x = (cc - 1) / 2, yBelow = rr / 2;
            if (yBelow === 0 || yBelow === gridSize) return false;
            return truthAt(x, yBelow, 0);
          };
          const vWallTruth = (rr, cc) => {
            if (cc < 0 || cc > 2 * gridSize || rr < 1 || rr > 2 * gridSize - 1) return false;
            const y = (rr - 1) / 2, xRight = cc / 2;
            if (xRight === 0 || xRight === gridSize) return false;
            return truthAt(xRight, y, 3);
          };
          // Cor do heatmap (g/y/r) a partir de uma distância e da média das células vizinhas.
          const bucketColor = (dd) =>
            dd === 0 || dd === 255 ? null : dd <= maxD / 3 ? "g" : dd <= 2 * maxD / 3 ? "y" : "r";
          const avgColor = (...cells) => {
            const ds = cells
              .filter(([xx, yy]) => expl(xx, yy))
              .map(([xx, yy]) => getDist(xx, yy))
              .filter((v) => v !== 0 && v !== 255);
            if (!ds.length) return null;
            return bucketColor(ds.reduce((a, b) => a + b, 0) / ds.length);
          };

          return (
            <div id="maze-container" className={showTruth ? "show-truth" : ""} style={{ '--n': gridSize, gridTemplateColumns: tmpl, gridTemplateRows: tmpl }}>
              {Array.from({ length: L * L }).map((_, i) => {
                const r = Math.floor(i / L), c = i % L;
                const evenR = r % 2 === 0, evenC = c % 2 === 0;

                // QUINA (poste): acende ao encostar numa parede conhecida; no Raio-X,
                // também quando encosta numa parede real (truth).
                if (evenR && evenC) {
                  const lit = hWallKnown(r, c - 1) || hWallKnown(r, c + 1)
                    || vWallKnown(r - 1, c) || vWallKnown(r + 1, c);
                  const litTruth = !lit && showTruth && !isRealMode
                    && (hWallTruth(r, c - 1) || hWallTruth(r, c + 1)
                      || vWallTruth(r - 1, c) || vWallTruth(r + 1, c));
                  // Poste interno cercado por células exploradas → cor de explorado.
                  const ci = c / 2, cj = r / 2;
                  const allExpl = expl(ci - 1, cj - 1) && expl(ci, cj - 1)
                    && expl(ci - 1, cj) && expl(ci, cj);
                  const allGoal = GOALS.some(g => g.x === ci - 1 && g.y === cj - 1)
                    && GOALS.some(g => g.x === ci && g.y === cj - 1)
                    && GOALS.some(g => g.x === ci - 1 && g.y === cj)
                    && GOALS.some(g => g.x === ci && g.y === cj);
                  // Poste cercado pelas 4 células do centro (goal): tom suave derivado do
                  // próprio verde do goal (is-goal-soft) pra casar com as divisórias.
                  const goalCorner = allGoal;
                  const cColor = !lit && !litTruth && allExpl
                    ? avgColor([ci - 1, cj - 1], [ci, cj - 1], [ci - 1, cj], [ci, cj])
                    : null;
                  let cornerCls = "lattice-corner";
                  if (goalCorner) cornerCls += " is-goal-soft";
                  else if (lit) cornerCls += " is-known";
                  else if (litTruth) cornerCls += " is-truth";
                  else if (allExpl) cornerCls += " is-explored";
                  return <div key={i} className={cornerCls} data-color={cColor} />;
                }

                // PAREDE HORIZONTAL: separa a célula de cima e a de baixo.
                if (evenR && !evenC) {
                  const x = (c - 1) / 2;
                  const yBelow = r / 2;
                  const yAbove = yBelow - 1;
                  const outer = yBelow === 0 || yBelow === gridSize;
                  const known = outer
                    ? true
                    : wallAt(x, yBelow, 0) && (expl(x, yBelow) || expl(x, yAbove));
                  const truth = !outer && showTruth && !isRealMode && truthAt(x, yBelow, 0);
                  // Abertura que o carro atravessou (sem parede, entre células exploradas).
                  const openExplored = !known && !truth && expl(x, yBelow) && expl(x, yAbove);
                  const isGoal = GOALS.some(g => g.x === x && g.y === yBelow) && GOALS.some(g => g.x === x && g.y === yAbove);
                  // Divisória entre duas células do centro (goal): tom suave derivado do
                  // próprio verde do goal (is-goal-soft) pra um bloco contínuo e leve.
                  const goalDivider = isGoal;
                  const wColor = openExplored ? avgColor([x, yAbove], [x, yBelow]) : null;
                  let cls = "lattice-wall lattice-wall-h";
                  if (goalDivider) cls += " is-goal-soft";
                  else if (known) cls += " is-known";
                  else if (truth) cls += " is-truth";
                  else if (openExplored) cls += " is-explored";
                  return <div key={i} className={cls} data-color={wColor} />;
                }

                // PAREDE VERTICAL: separa a célula da esquerda e a da direita.
                if (!evenR && evenC) {
                  const y = (r - 1) / 2;
                  const xRight = c / 2;
                  const xLeft = xRight - 1;
                  const outer = xRight === 0 || xRight === gridSize;
                  const known = outer
                    ? true
                    : wallAt(xRight, y, 3) && (expl(xRight, y) || expl(xLeft, y));
                  const truth = !outer && showTruth && !isRealMode && truthAt(xRight, y, 3);
                  // Abertura que o carro atravessou (sem parede, entre células exploradas).
                  const openExplored = !known && !truth && expl(xRight, y) && expl(xLeft, y);
                  const isGoal = GOALS.some(g => g.x === xRight && g.y === y) && GOALS.some(g => g.x === xLeft && g.y === y);
                  // Divisória entre duas células do centro (goal): tom suave derivado do
                  // próprio verde do goal (is-goal-soft) pra um bloco contínuo e leve.
                  const goalDivider = isGoal;
                  const wColor = openExplored ? avgColor([xLeft, y], [xRight, y]) : null;
                  let cls = "lattice-wall lattice-wall-v";
                  if (goalDivider) cls += " is-goal-soft";
                  else if (known) cls += " is-known";
                  else if (truth) cls += " is-truth";
                  else if (openExplored) cls += " is-explored";
                  return <div key={i} className={cls} data-color={wColor} />;
                }

                // PISO: a célula real do labirinto (mantém toda a lógica e cores).
                const x = (c - 1) / 2, y = (r - 1) / 2;
                let classes = ["cell"];
                const cellExplored = expl(x, y);
                if (cellExplored) classes.push("explored");
                const isGoal = GOALS.some(g => g.x === x && g.y === y);
                if (isGoal) classes.push("goal");
                const d = getDist(x, y);
                let dataColor = null;
                if (cellExplored && d !== 0 && d !== 255)
                  dataColor = d <= maxD / 3 ? "g" : d <= 2 * maxD / 3 ? "y" : "r";
                const hasRobot = robotShown && robotShown.x === x && robotShown.y === y;
                return (
                  <div key={i} className={classes.join(" ")} data-color={dataColor}>
                    {hasRobot && <div id="robot" className={`dir-${robotShown.dir}`}></div>}
                    {isGoal && !hasRobot && "G"}
                    {!isGoal && !hasRobot && cellExplored && d !== 255 ? d : ""}
                  </div>
                );
              })}
            </div>
          );
        })()}
      </div>
    </>
  );
};

/* ============================================================
   PAINEL DE TELEMETRIA
   ============================================================ */
const TelemetrySidebar = ({ sim, wsStatus, batteryPct, latencyMs, packetsRx, liveTelemetry, dataMode, runStatus, esp32Online }) => {
  const mem = sim.memory;
  const statusText = runStatus ?? mem.status;
  const isReal = dataMode === 'real';

  let estadoClass = 'text-brand-h2 bg-app-raised border-border-rule';
  if (statusText === 'Mapeando...' || statusText === 'Explorando' || statusText === 'Voltando') {
    estadoClass = 'text-white bg-brand-purple border-transparent';
  } else if (statusText === 'Centro Alcançado!' || statusText === 'Objetivo localizado!') {
    estadoClass = 'text-white bg-brand-green border-transparent';
  } else if (statusText === 'Preso!' || statusText === 'Erro!') {
    estadoClass = 'text-white bg-brand-danger border-transparent';
  }

  const timeSec = (dataMode === 'real' && liveTelemetry?.elapsed_time_ms != null)
    ? (liveTelemetry.elapsed_time_ms / 1000).toFixed(1)
    : (mem.timeMs / 1000).toFixed(1);
  const avgSpeed = (dataMode === 'real' && liveTelemetry?.speed_mm_s != null)
    ? (liveTelemetry.speed_mm_s / 10).toFixed(1)
    : (mem.timeMs > 0 ? ((mem.steps * 18) / (mem.timeMs / 1000)).toFixed(1) : "0.0");
  const stepsDisplay = (dataMode === 'real' && liveTelemetry?.step_count != null)
    ? liveTelemetry.step_count
    : mem.steps;

  const explored = mem.explored;
  let coveredCount = 0;
  if (explored) {
    for (let x = 0; x < sim.gridSize; x++)
      for (let y = 0; y < sim.gridSize; y++)
        if (explored[x]?.[y]) coveredCount++;
  }
  const totalCells = sim.gridSize * sim.gridSize;
  const coveragePct = Math.round((coveredCount / totalCells) * 100);

  let phaseText = 'Espera';
  if (statusText === 'Mapeando...' || statusText === 'Explorando') phaseText = 'Busca';
  else if (statusText === 'Centro Alcançado!' || statusText === 'Objetivo localizado!') phaseText = 'Resolvido';

  const isConnected = wsStatus === 'Conectado';
  const latencyThreshold = parseInt(localStorage.getItem('LATENCY_THRESHOLD') || '500', 10);
  const latencyOver = latencyMs != null && latencyMs > latencyThreshold;
  const batteryDisplay = batteryPct != null ? `${batteryPct}%` : '—';
  const batteryWidth = batteryPct != null ? Math.max(0, Math.min(100, batteryPct)) : 0;

  const robotX = mem.robot?.x ?? 0;
  const robotY = mem.robot?.y ?? 0;
  const distCenter = mem.distances?.[robotX]?.[robotY];
  const distDisplay = distCenter != null && distCenter !== 255 ? distCenter : '—';

  return (
    <>
      <div className="flex flex-col">
        <div className="flex items-center gap-2 px-1 pb-1">
          <h3 className="text-label">Status do Robô</h3>
          <span className="flex-1 h-px bg-border-rule" />
        </div>
        <div className="bg-panel overflow-hidden">
          <ConnRow
            icon={<Play size={16} />}
            label="Estado"
            value={<span className={`inline-flex items-center justify-center h-6 w-[130px] rounded-md border text-[11px] font-semibold leading-none ${estadoClass}`}>{statusText}</span>}
          />
          <ConnRow
            icon={<Wifi size={16} />}
            label="Servidor"
            value={
              <span className={`inline-flex items-center justify-center h-6 w-[130px] rounded-md border text-[11px] font-semibold leading-none ${isConnected ? 'text-white bg-brand-green border-transparent' : 'text-white bg-brand-danger border-transparent'}`}>
                {isConnected ? 'Conectado' : 'Desconectado'}
              </span>
            }
          />
          <ConnRow
            icon={<Bot size={16} />}
            label="Robô"
            value={
              <span className={`inline-flex items-center justify-center h-6 w-[130px] rounded-md border text-[11px] font-semibold leading-none ${esp32Online ? 'text-white bg-brand-green border-transparent' : 'text-white bg-brand-danger border-transparent'}`}>
                {esp32Online ? 'Online' : 'Offline'}
              </span>
            }
          />
          <ConnRow
            icon={<Bot size={16} />}
            label="Modo"
            value={
              <span className={`inline-flex items-center justify-center h-6 w-[130px] rounded-md border text-[11px] font-semibold leading-none ${isReal ? 'text-white bg-brand-green border-transparent' : 'text-white bg-brand-purple border-transparent'}`}>
                {isReal ? 'Corrida' : 'Simulador'}
              </span>
            }
          />
          <ConnRow
            icon={<Clock size={16} />}
            label="Latência"
            value={
              <span className={`font-mono font-semibold text-sm ${latencyOver ? 'text-brand-danger-text' : 'text-brand-h1'}`}>
                {latencyMs != null ? latencyMs : '—'} <span className="text-brand-h3 text-xs">ms</span>
              </span>
            }
          />
          <ConnRow
            icon={<Activity size={16} />}
            label="Pacotes"
            value={<span className="font-mono font-semibold text-sm text-brand-h1">{packetsRx}</span>}
          />
          <ConnRow
            icon={<Battery size={16} />}
            label="Bateria"
            last
            value={
              <span className="flex items-center gap-2 font-mono font-semibold text-sm text-brand-h1">
                <span className="w-10 h-2 rounded-full bg-app-inset overflow-hidden border border-border-rule">
                  <span className="block h-full rounded-full" style={{ width: `${batteryWidth}%`, backgroundColor: 'var(--success)' }} />
                </span>
                {batteryDisplay}
              </span>
            }
          />
        </div>
      </div>

      <div className="flex flex-col">
        <div className="flex items-center gap-2 px-1 pb-1">
          <h3 className="text-label">Telemetria</h3>
          <span className="flex-1 h-px bg-border-rule" />
        </div>
        <div className="grid grid-cols-2 gap-3">
          <MetricCard label="Velocidade" value={avgSpeed} unit="cm/s" icon={<Gauge size={16} />} accent />
          <MetricCard label="Tempo" value={timeSec} unit="s" icon={<Timer size={16} />} />
          <MetricCard label="Passos" value={stepsDisplay} unit="" icon={<Footprints size={16} />} />
          <MetricCard label="Giros" value={mem.turns} unit="" icon={<RefreshCw size={16} />} />
        </div>
      </div>

      <div className="flex flex-col">
        <div className="flex items-center gap-2 px-1 pb-1">
          <h3 className="text-label">Algoritmo</h3>
          <span className="flex-1 h-px bg-border-rule" />
        </div>
        <div className="bg-panel p-3 flex flex-col gap-2">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-lg grid place-items-center text-white flex-none bg-brand-purple">
              <Zap size={16} />
            </div>
            <div>
              <div className="text-base font-bold tracking-tight text-brand-h1">Flood-Fill</div>
              <div className="text-[11px] text-brand-h3 uppercase tracking-wider font-semibold mt-0.5">Resolução por inundação</div>
            </div>
          </div>
          <div className="flex gap-2">
            <AlgoStat label="Fase" value={phaseText} />
            <AlgoStat label="Dist. centro" value={distDisplay} />
            <AlgoStat label="Cobertura" value={`${coveragePct}%`} />
          </div>
        </div>
      </div>
    </>
  );
};

const ConnRow = ({ icon, label, value, last = false }) => (
  <div className={`flex items-center justify-between h-11 px-3 ${last ? '' : 'border-b border-border-rule'}`}>
    <div className="flex items-center gap-3">
      <div className="w-7 h-7 rounded-lg grid place-items-center bg-app-raised border border-border-rule text-brand-h2 flex-none">{icon}</div>
      <span className="text-brand-h2 text-sm font-medium">{label}</span>
    </div>
    <div className="ml-auto">{value}</div>
  </div>
);

const MetricCard = ({ label, value, unit, icon, accent = false }) => (
  <div className="bg-panel p-3 rounded-xl transition-all hover:border-border-accent hover:bg-app-hover group">
    <div className="flex items-center justify-between">
      <span className="text-label">{label}</span>
      <span className="text-brand-h3 group-hover:text-brand-h2 transition-colors">{icon}</span>
    </div>
    <div className={`mt-1.5 font-mono text-2xl font-bold tracking-tight leading-none tabular-nums ${accent ? 'text-brand-purple-light' : 'text-brand-h1'}`}>
      {value}
      {unit && <span className="text-sm text-brand-h2 font-medium font-sans ml-1">{unit}</span>}
    </div>
  </div>
);

const AlgoStat = ({ label, value }) => (
  <div className="flex-1 bg-app-inset border border-border-rule rounded-lg p-2">
    <div className="text-label">{label}</div>
    <div className="font-mono text-base font-semibold text-brand-h1 mt-1 tabular-nums">{value}</div>
  </div>
);

/* ============================================================
   VISÃO DE HISTÓRICO
   ============================================================ */
const RankingPanel = ({ runs, onSelectRun }) => {
  const [sourceFilter, setSourceFilter] = useState('Todos');

  const ranking = useMemo(() => {
    return runs
      .filter(r => r.status === 'Centro Alcançado!')
      .filter(r => sourceFilter === 'Todos' || r.source === (sourceFilter === 'Físico' ? 'real' : 'simulator'))
      .sort((a, b) => parseTimeToSeconds(a.time) - parseTimeToSeconds(b.time));
  }, [runs, sourceFilter]);

  const getPodiumStyle = (idx) => {
    if (idx === 0) return 'border-l-2 border-l-yellow-400 bg-yellow-500/10 text-yellow-400';
    if (idx === 1) return 'border-l-2 border-l-slate-400 bg-slate-400/10 text-slate-300';
    if (idx === 2) return 'border-l-2 border-l-amber-600 bg-amber-600/10 text-amber-500';
    return 'border-l-2 border-l-transparent text-brand-h3 hover:bg-app-hover';
  };

  const getPodiumIcon = (idx) => {
    if (idx === 0) return <div className="w-3 h-3 rounded-full bg-yellow-400/80" />;
    if (idx === 1) return <div className="w-3 h-3 rounded-full bg-slate-400/80" />;
    if (idx === 2) return <div className="w-3 h-3 rounded-full bg-amber-600/80" />;
    return <span className="font-bold text-xs opacity-50 text-brand-h3">{idx + 1}</span>;
  };

  return (
    <div className="mb-6">
      <div className="flex justify-between items-center mb-4">
        <h3 className="text-sm font-semibold text-brand-h2 uppercase tracking-wider">Ranking Geral</h3>
        <div className="pill-container">
          {['Todos', 'Físico', 'Simulador'].map(f => (
            <button key={f} onClick={() => setSourceFilter(f)}
              className={`pill-item font-medium text-xs transition-colors ${sourceFilter === f ? 'bg-brand-purple text-white' : 'text-brand-h3 hover:text-brand-h1 hover:bg-white/[0.03]'}`}>
              {f}
            </button>
          ))}
        </div>
      </div>
      <div className="overflow-hidden rounded-xl border border-border-rule bg-app-bg shadow-card">
        <table className="w-full text-left border-collapse whitespace-nowrap">
          <thead className="bg-app-surface border-b border-border-rule">
            <tr>
              <th className="p-4 text-label w-16 text-center">Posição</th>
              <th className="p-4 text-label">Tipo</th>
              <th className="p-4 text-label">Labirinto</th>
              <th className="p-4 text-label">Tempo</th>
              <th className="p-4 text-label">Velocidade</th>
              <th className="p-4 text-label">Bateria</th>
              <th className="p-4 text-label">Movimentos</th>
              <th className="p-4 text-label">Status</th>
              <th className="p-4 text-label w-10 text-center"></th>
            </tr>
          </thead>
          <tbody className="divide-y divide-border-rule">
            {ranking.length > 0 ? ranking.map((run, idx) => (
              <tr key={run.id} onClick={() => onSelectRun(run)} className={`transition-colors cursor-pointer group ${getPodiumStyle(idx)}`}>
                <td className="p-4"><div className="flex justify-center items-center h-full">{getPodiumIcon(idx)}</div></td>
                <td className="p-4">
                  {run.source === 'simulator' ? (
                    <span className="badge w-[100px] bg-brand-purple text-white" title="Corrida gerada no simulador local"><Cpu size={14} /><span>Simulada</span></span>
                  ) : (
                    <span className="badge w-[100px] bg-brand-green text-white" title="Corrida do robô físico"><Bot size={14} /><span>Corrida</span></span>
                  )}
                </td>
                <td className="p-4 text-brand-h2 font-medium text-sm"><div className="bg-app-raised inline-block px-3 py-1 rounded-lg border border-border-rule font-mono">{run.maze}</div></td>
                <td className={`p-4 font-mono text-sm h-14 ${idx === 0 ? 'text-yellow-400 font-bold' : 'text-brand-h1'}`}>
                  <div className="flex items-center gap-2 h-full"><span>{run.time}</span></div>
                </td>
                <td className="p-4 text-brand-h2 text-sm">{run.speed}</td>
                <td className="p-4"><div className="flex items-center gap-2"><Battery size={16} className="text-brand-green" /><span className="text-brand-h1 text-sm">{run.battery}</span></div></td>
                <td className="p-4"><div className="flex items-center gap-2"><Footprints size={16} className="text-brand-purple-light" /><span className="text-brand-h1 text-sm">{run.steps}</span></div></td>
                <td className="p-4">
                  <div className="inline-flex items-center justify-center h-6 px-3 rounded-md border text-[11px] font-semibold leading-none gap-1.5 w-max text-white bg-brand-green border-transparent">
                    <CheckCircle2 size={14} /><span>{run.status}</span>
                  </div>
                </td>
                <td className="p-4 text-right">
                  <div className="text-brand-h3 group-hover:text-brand-h1 transition-colors flex items-center justify-end">
                    <span className="text-xs mr-1 opacity-0 group-hover:opacity-100 transition-opacity">Replay</span>
                    <ChevronRight size={16} />
                  </div>
                </td>
              </tr>
            )) : (
              <tr><td colSpan="8" data-testid="estado-vazio" className="p-12 text-center text-brand-h3">Nenhuma corrida concluída encontrada no ranking.</td></tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};

const HistoryView = ({ historyData, filter, setFilter, loading, error, onRefresh }) => {
  const [subTab, setSubTab] = useState('tabela');
  const [selectedRun, setSelectedRun] = useState(null);
  const [replayPath, setReplayPath] = useState(null);
  const [replayLoading, setReplayLoading] = useState(false);
  const [replayError, setReplayError] = useState(null);

  useEffect(() => {
    setReplayPath(null);
    setReplayError(null);
    if (!selectedRun) return;
    if (selectedRun.mapSnapshot) return;
    const rawPath = selectedRun.raw?.path_traversed;
    if (rawPath && rawPath.length > 0) { setReplayPath(rawPath); return; }
    const numericId = parseInt(String(selectedRun.id).replace(/^db-/, ''), 10);
    if (!Number.isFinite(numericId)) return;
    setReplayLoading(true);
    getCorrida(numericId)
      .then(data => setReplayPath(data?.path_traversed ?? []))
      .catch(err => setReplayError(err.message || 'Erro ao carregar replay'))
      .finally(() => setReplayLoading(false));
  }, [selectedRun]);

  const filteredHistory = historyData;
  const successfulRuns = filteredHistory.filter(r => r.status === 'Centro Alcançado!');
  const bestRun = successfulRuns.length > 0 ? successfulRuns.reduce((prev, curr) => {
    const parseTime = (t) => { const parts = t.split(' '); return parseInt(parts[0]) * 60 + parseFloat(parts[1]); };
    return parseTime(curr.time) < parseTime(prev.time) ? curr : prev;
  }) : null;

  const totalRuns = filteredHistory.length;
  const successRate = totalRuns > 0 ? Math.round((successfulRuns.length / totalRuns) * 100) : 0;
  const bestTimeStr = bestRun ? bestRun.time : '--';
  const avgSpeed = totalRuns > 0 ? (filteredHistory.reduce((acc, curr) => acc + parseFloat(curr.speed), 0) / totalRuns).toFixed(1) + ' cm/s' : '--';

  const exportCSV = () => {
    const headers = ['Data/Hora', 'Tipo', 'Labirinto', 'Status', 'Tempo', 'Velocidade', 'Bateria', 'Movimentos'];
    const tipoLabel = (s) => (s === 'simulator' ? 'Simulada' : 'Real');
    const rows = filteredHistory.map(run => [run.date, tipoLabel(run.source), run.maze, run.status, run.time, run.speed, run.battery, run.steps]);
    const csvContent = "data:text/csv;charset=utf-8," + [headers.join(','), ...rows.map(e => e.join(','))].join('\n');
    const link = document.createElement("a");
    link.setAttribute("href", encodeURI(csvContent));
    link.setAttribute("download", `historico_micromouse_${filter}.csv`);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  };

  return (
    <div data-testid="historico-view" className="flex-1 bg-app-surface w-full h-full overflow-y-auto p-6 box-border">
      <div className="flex justify-between items-center mb-6">
        <div data-testid="pill-container" className="pill-container">
          {['tabela', 'ranking'].map(t => (
            <button key={t} data-testid="pill-item" onClick={() => setSubTab(t)}
              className={`group pill-item font-medium text-sm transition-all ${subTab === t ? 'text-white bg-brand-purple' : 'text-brand-h3 hover:text-brand-h1 hover:bg-white/[0.03]'}`}>
              {t.charAt(0).toUpperCase() + t.slice(1)}
            </button>
          ))}
        </div>
        <div data-testid="pill-container" className="pill-container">
          <CustomSelect
            data-testid="filtro-labirinto"
            className="h-[var(--pill-h)] bg-app-raised text-brand-h1 px-4 rounded-[16px] font-medium text-sm border border-border-rule hover:bg-app-hover hover:border-border-accent transition-all min-w-[180px]"
            value={filter}
            onChange={(val) => setFilter(val)}
            options={[{ value: 'Todos', label: 'Todos os Labirintos' }, { value: '4x4', label: 'Pista 4x4' }, { value: '8x8', label: 'Pista 8x8' }, { value: '16x16', label: 'Pista 16x16' }]}
          />
          <button data-testid="pill-item" onClick={onRefresh} title="Recarregar do backend"
            className="group pill-item gap-2 bg-app-raised border border-border-rule hover:border-border-accent text-brand-h1 font-medium transition-all text-sm w-[130px]">
            <RefreshCw size={16} className={`transition-transform duration-300 group-hover:rotate-180 group-active:scale-90 ${loading ? 'animate-spin' : ''}`} /><span>{loading ? 'Carregando…' : 'Atualizar'}</span>
          </button>
          {subTab === 'tabela' && (
            <button data-testid="pill-item" onClick={exportCSV} className="group pill-item gap-2 text-white font-medium transition-all text-sm border border-transparent bg-brand-purple">
              <Download size={16} className="transition-transform group-hover:-translate-y-0.5 group-active:scale-90" /><span>Exportar CSV</span>
            </button>
          )}
        </div>
      </div>

      {error && (
        <div className="mb-4 bg-red-500/10 border border-red-500/30 text-red-400 text-sm px-4 py-3 rounded-xl">
          Falha ao carregar histórico do backend: {error}. Mostrando apenas corridas do simulador.
        </div>
      )}

      {subTab === 'tabela' && (
        <div className="flex items-center flex-wrap gap-x-6 gap-y-2 bg-app-bg border border-border-rule rounded-xl px-5 py-2 mb-3 text-sm shadow-card">
          <div className="flex items-center gap-2"><span className="text-label">Corridas</span><span className="text-brand-h1 font-bold">{totalRuns}</span></div>
          <div className="w-px h-4 bg-border-rule" />
          <div className="flex items-center gap-2"><span className="text-label">Sucesso</span><span className="text-brand-green font-bold">{successRate}%</span></div>
          <div className="w-px h-4 bg-border-rule" />
          <div className="flex items-center gap-2"><span className="text-label">Melhor Tempo</span><span className="text-brand-purple-light font-bold font-mono">{bestTimeStr}</span></div>
          <div className="w-px h-4 bg-border-rule" />
          <div className="flex items-center gap-2"><span className="text-label">Velocidade Média</span><span className="text-brand-h1 font-bold">{avgSpeed}</span></div>
        </div>
      )}

      {subTab === 'ranking' && <RankingPanel runs={historyData} onSelectRun={setSelectedRun} />}

      {subTab === 'tabela' && (
        <div className="overflow-hidden rounded-xl border border-border-rule bg-app-bg shadow-card">
          <table className="w-full text-left border-collapse whitespace-nowrap">
            <thead className="bg-app-surface border-b border-border-rule">
              <tr>
                <th className="p-4 text-label">Data / Hora</th>
                <th className="p-4 text-label">Tipo</th>
                <th className="p-4 text-label">Labirinto</th>
                <th className="p-4 text-label">Tempo</th>
                <th className="p-4 text-label">Velocidade</th>
                <th className="p-4 text-label">Bateria</th>
                <th className="p-4 text-label">Movimentos</th>
                <th className="p-4 text-label">Status</th>
                <th className="p-4 text-label w-10 text-center"></th>
              </tr>
            </thead>
            <tbody className="divide-y divide-border-rule">
              {filteredHistory.length > 0 ? filteredHistory.map((run) => (
                <tr key={run.id} data-testid="corrida-item" onClick={() => setSelectedRun(run)} className="hover:bg-app-hover transition-colors cursor-pointer group">
                  <td className="p-4"><div className="flex items-center gap-2"><Calendar size={16} className="text-brand-h3 group-hover:text-brand-h2 transition-colors" /><span className="text-brand-h1 font-medium text-sm">{run.date}</span></div></td>
                  <td className="p-4">
                    {run.source === 'simulator' ? (
                      <span className="badge w-[100px] bg-brand-purple text-white" title="Corrida gerada no simulador local"><Cpu size={14} /><span>Simulada</span></span>
                    ) : (
                      <span className="badge w-[100px] bg-brand-green text-white" title="Corrida do robô físico"><Bot size={14} /><span>Corrida</span></span>
                    )}
                  </td>
                  <td className="p-4 text-brand-h2 font-medium text-sm"><div className="bg-app-raised inline-block px-3 py-1 rounded-lg border border-border-rule font-mono">{run.maze}</div></td>
                  <td className="p-4 text-brand-h1 font-mono text-sm h-14"><div className="flex items-center gap-2 h-full"><span>{run.time}</span></div></td>
                  <td className="p-4 text-brand-h2 text-sm">{run.speed}</td>
                  <td className="p-4"><div className="flex items-center gap-2"><Battery size={16} className="text-brand-green" /><span className="text-brand-h1 text-sm">{run.battery}</span></div></td>
                  <td className="p-4"><div className="flex items-center gap-2"><Footprints size={16} className="text-brand-purple-light" /><span className="text-brand-h1 text-sm">{run.steps}</span></div></td>
                  <td className="p-4">
                    <div className={`inline-flex items-center justify-center h-6 px-3 rounded-md border text-[11px] font-semibold leading-none gap-1.5 w-max ${run.status === 'Centro Alcançado!' ? 'text-white bg-brand-green border-transparent' : 'text-white bg-brand-danger border-transparent'}`}>
                      {run.status === 'Centro Alcançado!' ? <CheckCircle2 size={14} /> : <XCircle size={14} />}
                      <span>{run.status}</span>
                    </div>
                  </td>
                  <td className="p-4 text-right">
                    <div className="text-brand-h3 group-hover:text-brand-h1 transition-colors flex items-center justify-end">
                      <span className="text-xs mr-1 opacity-0 group-hover:opacity-100 transition-opacity">Replay</span>
                      <ChevronRight size={16} />
                    </div>
                  </td>
                </tr>
              )) : (
                <tr><td colSpan="8" data-testid="estado-vazio" className="p-12 text-center text-brand-h3">Nenhuma corrida encontrada para o filtro selecionado.</td></tr>
              )}
            </tbody>
          </table>
        </div>
      )}

      {selectedRun && (
        <div className="absolute inset-0 z-50 bg-app-bg/80 backdrop-blur-sm flex items-center justify-center p-6" onClick={() => setSelectedRun(null)}>
          <div className="bg-panel w-full max-w-2xl shadow-pop flex flex-col p-6 rounded-xl" onClick={e => e.stopPropagation()}>
            <div className="flex justify-between items-start mb-6">
              <div>
                <h3 className="text-2xl font-bold text-brand-h1 mb-1">Detalhes da Corrida</h3>
                <p className="text-brand-h3 text-sm">{selectedRun.date}</p>
              </div>
              <button onClick={() => setSelectedRun(null)} className="text-brand-h3 hover:text-white transition-colors"><XCircle size={24} /></button>
            </div>
            <div className="grid grid-cols-2 gap-6">
              <div className="space-y-4">
                <div className="bg-app-bg p-4 rounded-xl border border-border-rule flex justify-between items-center">
                  <span className="text-brand-h3 text-sm">Tipo</span>
                  {selectedRun.source === 'simulator' ? (
                    <span className="badge bg-brand-purple/20 text-brand-accent border border-brand-purple/40"><Cpu size={12} /><span>Simulada</span></span>
                  ) : (
                    <span className="badge bg-brand-green/15 text-brand-green-text border border-brand-green/30"><Bot size={12} /><span>Real</span></span>
                  )}
                </div>
                <div className="bg-app-bg p-4 rounded-xl border border-border-rule flex justify-between items-center">
                  <span className="text-brand-h3 text-sm">Tempo</span>
                  <span className="text-brand-h1 font-mono font-bold">{selectedRun.time}</span>
                </div>
                <div className="bg-app-bg p-4 rounded-xl border border-border-rule flex justify-between items-center">
                  <span className="text-brand-h3 text-sm">Labirinto</span>
                  <span className="text-brand-h1 font-bold font-mono">{selectedRun.maze}</span>
                </div>
                <div className="bg-app-bg p-4 rounded-xl border border-border-rule flex justify-between items-center">
                  <span className="text-brand-h3 text-sm">Velocidade Média</span>
                  <span className="text-brand-h1 font-bold">{selectedRun.speed}</span>
                </div>
                <div className="bg-app-bg p-4 rounded-xl border border-border-rule flex justify-between items-center">
                  <span className="text-brand-h3 text-sm">Bateria Restante</span>
                  <span className="text-brand-green font-bold">{selectedRun.battery}</span>
                </div>
              </div>
              {selectedRun.mapSnapshot ? (
                <MiniMap snapshot={selectedRun.mapSnapshot} />
              ) : replayLoading ? (
                <div className="bg-app-bg rounded-xl border border-border-rule flex items-center justify-center p-4 h-full text-brand-h3 text-sm">Carregando replay…</div>
              ) : replayError ? (
                <div className="bg-app-bg rounded-xl border border-red-500/30 flex items-center justify-center p-4 h-full text-red-400 text-sm text-center">{replayError}</div>
              ) : replayPath && replayPath.length > 0 ? (
                <ReplayCanvas pathMm={replayPath} mazeSize={parseInt(selectedRun.maze.split('x')[0], 10)} knownWalls={selectedRun.knownWalls ?? selectedRun.raw?.known_walls ?? null} />
              ) : (
                <div className="bg-app-bg rounded-xl border border-border-rule flex items-center justify-center p-4 h-full text-brand-h3 text-xs text-center">Trajeto indisponível para esta corrida.</div>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default App;
