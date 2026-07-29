# Chronos Exchange

A simulated stock exchange with a C++ matching engine, FastAPI backend, MySQL database, and React frontend. Built as a 4th semester DBMS project.

---

## Stack

| Layer | Tech |
|---|---|
| Frontend | React 19 + Vite + Tailwind (port 5173) |
| Backend | FastAPI + SQLAlchemy + PyMySQL (port 8000) |
| Matching engine | C++17 subprocess, JSON IPC over stdin/stdout |
| Database | MySQL 8 (`chronos_db`) |

---

## Prerequisites

- **WSL Ubuntu 24.04** (or native Linux)
- **Python 3.11+** with pip
- **Node.js 22+** with npm
- **MySQL 8** running locally
- **g++ with C++17 support**

---

## Setup

### 1. Clone the repo

```bash
git clone https://github.com/ParthXplorer/chronos.git
cd chronos
```

### 2. Database

```bash
mysql -u root -p
```

```sql
CREATE DATABASE chronos_db;
EXIT;
```

```bash
# Import schema and seed data (filter out CREATE DATABASE / USE lines)
grep -v -E "^CREATE DATABASE|^USE " database/schema.sql | mysql -u root -p chronos_db
```

Then apply triggers:

```bash
mysql -u root -p chronos_db < database/triggers.sql
```

### 3. Backend

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

Create `backend/.env`:

```env
DB_USER=root
DB_PASSWORD=your_password
DB_HOST=127.0.0.1
DB_PORT=3306
DB_NAME=chronos_db
JWT_SECRET=your_secret_key
JWT_ALGORITHM=HS256
JWT_EXPIRE_MINUTES=60
ALLOWED_ORIGINS=http://localhost:5173,http://127.0.0.1:5173
```

### 4. Matching engine (C++)

```bash
cd engine
mkdir -p bin
g++ -std=c++17 -O2 -Iinclude src/main.cpp src/MatchingEngine.cpp -o bin/chronos_engine
```

> Each team member must build the binary locally — `engine/bin/` is gitignored.

### 5. Frontend

```bash
cd frontend
npm install
```

---

## Running

Open three terminals:

**Terminal 1 — Backend**

```bash
cd backend
source venv/bin/activate
uvicorn main:app --reload
```

**Terminal 2 — Frontend**

```bash
cd frontend
npm run dev
```

**Terminal 3 — (optional) Engine test**

```bash
# From repo root
python3 test_engine.py
python3 test_reload.py
```

Then open **http://localhost:5173** in your browser.

---

## Benchmarks

Two benchmarks measure order latency at different layers. Neither needs the frontend or the database.

**Matching engine only** — times `submitOrder()` in isolation over 1M+ orders:

```bash
cd engine
g++ -std=c++17 -O2 -Iinclude src/bench.cpp src/MatchingEngine.cpp -o bin/bench
./bin/bench
```

**Full IPC roundtrip** — times Python → JSON → pipe → C++ → back, excluding the DB. Needs `bin/chronos_engine` already built:

```bash
cd backend
source venv/bin/activate
python bench_ipc.py
```

Results are hardware-dependent. On our machines the matching itself is sub-microsecond, while the JSON-over-pipe IPC adds roughly 50 µs — so the serialization, not the matching algorithm, is the bottleneck.

---

## API docs

FastAPI auto-generates interactive docs at **http://localhost:8000/docs**

---

## Project structure

```
chronos/
├── backend/
│   ├── app/
│   │   ├── routers/        # auth, stocks, orders, portfolio, analytics, admin
│   │   ├── engine_bridge.py
│   │   ├── models.py
│   │   ├── schemas.py
│   │   └── database.py
│   ├── bench_ipc.py        # IPC roundtrip benchmark
│   └── main.py
├── engine/
│   ├── include/            # Types.h, OrderBook.h, MatchingEngine.h
│   ├── src/                # main.cpp, MatchingEngine.cpp, bench.cpp
│   └── bin/                # compiled binaries (gitignored)
├── frontend/
│   └── src/
│       ├── pages/          # Dashboard, MarketWatch, OrderBook, PlaceOrder, Portfolio
│       └── components/
├── database/
│   ├── schema.sql
│   ├── triggers.sql
│   └── queries.sql
├── test_engine.py
└── test_reload.py
```

---

## Team

| Member | Responsibility |
|---|---|
| Parth Choyal | Backend, matching engine, DB design |
| Divanshu Jain | WebSockets, SQL triggers/queries, frontend |
| Prabhav Singhal | React frontend |

---

## Notes

- PyMySQL requires `127.0.0.1` (not `localhost`) in `.env` for TCP connections
- `bcrypt` must be pinned to `4.0.1` for passlib compatibility — already in `requirements.txt`
- The engine binary must be compiled on each machine before starting the backend
- The backend startup will log a warning if the engine binary is not found — orders will not match until it is built
