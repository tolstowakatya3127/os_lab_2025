# sqlitefs.py
#!/usr/bin/env python3
import os
import stat
import errno
import time
import sqlite3
from fuse import FUSE, FuseOSError, Operations
from database import Database

CHUNK_SIZE = 4096 # 4 КБ размер чанка

class SQLiteFS(Operations):
    def __init__(self, db: Database):
        self.db = db

    def _get_file(self, path):
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            cursor.execute("SELECT id, mode, uid, gid, size, atime, mtime, ctime FROM files WHERE path=?", (path,))
            row = cursor.fetchone()
            conn.close()
            return row

    def getattr(self, path, fh=None):
        row = self._get_file(path)
        if not row:
            raise FuseOSError(errno.ENOENT)
        return {
            'st_mode': row[1], 'st_uid': row[2], 'st_gid': row[3],
            'st_size': row[4], 'st_atime': row[5], 'st_mtime': row[6],
            'st_ctime': row[7], 'st_nlink': 1
        }

    def readdir(self, path, fh):
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            # Ищем все файлы, которые находятся внутри текущей директории
            prefix = path if path == '/' else path + '/'
            cursor.execute("SELECT path FROM files WHERE path LIKE ? AND path != ?", (prefix + '%', path))
            rows = cursor.fetchall()
            conn.close()

        res = ['.', '..']
        for (p,) in rows:
            # Отрезаем родительский путь, оставляя только имя
            name = p[len(prefix):]
            if '/' not in name: # Только прямые потомки
                res.append(name)
        return res

    def mkdir(self, path, mode):
        parent = os.path.dirname(path) if path != '/' else '/'
        if not self._get_file(parent):
            raise FuseOSError(errno.ENOENT)
        
        now = time.time()
        with self.db.lock:
            conn = self.db._get_conn()
            try:
                conn.execute('''
                    INSERT INTO files (path, mode, uid, gid, atime, mtime, ctime)
                    VALUES (?, ?, 0, 0, ?, ?, ?)
                ''', (path, stat.S_IFDIR | mode, now, now, now))
                conn.commit()
            except sqlite3.IntegrityError:
                raise FuseOSError(errno.EEXIST)
            finally:
                conn.close()

    def rmdir(self, path):
        row = self._get_file(path)
        if not row or not stat.S_ISDIR(row[1]):
            raise FuseOSError(errno.ENOTDIR)
        
        prefix = path + '/'
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            cursor.execute("SELECT id FROM files WHERE path LIKE ?", (prefix + '%',))
            if cursor.fetchone():
                conn.close()
                raise FuseOSError(errno.ENOTEMPTY) # Нельзя удалить непустую папку
            
            conn.execute("DELETE FROM files WHERE path=?", (path,))
            conn.commit()
            conn.close()

    def create(self, path, mode):
        now = time.time()
        with self.db.lock:
            conn = self.db._get_conn()
            conn.execute('''
                INSERT INTO files (path, mode, uid, gid, atime, mtime, ctime)
                VALUES (?, ?, 0, 0, ?, ?, ?)
            ''', (path, stat.S_IFREG | mode, now, now, now))
            conn.commit()
            conn.close()
        return 0

    def unlink(self, path):
        if not self._get_file(path):
            raise FuseOSError(errno.ENOENT)
        with self.db.lock:
            conn = self.db._get_conn()
            conn.execute("DELETE FROM files WHERE path=?", (path,))
            conn.commit()
            conn.close()

    def write(self, path, data, offset, fh):
        row = self._get_file(path)
        if not row:
            raise FuseOSError(errno.ENOENT)
        file_id = row[0]
        
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            
            # Чанковое хранение (разбиваем данные по 4096 байт)
            start_chunk = offset // CHUNK_SIZE
            end_chunk = (offset + len(data) - 1) // CHUNK_SIZE
            
            for chunk_idx in range(start_chunk, end_chunk + 1):
                # Вычисляем границы байтов для текущего чанка
                chunk_start = chunk_idx * CHUNK_SIZE
                write_start = max(0, offset - chunk_start)
                write_end = min(CHUNK_SIZE, offset + len(data) - chunk_start)
                
                # Получаем существующий чанк из БД (если запись идет в середину файла)
                cursor.execute("SELECT data FROM file_data WHERE file_id=? AND chunk_index=?", (file_id, chunk_idx))
                res = cursor.fetchone()
                chunk_data = bytearray(res[0]) if res else bytearray()
                
                # Расширяем чанк нулями, если нужно
                if len(chunk_data) < write_end:
                    chunk_data.extend(b'\x00' * (write_end - len(chunk_data)))
                
                # Вставляем новые данные
                data_start_idx = chunk_start + write_start - offset
                chunk_data[write_start:write_end] = data[data_start_idx : data_start_idx + (write_end - write_start)]
                
                # Сохраняем чанк в БД
                cursor.execute('''
                    INSERT OR REPLACE INTO file_data (file_id, chunk_index, data)
                    VALUES (?, ?, ?)
                ''', (file_id, chunk_idx, bytes(chunk_data)))
            
            # Обновляем размер файла
            new_size = max(row[4], offset + len(data))
            now = time.time()
            cursor.execute("UPDATE files SET size=?, mtime=? WHERE id=?", (new_size, now, file_id))
            
            conn.commit()
            conn.close()
        return len(data)

    def read(self, path, size, offset, fh):
        row = self._get_file(path)
        if not row: raise FuseOSError(errno.ENOENT)
        file_id = row[0]
        
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            
            start_chunk = offset // CHUNK_SIZE
            end_chunk = (offset + size - 1) // CHUNK_SIZE
            
            cursor.execute("SELECT chunk_index, data FROM file_data WHERE file_id=? AND chunk_index BETWEEN ? AND ? ORDER BY chunk_index", 
                           (file_id, start_chunk, end_chunk))
            chunks = cursor.fetchall()
            conn.close()

        result = bytearray()
        for chunk_idx, chunk_data in chunks:
            chunk_start = chunk_idx * CHUNK_SIZE
            
            # Вычисляем, какую часть чанка нужно вернуть
            read_start = max(0, offset - chunk_start)
            read_end = min(CHUNK_SIZE, offset + size - chunk_start)
            
            result.extend(chunk_data[read_start:read_end])
            
        return bytes(result)

    def truncate(self, path, length, fh=None):
        # Урезает файл (нужно для работы команды echo, которая перезаписывает файл с нуля)
        row = self._get_file(path)
        if not row: raise FuseOSError(errno.ENOENT)
        file_id = row[0]
        
        with self.db.lock:
            conn = self.db._get_conn()
            cursor = conn.cursor()
            last_chunk = length // CHUNK_SIZE
            cursor.execute("DELETE FROM file_data WHERE file_id=? AND chunk_index >= ?", (file_id, last_chunk + 1))
            cursor.execute("UPDATE files SET size=?, mtime=? WHERE id=?", (length, time.time(), file_id))
            conn.commit()
            conn.close()
        return 0

if __name__ == '__main__':
    import sys
    db_path = './test.db'
    mountpoint = sys.argv[1] if len(sys.argv) > 1 else '/tmp/sqlitefs'
    
    db = Database(db_path)
    fs = SQLiteFS(db)
    
    print(f"Монтирование в {mountpoint}... Нажмите Ctrl+C для выхода.")
    FUSE(fs, mountpoint, foreground=True)