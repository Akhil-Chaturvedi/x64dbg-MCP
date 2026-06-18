function parseTimeout(raw: string | undefined): number {
  const value = parseInt(raw ?? '0', 10);
  if (Number.isNaN(value) || value <= 0) {
    return 0;
  }
  return value;
}

export const config = {
  host: process.env.X64DBG_MCP_HOST ?? '127.0.0.1',
  port: parseInt(process.env.X64DBG_MCP_PORT ?? '27042', 10),
  timeout: parseTimeout(process.env.X64DBG_MCP_TIMEOUT),
  retries: parseInt(process.env.X64DBG_MCP_RETRIES ?? '3', 10),
  token: process.env.X64DBG_MCP_TOKEN ?? '',
};

export function getBaseUrl(): string {
  return `http://${config.host}:${config.port}`;
}