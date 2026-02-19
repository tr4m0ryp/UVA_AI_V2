FROM node:22-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends git openssh-client ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN npm install -g @openai/codex@latest

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /workspace

ENTRYPOINT ["/entrypoint.sh"]
