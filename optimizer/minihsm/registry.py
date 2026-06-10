"""
Device Registry — guarda la IP y estado de cada miniHSM registrado.
El serverHSM usa este registro para saber dónde está cada dispositivo.
"""

import time
import threading
from dataclasses import dataclass, field
from typing import Optional

DEVICE_TIMEOUT_SEC = 600  # 10 min sin heartbeat = considerado offline


@dataclass
class DeviceEntry:
    device_id:   str
    ip:          str
    port:        int   = 80
    last_seen:   float = field(default_factory=time.time)
    firmware:    str   = ""
    registered:  bool  = True

    @property
    def is_online(self) -> bool:
        return (time.time() - self.last_seen) < DEVICE_TIMEOUT_SEC

    @property
    def base_url(self) -> str:
        return f"http://{self.ip}:{self.port}"

    @property
    def seconds_since_heartbeat(self) -> int:
        return int(time.time() - self.last_seen)


class DeviceRegistry:
    """
    Registro en memoria de dispositivos miniHSM activos.
    Thread-safe. En producción se puede persistir en Redis o DB.
    """

    def __init__(self):
        self._devices: dict[str, DeviceEntry] = {}
        self._lock = threading.Lock()

    def register(self, device_id: str, ip: str,
                 port: int = 80, firmware: str = "") -> DeviceEntry:
        with self._lock:
            existing = self._devices.get(device_id)
            if existing and existing.ip != ip:
                import logging
                logging.getLogger("registry").info(
                    f"Device {device_id}: IP changed {existing.ip} → {ip}"
                )
            entry = DeviceEntry(
                device_id = device_id,
                ip        = ip,
                port      = port,
                last_seen = time.time(),
                firmware  = firmware,
            )
            self._devices[device_id] = entry
            return entry

    def get(self, device_id: str) -> Optional[DeviceEntry]:
        with self._lock:
            return self._devices.get(device_id)

    def get_url(self, device_id: str) -> Optional[str]:
        entry = self.get(device_id)
        if entry and entry.is_online:
            return entry.base_url
        return None

    def all_devices(self) -> list[DeviceEntry]:
        with self._lock:
            return list(self._devices.values())

    def online_devices(self) -> list[DeviceEntry]:
        return [d for d in self.all_devices() if d.is_online]


# Instancia global
registry = DeviceRegistry()
