#!/bin/bash
# deploy-solana.sh - Automated Solana Program Deployment Script
# Location: scripts/deploy-solana.sh
# Complete deployment automation for So Lang Solana programs

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SOLANG_COMPILER="$PROJECT_ROOT/bin/solang-solana"
EXAMPLES_DIR="$PROJECT_ROOT/examples/solana"
BUILD_DIR="$PROJECT_ROOT/solana_build"
KEYPAIRS_DIR="$PROJECT_ROOT/keypairs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "\n${PURPLE}==== $1 ====${NC}"
}

# Default configuration
CLUSTER="localhost"
FRAMEWORK="anchor"  # anchor or native
PROGRAM_NAME=""
AUTO_CONFIRM=false
SKIP_BUILD=false
SKIP_TESTS=false
VERBOSE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cluster)
            CLUSTER="$2"
            shift 2
            ;;
        --framework)
            FRAMEWORK="$2"
            shift 2
            ;;
        --program)
            PROGRAM_NAME="$2"
            shift 2
            ;;
        --auto-confirm)
            AUTO_CONFIRM=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help)
            cat << EOF
So Lang Solana Deployment Script

Usage: $0 [OPTIONS]

Options:
    --cluster CLUSTER      Target cluster (localhost, devnet, mainnet-beta)
    --framework FRAMEWORK  Target framework (anchor, native)
    --program PROGRAM      Specific program to deploy (default: all)
    --auto-confirm         Skip confirmation prompts
    --skip-build          Skip compilation step
    --skip-tests          Skip running tests
    --verbose             Verbose output
    --help                Show this help message

Examples:
    $0 --cluster localhost --framework anchor
    $0 --cluster devnet --program counter --auto-confirm
    $0 --framework native --skip-tests

EOF
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Verbose mode
if [ "$VERBOSE" = true ]; then
    set -x
fi

# Validation functions
check_dependencies() {
    log_step "Checking Dependencies"
    
    # Check Solana CLI
    if ! command -v solana &> /dev/null; then
        log_error "Solana CLI not found. Please install it first."
        log_info "Run: sh -c \"\$(curl -sSfL https://release.solana.com/v1.16.0/install)\""
        exit 1
    fi
    
    # Check Rust
    if ! command -v rustc &> /dev/null; then
        log_error "Rust not found. Please install it first."
        log_info "Run: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
        exit 1
    fi
    
    # Check Anchor (if using Anchor framework)
    if [ "$FRAMEWORK" = "anchor" ] && ! command -v anchor &> /dev/null; then
        log_error "Anchor CLI not found. Please install it first."
        log_info "Run: npm install -g @coral-xyz/anchor-cli"
        exit 1
    fi
    
    # Check So Lang compiler
    if [ ! -f "$SOLANG_COMPILER" ]; then
        log_error "So Lang Solana compiler not found at $SOLANG_COMPILER"
        log_info "Run: make -f Makefile.solana solana-compiler"
        exit 1
    fi
    
    log_success "All dependencies found"
}

check_solana_config() {
    log_step "Checking Solana Configuration"
    
    local current_cluster
    current_cluster=$(solana config get | grep "RPC URL" | awk '{print $3}')
    
    case $CLUSTER in
        localhost)
            expected_url="http://localhost:8899"
            ;;
        devnet)
            expected_url="https://api.devnet.solana.com"
            ;;
        mainnet-beta)
            expected_url="https://api.mainnet-beta.solana.com"
            ;;
        *)
            log_error "Invalid cluster: $CLUSTER"
            exit 1
            ;;
    esac
    
    if [ "$current_cluster" != "$expected_url" ]; then
        log_warning "Current cluster ($current_cluster) doesn't match target ($expected_url)"
        if [ "$AUTO_CONFIRM" = false ]; then
            read -p "Switch to $CLUSTER cluster? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                log_info "Keeping current cluster configuration"
            else
                solana config set --url "$CLUSTER"
                log_success "Switched to $CLUSTER cluster"
            fi
        else
            solana config set --url "$CLUSTER"
            log_success "Switched to $CLUSTER cluster"
        fi
    else
        log_success "Already configured for $CLUSTER cluster"
    fi
    
    # Check wallet
    local wallet_path
    wallet_path=$(solana config get | grep "Keypair Path" | awk '{print $3}')
    
    if [ ! -f "$wallet_path" ]; then
        log_error "Wallet keypair not found at $wallet_path"
        log_info "Run: solana-keygen new"
        exit 1
    fi
    
    # Check balance for non-localhost clusters
    if [ "$CLUSTER" != "localhost" ]; then
        local balance
        balance=$(solana balance --url "$CLUSTER" 2>/dev/null || echo "0")
        
        if (( $(echo "$balance" | cut -d' ' -f1 | awk '{print ($1 < 0.1)}') )); then
            log_warning "Low SOL balance: $balance"
            if [ "$CLUSTER" = "devnet" ]; then
                log_info "You can get devnet SOL with: solana airdrop 2 --url devnet"
            fi
        else
            log_success "Wallet balance: $balance"
        fi
    fi
}

