# 🧮 Matrix Operations — Multi-Processing System
> Real-Time Applications & Embedded Systems | ENCS4330 | Birzeit University

A Linux-based multi-processing application that performs matrix operations (addition, subtraction, multiplication, determinant, eigenvalues & eigenvectors) using **signals**, **pipes**, and **FIFOs**, with optional **OpenMP** parallelization.

---

## 📁 Project Structure

```
.
├── src/                   # C source files
│   ├── main.c             # Entry point, process initialization
│   ├── parent.c           # Parent process logic & operation dispatch
│   ├── menu.c             # Interactive menu (runs in its own process)
│   ├── child.c            # Worker child process logic
│   ├── pool_manager.c     # Child process pool management
│   ├── pool_child.c       # Pool child helpers
│   ├── matrix_ops.c       # Core matrix operations
│   ├── pipes.c            # Pipe setup & IPC utilities
│   ├── signals.c          # Signal handlers
│   └── functions.c        # General utility functions
├── include/               # Header files
│   ├── header.h
│   ├── config.h
│   ├── constants.h
│   ├── functions.h
│   ├── menu.h
│   ├── operations.h
│   ├── pipes.h
│   ├── pool_manager.h
│   └── signals.h
├── data/
│   ├── config.txt         # App configuration & matrix directory path
│   ├── matrices/          # Sample matrix files (.txt)
│   └── output/            # Output saved matrices
└── Makefile
```

---

## ⚙️ Requirements

- GCC with OpenMP support (`libgomp`)
- A Linux / Unix environment — any of the following work:
  - **WSL** (Windows Subsystem for Linux) with VS Code
  - Native Linux
  - macOS with GCC installed via Homebrew

---

## 🔨 Build & Run

### 1. Clone the repository

```bash
git clone https://github.com/diaabadaha/Matrix-Operations-MultiProcessing.git
cd Matrix-Operations-MultiProcessing
```

### 2. Install dependencies (if needed)

On Ubuntu / WSL:
```bash
sudo apt update
sudo apt install gcc make libgomp1
```

On macOS:
```bash
brew install gcc
```

### 3. Build

```bash
make
```

### 4. Run

```bash
./run_project
```

Or pass a custom config file as an argument:
```bash
./run_project data/config.txt
```

### 5. Clean build artifacts

```bash
make clean
```

---

## 💻 Running on WSL with VS Code

1. Open VS Code → press `Ctrl+Shift+P` → search **"WSL: Open Folder in WSL"**
2. Navigate to the cloned project folder
3. Open the integrated terminal with `Ctrl+` `` ` ``
4. Run the build and run commands above directly in the terminal

> Make sure the **WSL** and **C/C++** extensions are installed in VS Code.

---

## 🖥️ Features & Menu

When launched, the application displays an interactive menu:

| # | Operation |
|---|-----------|
| 1 | Enter a matrix |
| 2 | Display a matrix |
| 3 | Delete a matrix |
| 4 | Modify a matrix (row / column / element) |
| 5 | Read a matrix from a file |
| 6 | Read all matrices from a folder |
| 7 | Save a matrix to a file |
| 8 | Save all matrices to a folder |
| 9 | Display all matrices in memory |
| 10 | Add 2 matrices |
| 11 | Subtract 2 matrices |
| 12 | Multiply 2 matrices |
| 13 | Find the determinant of a matrix |
| 14 | Find eigenvalues & eigenvectors |
| 15 | Exit |

---

## 🔀 Architecture

The system uses a **multi-process architecture** with IPC:

- **Main process** — initializes the child pool and coordinates everything.
- **Menu process** — runs the interactive UI in a forked child, communicates via pipes.
- **Worker pool** — a pre-spawned pool of child processes ready to handle computations, with aging to retire idle workers.

### IPC Strategy

| Mechanism | Usage |
|-----------|-------|
| **Pipes** | Menu ↔ Parent communication; dispatching tasks to workers |
| **FIFOs** | Named communication channels between processes |
| **Signals** | Triggering operations, termination, and synchronization |
| **OpenMP** | Loop-level parallelism inside matrix operations |

### Parallelism

- **Addition / Subtraction** — one child process per matrix element.
- **Multiplication** — one child process per output element (row × column pair).
- **Determinant / Eigenvalues** — multiple children for sub-computations.
- **OpenMP** — used inside operations to further parallelize loops.

Execution time is compared between multi-process and single-threaded runs.

---

## 📄 Config File

The `data/config.txt` file allows you to specify:
- The directory to auto-load matrices from on startup
- Custom menu ordering preferences

Pass it as an argument: `./run_project data/config.txt`

---

## 📚 Course Info

- **Course:** ENCS4330 — Real-Time Applications & Embedded Systems
- **Institution:** Birzeit University
- **Instructor:** Dr. Hanna Bullata
- **Semester:** 1st Semester 2025/2026
