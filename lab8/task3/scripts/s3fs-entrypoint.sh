#!/bin/bash
set -e

echo "${AWS_ACCESS_KEY_ID}:${AWS_SECRET_ACCESS_KEY}" > /etc/passwd-s3fs
chmod 600 /etc/passwd-s3fs

# Ждем MinIO (Silo)
until curl -sf "${S3_ENDPOINT}/minio/health/live"; do
    echo 'Waiting for MinIO...'
    sleep 2
done

echo "MinIO is ready. Mounting S3..."

# Монтируем с принудительным V4 и разрешением на непустую папку
s3fs ${S3_BUCKET} /mnt/s3 \
    -o passwd_file=/etc/passwd-s3fs \
    -o url=${S3_ENDPOINT} \
    -o use_path_request_style \
    -o allow_other \
    -o nonempty \
    -o sigv4

echo "S3 Mounted successfully!"
tail -f /dev/null