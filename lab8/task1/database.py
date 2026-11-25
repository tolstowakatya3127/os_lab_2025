# database.py
import sqlite3
import threading
import time
import stat

class Database:
    def __init__(self, db_path):
        self.db_path = db_path
        self.lock = threading.Lock() # Защита от одновременного доступа
        self._init_db()

    def _get_conn(self):
        return sqlite3.connect(self.db_path)

    def _init_db(self):
        with self.lock:
            conn = self._get_conn()
            cursor = conn.cursor()
            # Таблица метаданных файлов
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS files (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    path TEXT UNIQUE NOT NULL,
                    mode INTEGER NOT NULL,
                    uid INTEGER NOT NULL,
                    gid INTEGER NOT NULL,
                    size INTEGER DEFAULT 0,
                    atime REAL NOT NULL,
                    mtime REAL NOT NULL,
                    ctime REAL NOT NULL,
                    nlink INTEGER DEFAULT 1
                )
            ''')
            # Таблица содержимого (чанков по 4КБ)
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS file_data (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    file_id INTEGER NOT NULL,
                    chunk_index INTEGER NOT NULL,
                    data BLOB,
                    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
                    UNIQUE(file_id, chunk_index)
                )
            ''')
            
            # Создаем корневую директорию, если её нет
            cursor.execute("SELECT id FROM files WHERE path = '/'")
            if not cursor.fetchone():
                now = time.time()
                cursor.execute('''
                    INSERT INTO files (path, mode, uid, gid, size, atime, mtime, ctime)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                ''', ('/', stat.S_IFDIR | 0o755, 0, 0, 0, now, now, now))
            
            conn.commit()
            conn.close()