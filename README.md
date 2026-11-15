# Concurrent TCP Client-Server \& Sliding Window Protocol

This project implements a concurrent TCP server and two client variants using POSIX sockets in C:

* **Part A:** Stop-and-Wait communication where the client sends a `"hello"` message and blocks until an `"ACK"` is received.
* **Part B:** Full-duplex Sliding Window communication (window size = 5) where the client sends up to 5 unacknowledged messages before blocking, sliding the window forward by 1 unit as each `"ACK"` arrives.

\---

## 1\. Compilation \& Execution Steps

Ensure you are on a Linux environment (or WSL on Windows) with `gcc` installed.

### Part A: Stop-and-Wait

1. **Compile both files:**

```bash
   gcc server\_a.c -o server\_a
   gcc client\_a.c -o client\_a
   ```

2. **Terminal 1 (Start the Server):**

```bash
   ./server\_a
   ```

   *Output:* `Server A running on port 9090...`

3. **Terminal 2 (Run the Client):**

```bash
   ./client\_a
   ```

   *(Optional: Pass a custom IP if running across two machines, e.g., `./client\_a 192.168.1.5`)*

\---

### Part B: Sliding Window (Window Size = 5)

1. **Compile both files** *(Note: `-pthread` is required for POSIX threads and semaphores in `client\_b.c`)*:

```bash
   gcc server\_b.c -o server\_b
   gcc client\_b.c -o client\_b -pthread
   ```

2. **Terminal 1 (Start the Server):**

```bash
   ./server\_b
   ```

   *Output:* `Server B (Sliding Window) running on port 9090...`

3. **Terminal 2 (Run the Client):**

```bash
   ./client\_b
   ```

   **What you will observe:** The client immediately blasts out `hello 1` through `hello 5`, prints `\[!] Window full (5 unACKed messages) -> Blocking...`, and then sends `hello 6` through `hello 10` one by one as each ACK returns from the server.

*(To stop either server when done, press `Ctrl + C` in Terminal 1.)*

\---

## 2\. Logical Flow: How It Works

### Server Concurrency (`fork()` + `SIGCHLD`)

* **Listening:** The server creates a TCP socket (`socket`), binds it to port `9090` (`bind`), and enters a passive listening state (`listen`).
* **Forking per Client:** Inside an infinite `while(1)` loop, `accept()` pauses execution until a client connects. Once connected, `accept()` returns a new dedicated file descriptor (`conn\_fd`). The server immediately calls `fork()`:

  * **Child Process (`pid == 0`):** Closes the main `listen\_fd` (it doesn't need to accept new clients) and handles all `recv()`/`send()` communication with that specific client before exiting.
  * **Parent Process:** Closes `conn\_fd` (the child owns it now) and loops back to `accept()` to wait for the next client. This allows multiple clients to connect simultaneously without blocking each other.
* **Zombie Prevention:** When a child process finishes, the OS keeps its exit status in the process table until the parent reads it. Registering `signal(SIGCHLD, cleanup\_children)` invokes `waitpid(-1, NULL, WNOHANG)` asynchronously whenever a child terminates, preventing zombie processes.

### Part B: Full-Duplex Sliding Window (`client\_b.c`)

* **Full-Duplex via Threads:** TCP sockets have independent send and receive buffers in the OS kernel, making them naturally full-duplex. In `client\_b.c`, the **main thread** acts exclusively as the sender (`send()`), while a spawned **background thread (`ack\_listener`)** acts exclusively as the receiver (`fgets()`), allowing incoming ACKs to be processed simultaneously while outgoing messages are being queued.
* **The Window Mechanism:**

  1. The semaphore `slots\_available` starts at `5`.
  2. Before sending each message, the sender calls `sem\_wait(\&slots\_available)`, decrementing the count (`5 -> 4 -> 3 -> 2 -> 1 -> 0`).
  3. After 5 messages are sent without an ACK, `slots\_available` hits `0`. On the 6th iteration, `sem\_wait()` puts the main thread to sleep (blocks it).
  4. Meanwhile, `server\_b` processes messages with a `400ms` simulated network/processing delay and sends back `"ACK (hello X)\\n"`.
  5. When `ack\_listener` reads an ACK, it calls `sem\_post(\&slots\_available)`, incrementing the count from `0` to `1`. This immediately wakes up the main thread to send `hello 6`—sliding the window forward by 1 unit.

\---

## 3\. Architectural Design Choices (Viva Defense)

### 1\. Counting Semaphore (`sem\_t`) vs. Mutex + Condition Variable + Counters

* **Alternative:** Tracking `base` and `next` integers guarded by a `pthread\_mutex\_t` and a `pthread\_cond\_t` (`while (next - base >= 5) pthread\_cond\_wait(...)`).
* **Why `sem\_t` was chosen:** A sliding window of size $N$ with cumulative/sequential ACKs is fundamentally a **bounded resource pool** of $N$ available transmission slots. A counting semaphore initialized to `5` encapsulates both the state counter and the blocking/waking mechanism atomically in two calls (`sem\_wait` and `sem\_post`). This eliminates manual counter arithmetic (`next - base`), prevents missed wake-up race conditions, and keeps the synchronization logic clean and concise.

### 2\. Newline-Delimited Streams (`fdopen` + `fgets` + `\\n`) vs. Fixed-Size Buffers (`MSG\_WAITALL`)

* **Alternative:** Padding every message to a fixed 32-byte array and calling `recv(..., 32, MSG\_WAITALL)`.
* **Why `\\n` delimiters were chosen:** TCP is a **byte-stream protocol**, not a message-oriented protocol—it does not preserve packet boundaries. If a client sends `"hello 1"` and `"hello 2"` rapidly, TCP may coalesce them into a single segment (`"hello 1hello 2"`). Wrapping the socket descriptor with `fdopen()` and ending every transmission with `\\n` allows `fgets()` to read cleanly up to the exact newline boundary regardless of message length, avoiding wasted bandwidth on zero-padding while keeping variable-length messages (`"hello 1"` vs `"hello 10"`) intact.

### 3\. `dup(sock\_fd)` in the Receiver Thread

* **Why it's used:** In `client\_b.c`, the receiver thread wraps the socket in a buffered `FILE\*` stream (`fdopen`) and calls `fclose()` when done. Calling `fclose()` on a `FILE\*` automatically closes its underlying OS file descriptor. Passing `dup(sock\_fd)` gives the `FILE\*` stream its own duplicated descriptor so closing the stream does not prematurely tear down `sock\_fd` before the main thread finishes cleanly.

### 4\. `SO\_REUSEADDR` via `setsockopt()`

* **Why it's used:** When a TCP server closes, the OS puts its port into a `TIME\_WAIT` state for \~60 seconds to catch stray delayed packets. Setting `SO\_REUSEADDR` lets you restart `./server\_a` or `./server\_b` immediately during testing without hitting a `bind: Address already in use` error.

