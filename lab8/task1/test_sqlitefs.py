# test_sqlitefs.py
import unittest
import tempfile
import os
import stat
from sqlitefs import SQLiteFS
from database import Database

class TestSQLiteFS(unittest.TestCase):
    def setUp(self):
        '''Создание временной БД перед каждым тестом'''
        self.temp_db = tempfile.NamedTemporaryFile(delete=False)
        self.db = Database(self.temp_db.name)
        self.fs = SQLiteFS(self.db)

    def tearDown(self):
        '''Очистка после теста'''
        self.temp_db.close()
        os.unlink(self.temp_db.name)

    def test_root_exists(self):
        '''Корневая директория должна существовать'''
        attr = self.fs.getattr('/')
        self.assertTrue(stat.S_ISDIR(attr['st_mode']))

    def test_create_file(self):
        '''Тест создания файла и проверки его атрибутов'''
        self.fs.create('/test.txt', 0o644)
        attr = self.fs.getattr('/test.txt')
        self.assertTrue(stat.S_ISREG(attr['st_mode']))
        self.assertEqual(attr['st_size'], 0)

    def test_write_and_read(self):
        '''Тест записи и чтения данных (включая работу с чанками)'''
        self.fs.create('/test.txt', 0o644)
        
        # Записываем данные
        data = b"Hello FUSE! This is a test message."
        written = self.fs.write('/test.txt', data, 0, None)
        self.assertEqual(written, len(data))
        
        # Проверяем размер
        attr = self.fs.getattr('/test.txt')
        self.assertEqual(attr['st_size'], len(data))
        
        # Читаем данные
        read_data = self.fs.read('/test.txt', len(data), 0, None)
        self.assertEqual(read_data, data)

    def test_readdir(self):
        '''Тест получения списка файлов в директории'''
        self.fs.mkdir('/mydir', 0o755)
        self.fs.create('/mydir/file1.txt', 0o644)
        self.fs.create('/mydir/file2.txt', 0o644)
        
        files = self.fs.readdir('/mydir', None)
        self.assertIn('file1.txt', files)
        self.assertIn('file2.txt', files)
        self.assertEqual(len(files), 4) # ., .., file1.txt, file2.txt

    def test_unlink_and_rmdir(self):
        '''Тест удаления файлов и директорий'''
        self.fs.mkdir('/mydir', 0o755)
        self.fs.create('/mydir/file.txt', 0o644)
        
        # Удаляем файл
        self.fs.unlink('/mydir/file.txt')
        with self.assertRaises(Exception): # Должно выдать ошибку ENOENT, так как файла больше нет
            self.fs.getattr('/mydir/file.txt')
            
        # Удаляем папку
        self.fs.rmdir('/mydir')
        with self.assertRaises(Exception):
            self.fs.getattr('/mydir')

if __name__ == '__main__':
    unittest.main()