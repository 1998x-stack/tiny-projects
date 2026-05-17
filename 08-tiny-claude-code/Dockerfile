FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    ripgrep \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir \
    anthropic \
    openai \
    pyyaml \
    rich

WORKDIR /workspace

# Copy .env template
RUN echo 'ANTHROPIC_API_KEY=your-key-here\n# Or use OpenAI-compatible:\n# OPENAI_API_KEY=your-key-here\n# OPENAI_BASE_URL=https://api.openai.com/v1' > /workspace/.env.example

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
