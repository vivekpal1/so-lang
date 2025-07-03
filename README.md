# So Programming Language

**A fast, simple toy programming language that compiles to C, Rust, and also compiles Solana programs**

## 📋 Language Examples

### Basic So Lang
```so
// Variables and arithmetic
let greeting = "Hello, So Lang!"
let version = 2
let result = 42 * 2

// Output
print(greeting)
print(version)
print(result)

// Functions
fn add(a, b) {
    return a + b
}

let sum = add(10, 20)
print(sum)

// Control flow
if sum > 25 {
    print("big number")
} else {
    print("small number")
}
```

### Solana Program
```so
program Counter {
    // State structure
    state CounterAccount {
        count: u64,
        authority: pubkey
    }
    
    // Initialize counter
    instruction initialize(
        @account(init, signer, writable) counter: CounterAccount,
        @account(signer) user: pubkey
    ) {
        counter.count = 0
        counter.authority = user.key
        print("Counter initialized")
    }
    
    // Increment counter
    instruction increment(
        @account(writable) counter: CounterAccount,
        @account(signer) user: pubkey
    ) {
        require(user.key == counter.authority, "Unauthorized")
        counter.count = counter.count + 1
        print("Counter incremented")
    }
    
    // Get current count
    instruction get_count(
        @account counter: CounterAccount
    ) {
        print(counter.count)
    }
}
```

## 🔧 Quick Start

### Prerequisites
```bash
# Basic development tools
sudo apt update && sudo apt install gcc make

# For Solana development (optional but recommended)
sh -c "$(curl -sSfL https://release.solana.com/v1.16.0/install)"
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### Build So Lang
```bash
# Clone/download the project
# Navigate to the project directory

# Build the optimized compiler
make

# Test it works
make test

# Test Solana functionality (if Solana CLI installed)
make test-solana

# Create example programs
make examples
```

### Your First Program

#### Basic Program
```bash
# Create a simple program
echo 'let message = "Hello, So Lang!"
let number = 42
print(message)
print(number)' > hello.so

# Compile to C
./bin/solang hello.so
gcc output.c -o hello && ./hello

# Or compile to Rust
./bin/solang hello.so --rust
rustc output.rs -o hello && ./hello
```

#### Solana Program
```bash
# Create a Solana program
echo 'program HelloSolana {
    instruction say_hello() {
        print("Hello from Solana blockchain!")
    }
}' > hello_solana.so

# Compile (automatically detected as Solana program)
./bin/solang hello_solana.so

# Output: 
# ✓ Detected Solana program
# Program ID for HelloSolana: ABC123...
# Generated: program.rs
```

## Solana

### Automatic Program Detection
So Lang automatically detects Solana programs by looking for:
- `program` keyword with program declarations
- Solana-specific constructs like `@account`, `instruction`, `state`
- Use of `--solana`, `--anchor`, or `--native-solana` flags

### Program ID Management
```bash
# Program IDs are automatically generated and managed
./bin/solang MyProgram.so --solana

# Output:
# Generating new program keypair for MyProgram...
# Program ID for MyProgram: HeLp1c5B5yfu6f1ryx5VvH5CffGgWg8F7auC7Dg1PWwr
# Keypair saved: keypairs/MyProgram-keypair.json
```

**Program keypairs are stored in `keypairs/` directory:**
```
keypairs/
├── Counter-keypair.json
├── TokenTransfer-keypair.json
└── VotingDAO-keypair.json
```

### Solana Language Features

#### Account Constraints
```so
@account(signer)                    // Must sign transaction
@account(writable)                  // Can modify account data
@account(init, signer, writable)    // Initialize new account
@account(seeds = ["user", user.key], bump)  // Program Derived Address
```

#### Instructions with Validation
```so
instruction transfer_tokens(
    @account(writable) from: TokenAccount,
    @account(writable) to: TokenAccount,
    @account(signer) authority: pubkey,
    amount: u64
) {
    require(from.amount >= amount, "Insufficient balance")
    require(authority.key == from.authority, "Unauthorized")
    transfer(from, to, authority, amount)
    print("Transfer completed")
}
```

#### State Management
```so
state UserProfile {
    owner: pubkey,
    balance: u64,
    created_at: u64,
    is_active: bool
}
```

## 🧪 Testing and Deployment

### Automated Solana Testing
```bash
# Complete end-to-end test with real Solana deployment
make test-solana

# This automatically:
# 1. Builds So Lang compiler
# 2. Starts solana-test-validator
# 3. Creates and compiles test program
# 4. Builds Rust project
# 5. Deploys to local Solana testnet
# 6. Verifies deployment success
```

### Manual Solana Development
```bash
# Setup Solana environment
make setup-solana

# Start local validator
solana-test-validator --reset &

