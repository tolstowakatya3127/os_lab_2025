import os
import shutil
import time
import hashlib
from dataclasses import dataclass
from enum import Enum
from typing import Optional, Dict
from pathlib import Path

class StorageTier(Enum):
    HOT = 'hot'
    WARM = 'warm'
    COLD = 'cold'

@dataclass
class FileMetadata:
    filename: str
    tier: StorageTier
    size: int
    created_at: float
    last_accessed: float
    access_count: int
    checksum: str

class HybridStorageManager:
    def __init__(self, hot_path: str, warm_path: str, cold_path: str):
        self.paths = {
            StorageTier.HOT: Path(hot_path),
            StorageTier.WARM: Path(warm_path),
            StorageTier.COLD: Path(cold_path)
        }
        self.metadata: Dict[str, FileMetadata] = {}
        self._ensure_dirs()

    def _ensure_dirs(self):
        for path in self.paths.values():
            path.mkdir(parents=True, exist_ok=True)

    def _get_path(self, filename: str, tier: StorageTier) -> Path:
        return self.paths[tier] / filename

    def put(self, filename: str, data: bytes):
        """Всегда сохраняем новые файлы в HOT"""
        path = self._get_path(filename, StorageTier.HOT)
        path.write_bytes(data)
        
        self.metadata[filename] = FileMetadata(
            filename=filename,
            tier=StorageTier.HOT,
            size=len(data),
            created_at=time.time(),
            last_accessed=time.time(),
            access_count=1,
            checksum=hashlib.md5(data).hexdigest()
        )
        print(f"[PUT] Файл {filename} сохранен в HOT tier.")

    def get(self, filename: str) -> Optional[bytes]:
        """Чтение с автоматическим продвижением (promote)"""
        if filename not in self.metadata:
            print("Файл не найден!")
            return None
        
        meta = self.metadata[filename]
        
        # Если файл не в HOT, продвигаем его
        if meta.tier != StorageTier.HOT:
            self._promote(filename)
        
        path = self._get_path(filename, StorageTier.HOT)
        meta.last_accessed = time.time()
        meta.access_count += 1
        print(f"[GET] Файл {filename} прочитан. Обновлено время доступа.")
        return path.read_bytes()

    def _promote(self, filename: str):
        """Перемещение на уровень выше (в HOT)"""
        meta = self.metadata[filename]
        old_path = self._get_path(filename, meta.tier)
        new_path = self._get_path(filename, StorageTier.HOT)
        
        old_tier_name = meta.tier.value.upper() # <-- Запоминаем старый уровень ДО изменения
        shutil.move(old_path, new_path)
        meta.tier = StorageTier.HOT
        print(f"[PROMOTE] Файл {filename} перемещен из {old_tier_name} в HOT.")

    def _demote(self, filename: str, target_tier: StorageTier):
        """Перемещение на уровень ниже"""
        meta = self.metadata[filename]
        old_path = self._get_path(filename, meta.tier)
        new_path = self._get_path(filename, target_tier)
        
        shutil.move(old_path, new_path)
        old_tier_name = meta.tier.value.upper()
        meta.tier = target_tier
        print(f"[DEMOTE] Файл {filename} перемещен из {old_tier_name} в {target_tier.value.upper()}.")

    def migrate(self, days_hot_to_warm: int = 7, days_warm_to_cold: int = 30):
        """Автоматическая миграция на основе времени"""
        now = time.time()
        for filename, meta in list(self.metadata.items()):
            age_days = (now - meta.last_accessed) / 86400.0

            if meta.tier == StorageTier.HOT and age_days > days_hot_to_warm:
                self._demote(filename, StorageTier.WARM)
            elif meta.tier == StorageTier.WARM and age_days > days_warm_to_cold:
                self._demote(filename, StorageTier.COLD)

    def status(self):
        print("\n--- ТЕКУЩИЙ СТАТУС ХРАНИЛИЩА ---")
        for tier in StorageTier:
            files = [f for f, m in self.metadata.items() if m.tier == tier]
            print(f"{tier.value.upper()} tier: {len(files)} файлов")
            for f in files:
                print(f"   - {f}")
        print("--------------------------------\n")

# --- Интерактивное меню для демонстрации ---
if __name__ == "__main__":
    # В Docker пути будут смонтированы сюда:
    mgr = HybridStorageManager("/data/hot", "/data/warm", "/data/cold")
    
    print("Гибридное хранилище готово!")
    print("Команды: put <имя> <текст> | get <имя> | status | migrate <дни> | exit")

    while True:
        try:
            cmd = input("> ").strip().split()
            if not cmd: continue

            if cmd[0] == "exit":
                break
            elif cmd[0] == "put":
                name = cmd[1]
                text = " ".join(cmd[2:]).encode('utf-8')
                mgr.put(name, text)
            elif cmd[0] == "get":
                data = mgr.get(cmd[1])
                if data:
                    print(f"Содержимое: {data.decode('utf-8')}")
            elif cmd[0] == "status":
                mgr.status()
            elif cmd[0] == "migrate":
                days = int(cmd[1]) if len(cmd) > 1 else 7
                # Искусственно старим время для теста
                for meta in mgr.metadata.values():
                    meta.last_accessed -= days * 86400
                mgr.migrate()
                mgr.status()
            else:
                print("Неизвестная команда")
        except Exception as e:
            print(f"Ошибка: {e}")