FROM python:3.10-slim
WORKDIR /app
# Копируем скрипт из папки app
COPY app/hybrid_storage.py .
CMD ["python3", "hybrid_storage.py"]