# Compile Solana program
./bin/solang examples/counter.so --native-solana

# Build and deploy (example for counter program)
# ... create Rust project structure ...
# cargo build-bpf
# solana program deploy target/deploy/program.so
```

## 🎯 Compilation Targets

### Command Line Options
```bash
./bin/solang program.so [options]

Options:
  --rust           Compile to Rust (regular program)
  --solana         Force Solana program compilation
  --anchor         Use Anchor framework (implies --solana --rust)
  --native-solana  Use native Solana (implies --solana --rust)
  --output FILE    Specify output file name
```

### Compilation Flow
```
So Lang Source (.so)
    ↓
Smart Detection/Explicit Flag
    ↓
┌─────────────────┬─────────────────┬─────────────────┐
│   Regular C     │   Regular Rust  │  Solana Rust    │
│                 │                 │                 │
│   output.c      │   output.rs     │  program.rs     │
│   ↓             │   ↓             │   ↓             │
│   GCC           │   rustc         │  cargo build-bpf│
│   ↓             │   ↓             │   ↓             │
│   Executable    │   Executable    │  .so (Solana)   │
└─────────────────┴─────────────────┴─────────────────┘
```

## 🏗️ Architecture

The So Lang compiler follows a clean, traditional pipeline:

1. **Lexer**: Tokenizes source code into meaningful symbols
2. **Parser**: Builds Abstract Syntax Tree (AST) from tokens  
3. **Detector**: Automatically identifies Solana programs
4. **Code Generator**: Emits C, Rust, or Solana-specific Rust code

### Key Components
- **Lexer** (`lexer_*` functions): Fast character-by-character tokenization with Solana keywords
- **Parser** (`parser_*` functions): Recursive descent parser with Solana syntax support
- **AST** (`ast_*` functions): Tree-based intermediate representation
- **Detector** (`detect_solana_program`): Smart program type detection
- **Compiler** (`compiler_*` functions): Multi-target code generation
- **Solana Utils**: Program ID generation, keypair management, validation

## 🛡️ Security and Validation

### Built-in Security for Solana Programs
- **Automatic Account Validation**: `@account` constraints enforced at compile time
- **Signer Verification**: `@account(signer)` automatically generates validation code
- **Ownership Checks**: Account ownership verified in generated code
- **Integer Overflow Protection**: Safe arithmetic operations
- **Program ID Validation**: Validates program IDs are valid Solana pubkeys

### Error Handling
```so
instruction secure_transfer(
    @account(writable, signer) from: TokenAccount,
    @account(writable) to: TokenAccount,
    amount: u64
) {
    require(from.amount >= amount, "Insufficient funds")
    require(amount > 0, "Amount must be positive")
    // Safe operations automatically generated
}
```

## ⚡ Performance

- **Compilation Speed**: 
  - Basic programs: ~0.001s
  - Solana programs: ~0.003s  
  - Complex programs: ~0.01s
- **Generated Code**: Zero-overhead, equivalent to hand-written code
- **Memory Usage**: Minimal allocation during compilation
- **Runtime Performance**: No runtime overhead (compiled languages)

## 🔧 Build Targets

```bash
# Core build targets
make                # Build optimized release version (default)
make debug          # Build debug version with symbols
make test           # Run basic test suite
make test-solana    # Run complete Solana integration test

# Development targets  
make examples       # Create example programs
make benchmark      # Performance testing
make memcheck       # Memory leak detection (requires valgrind)
make format         # Code formatting (requires clang-format)

# Environment setup
make setup-solana   # Install Solana development environment
make install        # Install So Lang to system PATH
make clean          # Remove build artifacts
make help           # Show all available targets
```

## 🤝 Contributing

Contributions are welcome! The codebase is designed to be educational and extensible:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/new-feature`
3. **Make your changes** to `so_lang.c` and `so_lang.h`
4. **Test thoroughly**: `make test && make test-solana`
5. **Follow the coding style** established in the project
6. **Submit a pull request** with a clear description

### Adding New Features
- **New Token Types**: Add to `TokenType` enum and lexer
- **New AST Nodes**: Add to `NodeType` enum and parser
- **New Backends**: Extend compiler with new target support
- **Solana Features**: Add new account constraints or instructions

## 🎯 Quick Commands Reference

```bash
# Setup and build
make setup-solana && make

# Test everything
make test && make test-solana

# Basic compilation
./bin/solang program.so                    # Auto-detect target
./bin/solang program.so --rust             # Force Rust
./bin/solang program.so --solana           # Force Solana

# Solana development
echo 'program Test { instruction hello() { print("Hello!") } }' > test.so
./bin/solang test.so                       # Auto-detects Solana program

# Get help
make help
./bin/solang --help
```

---

**🚀 Happy compiling!**
