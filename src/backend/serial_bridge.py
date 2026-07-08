"""
serial_bridge.py — Ponte serial integrada ao backend.
conectar / desconectar / listar portas / status + injeção de
telemetria. Sem reconexão automática nem persistência da porta (a serial caindo
apenas marca `last_error` e o estado vira desconectado).
"""
import asyncio
import json
import logging
import threading
from datetime import datetime, timezone

import serial
from serial.tools import list_ports

logger = logging.getLogger(__name__)

BAUD_PADRAO = 921600
_READ_TIMEOUT = 0.5  # segundos


def _agora_iso():
    """Timestamp de chegada em ISO 8601 UTC com milissegundos (ex.: 2026-06-15T12:00:00.123Z)."""
    return (
        datetime.now(timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


class SerialBridge:
    def __init__(self):
        self._serial = None
        self._thread = None
        self._stop = threading.Event()
        self._lock = threading.RLock()
        self.port = None
        self.baud = BAUD_PADRAO
        self.packets_rx = 0
        self.last_packet_at = None
        self.last_error = None
        # Injetados pelo app no startup (ver app.configure_serial_bridge):
        self._loop = None
        self._handler = None  # async def handler(TelemetriaSchema) -> None

    # ---- configuração ----
    def configure(self, loop, handler):
        """Liga a ponte ao event loop do backend e ao handler de telemetria."""
        self._loop = loop
        self._handler = handler

    # ---- estado ----
    @property
    def connected(self):
        return self._serial is not None and getattr(self._serial, "is_open", False)

    def status(self):
        return {
            "connected": self.connected,
            "port": self.port,
            "baud": self.baud,
            "packets_rx": self.packets_rx,
            "last_packet_at": self.last_packet_at,
            "last_error": self.last_error,
        }

    @staticmethod
    def listar_portas():
        """Lista as COMs disponíveis, marcando as Bluetooth e a provável porta de saída."""
        portas = []
        for p in list_ports.comports():
            hwid = p.hwid or ""
            bluetooth = hwid.upper().startswith("BTHENUM")
            # Porta de SAÍDA do par Bluetooth costuma trazer o MAC do dispositivo
            # (diferente de zeros) no hwid; a de entrada vem zerada.
            mac_digits = "".join(c for c in hwid if c in "0123456789abcdefABCDEF")
            suggested = bool(bluetooth and mac_digits and set(mac_digits[-12:]) != {"0"})
            portas.append(
                {
                    "device": p.device,
                    "description": p.description or "",
                    "hwid": hwid,
                    "bluetooth": bluetooth,
                    "suggested": suggested,
                }
            )
        return portas

    # ---- conexão ----
    def connect(self, port, baud=BAUD_PADRAO):
        """Abre a serial e dispara a thread leitora. Levanta exceção se a porta não abrir."""
        with self._lock:
            if self.connected:
                self._disconnect_locked()
            try:
                self._serial = serial.Serial(port, baud, timeout=_READ_TIMEOUT)
            except Exception as exc:
                self._serial = None
                self.last_error = str(exc)
                logger.error(f"Falha ao abrir {port}: {exc}")
                raise
            self.port = port
            self.baud = baud
            self.last_error = None
            self._stop.clear()
            self._thread = threading.Thread(
                target=self._read_loop, args=(self._serial,), name="serial-bridge", daemon=True
            )
            self._thread.start()
            logger.info(f"Serial conectada: {port} @ {baud}")

    def disconnect(self):
        with self._lock:
            self._disconnect_locked()

    def _disconnect_locked(self):
        self._stop.set()
        s = self._serial
        self._serial = None
        if s is not None:
            try:
                s.close()
            except Exception:
                pass
        self.last_error = None
        logger.info("Serial desconectada")

    # ---- leitura ----
    def _read_loop(self, s):
        while not self._stop.is_set():
            try:
                raw = s.readline()
            except Exception as exc:
                self.last_error = f"Conexão perdida: {exc}"
                logger.warning(self.last_error)
                break
            if not raw:
                continue  # timeout, sem dados
            line = raw.decode("utf-8", errors="replace").strip()
            if line:
                self._handle_line(line)
        # Saída do loop: se não foi desconexão manual, a serial caiu — limpa o estado.
        if not self._stop.is_set():
            with self._lock:
                if self._serial is s:
                    try:
                        s.close()
                    except Exception:
                        pass
                    self._serial = None

    def _handle_line(self, line):
        try:
            data = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            logger.debug(f"[ESP32] {line}")  # linha não-JSON = debug do firmware
            return
        if not isinstance(data, dict):
            logger.debug(f"[ESP32] {line}")
            return

        data["timestamp"] = _agora_iso()  # carimba a chegada
        self.packets_rx += 1
        self.last_packet_at = data["timestamp"]
        self._inject(data)

    def _inject(self, data):
        if self._loop is None or self._handler is None:
            logger.warning("Pacote serial descartado: ponte não configurada (sem loop/handler).")
            return
        # Import tardio evita ciclo de import com app.py.
        from schemas.telemetria import TelemetriaSchema

        try:
            schema = TelemetriaSchema(**data)
        except Exception as exc:
            logger.warning(f"Pacote serial inválido (ignorado): {exc}")
            return
        fut = asyncio.run_coroutine_threadsafe(self._handler(schema), self._loop)
        try:
            fut.result(timeout=5)
        except Exception as exc:
            logger.error(f"Erro ao processar pacote serial: {exc}")

    # ---- comandos ----
    def write_command(self, cmd):
        """Escreve um comando (ex.: 'start 4', 'reset') na serial. Retorna True se enviado."""
        with self._lock:
            if not self.connected:
                return False
            try:
                self._serial.write((cmd + "\n").encode("utf-8"))
                self._serial.flush()
                logger.info(f"Comando enviado à serial: {cmd}")
                return True
            except Exception as exc:
                self.last_error = f"Falha ao escrever comando: {exc}"
                logger.error(self.last_error)
                return False


# Singleton usado pelo app.
bridge = SerialBridge()