start_local_validator() {
    if [ "$CLUSTER" = "localhost" ]; then
        log_step "Starting Local Validator"
        
        # Check if validator is already running
        if pgrep -f "solana-test-validator" > /dev/null; then
            log_success "Local validator already running"
        else
            log_info "Starting local Solana validator..."
            solana-test-validator --reset --quiet &
            VALIDATOR_PID=$!
            
            # Wait for validator to start
            log_info "Waiting for validator to start..."
            sleep 5
            
            # Verify it's running
            if solana cluster-version --url localhost &> /dev/null; then
                log_success "Local validator started successfully"
            else
                log_error "Failed to start local validator"
                exit 1
            fi
        fi
    fi
}

generate_keypairs() {
    log_step "Setting Up Program Keypairs"
    
    mkdir -p "$KEYPAIRS_DIR"
    
    local programs=()
    if [ -n "$PROGRAM_NAME" ]; then
        programs=("$PROGRAM_NAME")
    else
        # Auto-detect programs from examples directory
        for file in "$EXAMPLES_DIR"/*.so; do
            if [ -f "$file" ]; then
                programs+=($(basename "$file" .so))
            fi
        done
    fi
    
    for program in "${programs[@]}"; do
        local keypair_file="$KEYPAIRS_DIR/${program}-keypair.json"
        
        if [ ! -f "$keypair_file" ]; then
            log_info "Generating keypair for $program..."
            solana-keygen new --no-passphrase --outfile "$keypair_file"
            log_success "Generated keypair: $keypair_file"
        else
            log_info "Keypair already exists: $keypair_file"
        fi
        
        # Show program ID
        local program_id
        program_id=$(solana-keygen pubkey "$keypair_file")
        log_info "Program ID for $program: $program_id"
    done
}

compile_programs() {
    if [ "$SKIP_BUILD" = true ]; then
        log_warning "Skipping compilation (--skip-build specified)"
        return
    fi
    
    log_step "Compiling So Lang Programs"
    
    mkdir -p "$BUILD_DIR"
    
    local programs=()
    if [ -n "$PROGRAM_NAME" ]; then
        programs=("$PROGRAM_NAME")
    else
        for file in "$EXAMPLES_DIR"/*.so; do
            if [ -f "$file" ]; then
                programs+=($(basename "$file" .so))
            fi
        done
    fi
    
    for program in "${programs[@]}"; do
        local source_file="$EXAMPLES_DIR/${program}.so"
        
        if [ ! -f "$source_file" ]; then
            log_warning "Source file not found: $source_file"
            continue
        fi
        
        log_info "Compiling $program ($FRAMEWORK)..."
        
        if [ "$FRAMEWORK" = "anchor" ]; then
            "$SOLANG_COMPILER" "$source_file" --anchor --output "$BUILD_DIR/${program}.rs"
            
            # Create Anchor project structure
            local anchor_dir="$BUILD_DIR/anchor_projects/$program"
            mkdir -p "$anchor_dir/programs/$program/src"
            mkdir -p "$anchor_dir/tests"
            
            # Copy generated Rust code
            cp "$BUILD_DIR/${program}.rs" "$anchor_dir/programs/$program/src/lib.rs"
            
            # Create Anchor.toml
            cat > "$anchor_dir/Anchor.toml" << EOF
[features]
seeds = false
skip-lint = false

[programs.localnet]
$program = "$(solana-keygen pubkey "$KEYPAIRS_DIR/${program}-keypair.json")"

[programs.devnet]
$program = "$(solana-keygen pubkey "$KEYPAIRS_DIR/${program}-keypair.json")"

[programs.mainnet-beta]
$program = "$(solana-keygen pubkey "$KEYPAIRS_DIR/${program}-keypair.json")"

[registry]
url = "https://api.apr.dev"

[provider]
cluster = "$CLUSTER"
wallet = "$(solana config get | grep "Keypair Path" | awk '{print $3}')"

[scripts]
test = "yarn run ts-mocha -p ./tsconfig.json -t 1000000 tests/**/*.ts"
EOF
            
            # Create Cargo.toml
            cat > "$anchor_dir/programs/$program/Cargo.toml" << EOF
[package]
name = "$program"
version = "0.1.0"
description = "Generated by So Lang"
edition = "2021"

[lib]
crate-type = ["cdylib", "lib"]
name = "$program"

[dependencies]
anchor-lang = "0.28.0"
anchor-spl = "0.28.0"
EOF
            
            # Build with Anchor
            cd "$anchor_dir"
            anchor build --program-name "$program"
            cd "$PROJECT_ROOT"
            
        else
            # Native Solana
            "$SOLANG_COMPILER" "$source_file" --native --output "$BUILD_DIR/${program}.rs"
            
            local native_dir="$BUILD_DIR/native_programs/$program"
            mkdir -p "$native_dir/src"
            
            cp "$BUILD_DIR/${program}.rs" "$native_dir/src/lib.rs"
            
            # Create Cargo.toml for native
            cat > "$native_dir/Cargo.toml" << EOF
[package]
name = "$program"
version = "0.1.0"
description = "Generated by So Lang - Native Solana"
edition = "2021"

[lib]
crate-type = ["cdylib"]
name = "$program"

[dependencies]
solana-program = "1.16"
spl-token = "4.0"
spl-associated-token-account = "2.0"
bytemuck = "1.14"
EOF
            
            # Build with cargo
            cd "$native_dir"
            cargo build-bpf
            cd "$PROJECT_ROOT"
        fi
        
        log_success "Compiled $program successfully"
    done
}

