# AGENTS.md - AI Agent Guide for Couchbase Mobile Tools

## Repository Overview

This is a **fork** of the official Couchbase Labs repository:
- **Upstream:** https://github.com/couchbaselabs/couchbase-mobile-tools
- **Fork:** https://github.com/Fujio-Turner/couchbase-mobile-tools

This repository contains multiple command-line tools for working with Couchbase Lite databases:
- **cblite** - Database inspection, query, and replication tool (primary tool)
- **cbl-log** - Log file analysis tool
- **LargeDatasetGenerator** - Load testing data generator

## Building the Project

### Prerequisites
```bash
# Ensure submodules are initialized
git submodule update --init --recursive
```

### Build Methods

#### Option 1: Interactive Python Script (Easiest)
```bash
# Install dependencies
pip install -r requirements.txt

# Run interactive build
python3 build.py
```

#### Option 2: CMake (For cblite)
```bash
cd cblite
mkdir -p build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
make -j 5 cblite
```

The built executable will be at: `cblite/build/cblite`

#### Option 3: Xcode (macOS only)
```bash
cd Xcode
# Open Tools.xcodeproj in Xcode
# Build the 'cblite' scheme
# For release: Product > Archive or Product > Build For > Profiling
```

### Build Requirements
- **CMake:** Version 3.5+ (4.x requires `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` flag)
- **C++ Compiler:** C++17 or later
- **macOS:** Xcode 11.4+ recommended
- **Linux:** GCC 7+ or Clang 3.9+
- **Windows:** Visual Studio 2017+ (MSVC 14.10+)

### Testing
```bash
# Run tests (from build directory)
cd cblite/build
./cblitetest
```

## Recent Enhancements (Custom to This Fork)

### 1. Fixed Collection Limit Bug
**Issue:** Pull/push commands only processed 3 collections maximum  
**Fix:** Overrode `processFlag()` in CpCommand to handle `--collection` before parent class intercepts it  
**Files:** `cblite/CpCommand.cc`

### 2. Channel Filtering Support
**Feature:** Per-collection channel filters for Sync Gateway replication  
**Flag:** `--channels <channel,...>` (must follow `--collection`)  
**Usage:**
```bash
cblite pull mydb.cblite2 \
  --collection us.prices --channels online \
  --collection us.jobs --channels "atl:100,bob" \
  wss://sync-gateway.example.com/mydb
```
**Files:** `cblite/CpCommand.cc`, `litecp/DBEndpoint.{hh,cc}`

### 3. OIDC Bearer Token Authentication
**Feature:** Use OIDC bearer tokens from file for authentication  
**Flag:** `--oidc <file>`  
**Usage:**
```bash
echo "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9..." > ~/token.txt
cblite pull mydb.cblite2 --oidc ~/token.txt wss://...
```
**Implementation:** Reads token from file, sends as `Authorization: Bearer {token}` header  
**Files:** `cblite/CpCommand.cc`, `litecp/DBEndpoint.{hh,cc}`

### 4. Enhanced Version Display
**Feature:** Shows LiteCore git tag and full commit hash  
**Output:**
```
cblite tool for Couchbase Lite (LiteCore) 0.0.0 (cffb68ff1536aa3a+) [3.2.1]
LiteCore git: cffb68ff1536aa3afd6163c4b74c3eca30fed9d7
```
**Files:** `cblite/CBLiteTool.{hh,cc}`, `cblite/CMakeLists.txt`, `vendor/couchbase-lite-core/cmake/generate_edition.cmake`, `vendor/couchbase-lite-core/cmake/repo_version.h.in`

## Code Organization

### Main Tools
- `cblite/` - cblite tool source code (command-line interface for databases)
- `cbl-log/` - cbl-log tool source code (log file parser)
- `litecp/` - Shared endpoint classes for replication/import/export

