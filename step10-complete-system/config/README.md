# Configuration Files

This directory contains configuration files for the multi-tenant gRPC server.

## Files

- **config.json** - Default development configuration
- **config.production.json** - Production environment configuration
- **config.test.json** - Test environment configuration

## Configuration Structure

### Server Settings
- `host`: Server bind address (default: "0.0.0.0")
- `port`: Server port (default: 50053)
- `enable_port_reuse`: Allow quick port reuse (SO_REUSEADDR)
- `max_receive_message_size`: Maximum message size for incoming messages (-1 = unlimited)
- `max_send_message_size`: Maximum message size for outgoing messages (-1 = unlimited)

### Logging Settings
- `level`: Log level (trace, debug, info, warn, error, critical, off)
- `format`: Log format (default, json, custom)
- `enable_console`: Enable console output
- `log_file_path`: Path to log file (empty = no file logging)
- `max_file_size`: Maximum log file size in bytes
- `max_files`: Maximum number of rotating log files

### Interceptor Settings
- `enable_logging`: Enable logging interceptor
- `enable_auth`: Enable authentication interceptor
- `enable_tenant`: Enable tenant context interceptor
- `enable_metrics`: Enable metrics collection interceptor

### Database Settings
- `type`: Database type (sqlite)
- `connection_string`: Database connection string (":memory:" for in-memory)
- `pool_size`: Number of connections in the pool
- `connection_timeout`: Connection timeout in seconds

### Security Settings
- `enable_tls`: Enable TLS/SSL
- `cert_file`: Path to server certificate file
- `key_file`: Path to server private key file
- `ca_file`: Path to CA certificate file
- `require_client_auth`: Require client certificate authentication

## Usage

The server loads configuration from `config.json` by default. You can specify a different configuration file using the `--config` command line argument:

```bash
./step10_server --config=config/config.production.json
```

Or set the environment:

```bash
export CONFIG_FILE=config/config.production.json
./step10_server
```

## Environment-Specific Configurations

### Development (config.json)
- Debug logging enabled
- Console output enabled
- In-memory database
- No TLS
- All interceptors enabled

### Production (config.production.json)
- Warning level logging
- JSON formatted logs
- File-based logging with rotation
- Persistent database
- TLS enabled (requires certificates)
- All interceptors including metrics

### Test (config.test.json)
- Debug logging
- Console output only
- In-memory database
- Simplified interceptor chain
- No authentication/tenant checks

## Configuration Validation

The server validates all configuration values on startup:
- Port must be between 1024-65535
- Log level must be valid
- Database pool size must be at least 1
- TLS requires cert and key files if enabled

Invalid configuration will prevent server startup with an error message.