deploy_programs() {
    log_step "Deploying Programs"
    
    local programs=()
    if [ -n "$PROGRAM_NAME" ]; then
        programs=("$PROGRAM_NAME")
    else
        for file in "$EXAMPLES_DIR"/*.so; do
            if [ -f "$file" ]; then
                programs+=($(basename "$file" .so))
            fi
        done
    fi
    
    for program in "${programs[@]}"; do
        log_info "Deploying $program..."
        
        local keypair_file="$KEYPAIRS_DIR/${program}-keypair.json"
        local program_id
        program_id=$(solana-keygen pubkey "$keypair_file")
        
        if [ "$FRAMEWORK" = "anchor" ]; then
            local anchor_dir="$BUILD_DIR/anchor_projects/$program"
            cd "$anchor_dir"
            
            # Deploy with Anchor
            anchor deploy --program-name "$program" --program-keypair "$keypair_file"
            
            cd "$PROJECT_ROOT"
            
        else
            # Native deployment
            local so_file="$BUILD_DIR/native_programs/$program/target/deploy/${program}.so"
            
            if [ ! -f "$so_file" ]; then
                log_error "Compiled program not found: $so_file"
                continue
            fi
            
            solana program deploy "$so_file" --keypair "$keypair_file" --url "$CLUSTER"
        fi
        
        log_success "Deployed $program to $CLUSTER"
        log_info "Program ID: $program_id"
    done
}

run_tests() {
    if [ "$SKIP_TESTS" = true ]; then
        log_warning "Skipping tests (--skip-tests specified)"
        return
    fi
    
    log_step "Running Tests"
    
    if [ "$FRAMEWORK" = "anchor" ]; then
        local programs=()
        if [ -n "$PROGRAM_NAME" ]; then
            programs=("$PROGRAM_NAME")
        else
            for file in "$EXAMPLES_DIR"/*.so; do
                if [ -f "$file" ]; then
                    programs+=($(basename "$file" .so))
                fi
            done
        fi
        
        for program in "${programs[@]}"; do
            local anchor_dir="$BUILD_DIR/anchor_projects/$program"
            
            if [ -d "$anchor_dir" ]; then
                log_info "Testing $program..."
                cd "$anchor_dir"
                
                if [ -f "package.json" ]; then
                    anchor test --skip-local-validator
                else
                    log_warning "No tests found for $program"
                fi
                
                cd "$PROJECT_ROOT"
            fi
        done
    else
        log_info "Native Solana programs - basic integration test"
        # For native programs, we could run basic RPC calls to verify deployment
        
        for program in "${programs[@]}"; do
            local program_id
            program_id=$(solana-keygen pubkey "$KEYPAIRS_DIR/${program}-keypair.json")
            
            log_info "Verifying $program deployment..."
            if solana account "$program_id" --url "$CLUSTER" &> /dev/null; then
                log_success "$program is deployed and accessible"
            else
                log_error "$program deployment verification failed"
            fi
        done
    fi
}

cleanup() {
    if [ "$CLUSTER" = "localhost" ] && [ -n "$VALIDATOR_PID" ]; then
        log_info "Stopping local validator..."
        kill $VALIDATOR_PID 2>/dev/null || true
    fi
}

# Set trap for cleanup
trap cleanup EXIT

# Main deployment flow
main() {
    log_step "So Lang Solana Deployment"
    log_info "Cluster: $CLUSTER"
    log_info "Framework: $FRAMEWORK"
    log_info "Program: ${PROGRAM_NAME:-"all programs"}"
    
    if [ "$AUTO_CONFIRM" = false ]; then
        echo
        read -p "Continue with deployment? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Deployment cancelled"
            exit 0
        fi
    fi
    
    check_dependencies
    check_solana_config
    start_local_validator
    generate_keypairs
    compile_programs
    deploy_programs
    run_tests
    
    log_step "Deployment Complete!"
    log_success "All programs deployed successfully to $CLUSTER"
    
    if [ "$CLUSTER" = "localhost" ]; then
        log_info "Local validator is running at http://localhost:8899"
        log_info "To stop it manually: pkill solana-test-validator"
    fi
    
    # Show program IDs
    log_info "Program IDs:"
    for file in "$EXAMPLES_DIR"/*.so; do
        if [ -f "$file" ]; then
            local program
            program=$(basename "$file" .so)
            local program_id
            program_id=$(solana-keygen pubkey "$KEYPAIRS_DIR/${program}-keypair.json" 2>/dev/null || echo "N/A")
            echo "  $program: $program_id"
        fi
    done
}

# Run main function
main "$@"