### Key Files for cblite Tool
- `cblite/CBLiteTool.{hh,cc}` - Main tool entry point and command dispatcher
- `cblite/CBLiteCommand.{hh,cc}` - Base class for all commands
- `cblite/CpCommand.cc` - Copy/replication command (pull, push, cp, export, import)
- `litecp/DBEndpoint.cc` - Database endpoint, handles replication configuration
- `litecp/RemoteEndpoint.cc` - Remote database endpoint (Sync Gateway)

### Command Structure
Commands inherit from `CBLiteCommand` which inherits from `LiteCoreTool`:
```
Tool (vendor/couchbase-lite-core/tool_support/Tool.hh)
  └─ LiteCoreTool (vendor/couchbase-lite-core/tool_support/LiteCoreTool.hh)
      └─ CBLiteCommand (cblite/CBLiteCommand.hh)
          └─ Specific commands (CpCommand, QueryCommand, etc.)
```

### Flag Processing Pattern
When adding new flags to a command:
1. Override `processFlag()` to handle your flags first
2. Call parent's `processFlag()` for standard flags
3. Return `true` if handled, `false` if not

Example from CpCommand:
```cpp
bool processFlag(const std::string &flag, const std::initializer_list<FlagSpec> &specs) override {
    if (flag == "--mycustomflag") {
        handleMyFlag();
        return true;
    }
    return CBLiteCommand::processFlag(flag, specs);
}
```

## Replication Architecture

### C4 API Used
- **C4Replicator** - Core replication engine
- **C4ReplicationCollection** - Per-collection replication config
- **kC4ReplicatorOptionChannels** - Channel filter option (Fleece array)
- **kC4ReplicatorOptionExtraHeaders** - Custom HTTP headers (Fleece dict)

### Adding Replication Options
Options are encoded as Fleece dictionaries:
```cpp
Encoder enc;
enc.beginDict();
enc.writeKey(slice(kC4ReplicatorOptionChannels));
enc.beginArray();
for (const string& channel : channels) {
    enc.writeString(channel);
}
enc.endArray();
enc.endDict();
alloc_slice options = enc.finish();
```

## Important Notes

### Vendor Submodule
- `vendor/couchbase-lite-core` is a git submodule
- Custom changes made to LiteCore cmake for GitTag support
- When updating submodule, these changes will need to be reapplied

### Security
- Never log or expose secrets (passwords, tokens, keys)
- OIDC tokens are read from files and kept in memory only
- Credentials are passed directly to C4 API, not logged

### Documentation
- Main docs: `Documentation.md`
- Tool-specific: `README.cblite.md`, `README.cbl-log.md`
- Building: `BUILDING.md`

## Common Commands

### Replication with all features
```bash
cblite pull mydb.cblite2 \
  --oidc ~/.config/sg_token \
  --collection us.prices --channels "online,retail" \
  --collection us.jobs --channels "atl:100" \
  --continuous --verbose \
  wss://sync-gateway.example.com/mydb
```

### Creating indexes
```bash
# Simple index
cblite mkindex mydb.cblite2 byDate createdAt

# Composite index with JSON
cblite mkindex --json mydb.cblite2 byTypeAndDate \
  '{"what":[[".docType"],[".createdAt"]]}'
```

### Querying with index verification
```bash
cblite query --explain mydb.cblite2 \
  'SELECT * FROM _ WHERE docType = "invoice" ORDER BY createdAt'
# Look for "USING INDEX byTypeAndDate" in output
```

## Development Notes

### Adding a New Flag to cp/pull/push Commands
1. Add flag to usage text in `CpCommand::usage()` and `CpCommand::openRemoteUsage()`
2. Override `processFlag()` in CpCommand to handle the flag
3. Add member variable to store flag value
4. If flag affects replication, add setter in `DBEndpoint.hh`
5. Update `DBEndpoint::replicatorParameters()` or `DBEndpoint::startReplicationWith()` to use the flag
6. Update `Documentation.md` with flag documentation and examples
7. Rebuild and test

### Code Style
- Use existing patterns from the codebase
- Include error checking and validation
- Add mutual exclusivity checks for conflicting flags
- Follow the existing flag naming conventions (lowercase with dashes)
