import React, { useState, useEffect, useRef } from 'react';
import { Bluetooth, Play, Square, Send, Compass, Crosshair, Navigation, Activity } from 'lucide-react';

export default function BluetoothPanel() {
  const [isConnected, setIsConnected] = useState(false);
  const [port, setPort] = useState(null);
  const [command, setCommand] = useState('');
  
  const [tofLeft, setTofLeft] = useState(0);
  const [tofCenter, setTofCenter] = useState(0);
  const [tofRight, setTofRight] = useState(0);

  const [accelX, setAccelX] = useState(0);
  const [accelY, setAccelY] = useState(0);
  const [accelZ, setAccelZ] = useState(0);
  const [angle, setAngle] = useState(0);

  const readerRef = useRef(null);

  // Fallback demo simulation if not connected
  useEffect(() => {
    let interval;
    if (!isConnected) {
      interval = setInterval(() => {
        setTofLeft(Math.floor(Math.random() * 2000));
        setTofCenter(Math.floor(Math.random() * 2000));
        setTofRight(Math.floor(Math.random() * 2000));
        setAccelX(Math.random() * 2 - 1);
        setAccelY(Math.random() * 2 - 1);
        setAccelZ(9.8 + (Math.random() * 0.5 - 0.25));
        setAngle((Date.now() / 50) % 360);
      }, 500);
    }
    return () => clearInterval(interval);
  }, [isConnected]);

  const processData = (data) => {
    try {
      if (data.startsWith('{')) {
        const parsed = JSON.parse(data);
        if (parsed.tof) {
          setTofLeft(parsed.tof[0]); setTofCenter(parsed.tof[1]); setTofRight(parsed.tof[2]);
        }
        if (parsed.accel) {
          setAccelX(parsed.accel[0]); setAccelY(parsed.accel[1]); setAccelZ(parsed.accel[2]);
        }
        if (parsed.angle !== undefined) setAngle(parsed.angle);
      } else {
        const parts = data.split(';');
        parts.forEach(part => {
          if (part.startsWith('T:')) {
            const vals = part.substring(2).split(',').map(Number);
            if (vals.length >= 3) { setTofLeft(vals[0]); setTofCenter(vals[1]); setTofRight(vals[2]); }
          } else if (part.startsWith('A:')) {
            const vals = part.substring(2).split(',').map(Number);
            if (vals.length >= 3) { setAccelX(vals[0]); setAccelY(vals[1]); setAccelZ(vals[2]); }
          } else if (part.startsWith('G:')) {
            setAngle(Number(part.substring(2)));
          }
        });
      }
    } catch (e) {
      console.warn("Parse error:", data);
    }
  };

  const readLoop = async (reader) => {
    let buffer = '';
    while (true) {
      try {
        const { value, done } = await reader.read();
        if (value) {
          buffer += value;
          let lines = buffer.split('\n');
          buffer = lines.pop(); 
          for (let line of lines) processData(line.trim());
        }
        if (done) break;
      } catch (err) {
        console.error("Read error", err);
        break;
      }
    }
  };

  const handleConnect = async () => {
    if (isConnected) {
      if (readerRef.current) await readerRef.current.cancel();
      if (port) await port.close();
      setIsConnected(false);
      setPort(null);
    } else {
      try {
        if (!navigator.serial) {
          alert("Web Serial API não suportada. Use Chrome ou Edge.");
          return;
        }
        const p = await navigator.serial.requestPort();
        await p.open({ baudRate: 115200 });
        setPort(p);
        setIsConnected(true);

        const textDecoder = new window.TextDecoderStream();
        p.readable.pipeTo(textDecoder.writable);
        const reader = textDecoder.readable.getReader();
        readerRef.current = reader;
        readLoop(reader);
      } catch (e) {
        console.error(e);
        alert("Erro ao conectar.");
      }
    }
  };

  const sendCommand = async (cmd) => {
    if (!port || !port.writable) return;
    const textEncoder = new window.TextEncoderStream();
    const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
    const writer = textEncoder.writable.getWriter();
    await writer.write(cmd + '\n');
    writer.releaseLock();
  };

  const getBarColor = (dist) => dist < 200 ? 'bg-red-500' : 'bg-gradient-to-r from-emerald-500 to-blue-500';

  return (
    <div className="flex flex-col h-full overflow-y-auto p-6 bg-app-bg text-gray-100 gap-6">
      
      <div className="flex justify-between items-center bg-gray-800/40 p-4 rounded-xl border border-gray-700/50">
        <div>
          <h2 className="text-xl font-bold bg-gradient-to-r from-blue-400 to-purple-400 bg-clip-text text-transparent">Bluetooth Web Serial</h2>
          <p className="text-sm text-gray-400">Conexão direta com a placa e leitura de ToFs / Giroscópio</p>
        </div>
        <div className="flex items-center gap-4">
          <div className="flex items-center gap-2">
            <span className={`w-3 h-3 rounded-full ${isConnected ? 'bg-emerald-500 shadow-[0_0_10px_#10b981]' : 'bg-red-500 shadow-[0_0_10px_#ef4444]'}`}></span>
            <span className="text-sm text-gray-300">{isConnected ? 'Conectado' : 'Desconectado'}</span>
          </div>
          <button 
            onClick={handleConnect}
            className={`flex items-center gap-2 px-4 py-2 rounded-lg font-medium transition-all ${isConnected ? 'bg-red-500/20 text-red-400 hover:bg-red-500/30' : 'bg-blue-600 hover:bg-blue-500 text-white'}`}
          >
            <Bluetooth size={18} />
            {isConnected ? 'Desconectar' : 'Conectar Pareado'}
          </button>
        </div>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        
        {/* Controles */}
        <div className="bg-gray-800/50 p-5 rounded-2xl border border-gray-700/50 flex flex-col gap-4">
          <h3 className="font-semibold text-gray-200 flex items-center gap-2"><Navigation size={18} className="text-purple-400" /> Ações Rápidas</h3>
          <div className="flex gap-3">
            <button disabled={!isConnected} onClick={() => sendCommand('START')} className="flex-1 flex items-center justify-center gap-2 bg-emerald-600 hover:bg-emerald-500 disabled:opacity-50 disabled:cursor-not-allowed py-2 rounded-lg font-medium text-white transition-colors">
              <Play size={18} fill="currentColor" /> Iniciar
            </button>
            <button disabled={!isConnected} onClick={() => sendCommand('STOP')} className="flex-1 flex items-center justify-center gap-2 bg-red-600 hover:bg-red-500 disabled:opacity-50 disabled:cursor-not-allowed py-2 rounded-lg font-medium text-white transition-colors">
              <Square size={18} fill="currentColor" /> Parar
            </button>
          </div>
          <div className="mt-2">
            <label className="text-sm text-gray-400 mb-1 block">Terminal de Comando</label>
            <div className="flex gap-2">
              <input 
                type="text" 
                disabled={!isConnected}
                value={command}
                onChange={(e) => setCommand(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && (sendCommand(command), setCommand(''))}
                placeholder="Ex: SET_SPEED 50" 
                className="flex-1 bg-gray-900 border border-gray-700 rounded-lg px-3 py-2 text-sm focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 disabled:opacity-50 text-white"
              />
              <button 
                disabled={!isConnected}
                onClick={() => { sendCommand(command); setCommand(''); }}
                className="bg-blue-600 hover:bg-blue-500 disabled:opacity-50 disabled:cursor-not-allowed px-3 py-2 rounded-lg text-white transition-colors"
              >
                <Send size={18} />
              </button>
            </div>
          </div>
        </div>

        {/* Sensores ToF */}
        <div className="bg-gray-800/50 p-5 rounded-2xl border border-gray-700/50 flex flex-col gap-4">
          <h3 className="font-semibold text-gray-200 flex items-center gap-2"><Crosshair size={18} className="text-blue-400" /> Sensores ToF (Distância)</h3>
          
          {[
            { label: 'Esquerda', val: tofLeft },
            { label: 'Centro', val: tofCenter },
            { label: 'Direita', val: tofRight }
          ].map((s, i) => (
            <div key={i} className="bg-gray-900/50 p-3 rounded-lg border border-gray-800">
              <div className="flex justify-between items-end mb-2">
                <span className="text-sm text-gray-400">{s.label}</span>
                <div className="text-lg font-mono font-bold">{s.val} <span className="text-xs text-gray-500 font-sans font-normal">mm</span></div>
              </div>
              <div className="h-2 w-full bg-gray-800 rounded-full overflow-hidden">
                <div className={`h-full ${getBarColor(s.val)} transition-all duration-300 ease-out`} style={{ width: `${Math.min(100, (s.val / 2000) * 100)}%` }}></div>
              </div>
            </div>
          ))}
        </div>

        {/* Giroscópio */}
        <div className="bg-gray-800/50 p-5 rounded-2xl border border-gray-700/50 flex flex-col gap-4">
          <h3 className="font-semibold text-gray-200 flex items-center gap-2"><Activity size={18} className="text-emerald-400" /> Acelerômetro e Giroscópio</h3>
          
          <div className="flex justify-between gap-2">
             {[
               { axis: 'X', val: accelX, color: 'text-red-400', bg: 'bg-red-400/10 border-red-400/20' },
               { axis: 'Y', val: accelY, color: 'text-emerald-400', bg: 'bg-emerald-400/10 border-emerald-400/20' },
               { axis: 'Z', val: accelZ, color: 'text-blue-400', bg: 'bg-blue-400/10 border-blue-400/20' }
             ].map((a, i) => (
                <div key={i} className={`flex-1 flex flex-col items-center justify-center p-2 rounded-lg border ${a.bg}`}>
                  <span className={`text-xs font-bold ${a.color}`}>{a.axis}</span>
                  <span className="font-mono text-sm">{a.val.toFixed(2)}</span>
                  <span className="text-[10px] text-gray-500">m/s²</span>
                </div>
             ))}
          </div>

          <div className="mt-2 bg-gray-900/50 p-4 rounded-xl border border-gray-800 flex items-center justify-between">
            <div>
              <span className="text-sm text-gray-400 block mb-1">Ângulo (Yaw)</span>
              <div className="text-2xl font-mono font-bold text-white">{angle.toFixed(1)}°</div>
            </div>
            <div className="relative w-16 h-16 rounded-full border-2 border-emerald-500/50 flex items-center justify-center">
              <Compass size={24} className="text-gray-600 absolute" />
              <div 
                className="absolute w-1 h-8 bg-emerald-500 bottom-1/2 origin-bottom transition-transform duration-100"
                style={{ transform: `rotate(${angle}deg)` }}
              ></div>
              <div className="w-2 h-2 rounded-full bg-white z-10"></div>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
}
