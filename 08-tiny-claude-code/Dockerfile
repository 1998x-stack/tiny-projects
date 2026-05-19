FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    ripgrep \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

WORKDIR /workspace

COPY . /app/
ENV PYTHONPATH=/app

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "/app/main.py"]
