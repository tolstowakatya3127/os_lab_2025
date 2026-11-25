import os
import time
import statistics
import boto3
import matplotlib.pyplot as plt
from dataclasses import dataclass
from typing import List

# Единый интерфейс для всех бэкендов 
class BenchmarkBase:
    def __init__(self, path: str):
        self.path = path

    def write_file(self, filename: str, size_mb: int):
        raise NotImplementedError

    def read_file(self, filename: str):
        raise NotImplementedError

    def delete_file(self, filename: str):
        raise NotImplementedError

#  1. Бэкенд для FUSE (s3fs и goofys) и локальной ФС 
class FUSEBenchmark(BenchmarkBase):
    def write_file(self, filename, size_mb):
        data = b'\0' * (size_mb * 1024 * 1024)
        with open(os.path.join(self.path, filename), 'wb') as f:
            f.write(data)

    def read_file(self, filename):
        with open(os.path.join(self.path, filename), 'rb') as f:
            return f.read()

    def delete_file(self, filename):
        os.remove(os.path.join(self.path, filename))

#  2. Бэкенд для boto3 (Native API) 
class Boto3Benchmark(BenchmarkBase):
    def __init__(self, bucket: str, endpoint: str):
        super().__init__("")
        self.bucket = bucket
        self.s3 = boto3.client(
            's3',
            endpoint_url=endpoint,
            aws_access_key_id='minioadmin',
            aws_secret_access_key='minioadmin123',
            region_name='us-east-1'
        )

    def write_file(self, filename, size_mb):
        data = b'\0' * (size_mb * 1024 * 1024)
        self.s3.put_object(Bucket=self.bucket, Key=filename, Body=data)

    def read_file(self, filename):
        response = self.s3.get_object(Bucket=self.bucket, Key=filename)
        return response['Body'].read()

    def delete_file(self, filename):
        self.s3.delete_object(Bucket=self.bucket, Key=filename)

#  Функция замера 
def run_benchmark(backend: BenchmarkBase, name: str, num_files=5, size_mb=1):
    print(f"Запуск теста: {name}...", end=" ", flush=True)
    latencies = []
    try:
        start_total = time.perf_counter()
        for i in range(num_files):
            start = time.perf_counter()
            backend.write_file(f"test_file_{i}.bin", size_mb)
            latencies.append((time.perf_counter() - start) * 1000)
        total_time = time.perf_counter() - start_total
        
        # Очистка
        for i in range(num_files):
            backend.delete_file(f"test_file_{i}.bin")
            
        throughput = (num_files * size_mb) / total_time
        print("Успешно!")
        
        return {
            "name": name,
            "throughput_mbps": round(throughput, 2),
        }
    except Exception as e:
        print(f"ОШИБКА (Пропущено). Причина: {e}")
        return None

#  Главный запуск 
if __name__ == '__main__':
    print(" СТАРТ БЕНЧМАРКА ")
    results = []
    
    # 1. Тестируем boto3 (Native API) - он работает всегда, пока жив MinIO
    boto_backend = Boto3Benchmark("benchmark", "http://localhost:9000")
    res_boto = run_benchmark(boto_backend, "boto3 (Native API)")
    if res_boto: results.append(res_boto)

    # 2. Тестируем s3fs (если папка не пустая, значит FUSE сработал)
    if os.path.exists("/mnt/s3fs"):
        res_s3fs = run_benchmark(FUSEBenchmark("/mnt/s3fs"), "s3fs (FUSE)")
        if res_s3fs: results.append(res_s3fs)
    else:
        print("Тест s3fs пропущен: папка не существует.")

    # 3. Тестируем локальную дисковую систему (для сравнения)
    # Создадим временную папку для честного сравнения
    os.makedirs("/tmp/local_test", exist_ok=True)
    res_local = run_benchmark(FUSEBenchmark("/tmp/local_test"), "Local Disk (SSD)")
    if res_local: results.append(res_local)

    #  Построение графиков 
    if len(results) > 0:
        names = [r["name"] for r in results]
        throughputs = [r["throughput_mbps"] for r in results]

        plt.figure(figsize=(10, 6))
        bars = plt.bar(names, throughputs, color=['#e74c3c', '#3498db', '#2ecc71'])
        plt.ylabel('Throughput (MB/s)')
        plt.title('Сравнение пропускной способности хранилищ')

        for bar, val in zip(bars, throughputs):
            plt.text(bar.get_x() + bar.get_width()/2, bar.get_height(), f'{val} MB/s', 
                     ha='center', va='bottom')

        plt.tight_layout()
        plt.savefig('throughput_comparison.png')
        print("\n ГОТОВО ")
        print("График сохранен в файл 'throughput_comparison.png'")
    else:
        print("Нет данных для построения графика.")