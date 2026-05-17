FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    graphviz \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir \
    graphviz \
    pytest \
    jupyter \
    numpy \
    matplotlib

WORKDIR /workspace

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
