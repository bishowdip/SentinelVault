# SentinelVault-C

A secure networked evidence vault demonstrating operating-system and security
concepts in C11: POSIX threads, synchronization, paging algorithms, protected
file operations, encryption, tamper-evident auditing, and concurrent TCP IPC.

## Build

Requirements: C compiler, Make, POSIX threads, and OpenSSL 3. On Apple Silicon,
the Makefile uses Homebrew OpenSSL at `/opt/homebrew/opt/openssl@3`.

```bash
make
make test
```

## Project layout

- `include/` contains header files and shared interfaces.
- `src/` contains C implementation files and demo entry points.
- `tests/` contains automated test programs.

## Run

```bash
./bin/scheduler_demo
./bin/memory_demo
./bin/process_demo
./bin/permission_demo
./bin/sentinelvault_server 9090
./bin/sentinelvault_client 127.0.0.1 9090
```

Demo accounts are created on first run:

| Username | Password | Role |
|---|---|---|
| admin | admin123 | admin |
| author | author123 | author |
| auditor | audit123 | auditor |
| guest | guest123 | viewer |

Use `HELP` in the client for the command list.

## Automated coverage

- Known FIFO, LRU, and pointer-based FIFO behavior
- Memory CSV schema and generation
- Fork/pipe process IPC
- Valid and invalid authentication
- Three-strike account lockout
- Filename validation and path-traversal rejection
- Protocol authentication gate
- Create, write, chmod, encrypt, authorized read, decrypt, and delete
- POSIX file creation, chmod, stat, and cleanup
- Permission denial for a low-privilege user
- Audit-chain verification

## Security design

- PBKDF2-HMAC-SHA256 password hashing with random per-user salts
- Unix-like owner/group/others permissions
- Strict filename allow-list preventing path traversal
- AES-256-CBC encryption with random IVs and a local mode-0600 key
- SHA-256 chained audit log with verification
- Bounded network input and one isolated session per client
- Mutexes and semaphores protecting shared file and metadata state

This is an academic prototype. It does not include TLS, hardware-backed key
storage, database transactions, or production identity management.
