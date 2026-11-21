# M4 Understanding Verification Study Guide

Use this to practice explaining your submitted M4 design aloud and in writing. The quiz is closed note, so focus on being able to reconstruct the reasoning and event sequence from memory. The project says `DESIGN_NOTE.md` is still only a checkpoint placeholder; `DESIGN.md` is the detailed decision record.

## What to be ready to explain

1. **A build finishes after the Workspace changes.** Explain the copied snapshot, version and generation checks, invalidation, and why an obsolete result cannot become searchable.
2. **A CR-1 message reaches the app and its threads.** Explain the sender, Unix socket server thread, queued Qt callback, GUI-thread document add, and the timing/meaning of the server reply.
3. **General multithreading analysis.** Identify a shared-state race or lifetime bug in code you have not seen before, explain a concrete interleaving, and propose a synchronization/ownership fix.

## Your design in one minute

`MainWindow` owns the live Workspace, widgets, installed `ProcessingCore`, and GUI state. When Build Corpus is clicked, `MainWindow::build_corpus()` copies `workspace_`, captures `workspace_version_` and a new `build_generation_`, then starts the owned `build_thread_`. The worker rebuilds a local temporary `ProcessingCore` from the snapshot and never reads the live Workspace or touches widgets. It sends success or failure back with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. The callback runs on the GUI thread. A successful result is installed only if its generation and workspace version still match. An add or Reset invalidates the corpus, so Search, Term Inspector, and Context cannot use an older corpus. The destructor stops IPC and joins the build worker before the window is destroyed.

For CR-1, `main()` parses `--ipc-socket PATH` and starts `IpcServer`. Its dedicated thread accepts and validates Unix-domain socket requests. The handler converts the UTF-8 path and posts a queued callback to `MainWindow`; that callback calls the same `add_document_file()` method as a local add. That GUI-thread method updates Workspace/UI, increments the version, and invalidates the corpus. The IPC thread sends its acceptance response after the request has been validated and queued; the UI mutation is asynchronous and may happen after the sender receives the response.

## Written question practice 1: stale build

**Prompt:** A build starts with Workspace version 4. While it is running, the user adds a document (or selects Reset). The old build then completes. What prevents it from replacing the corpus, and what can the user do meanwhile?

**Answer outline:**

- `build_corpus()` copied the Workspace and captured its version and generation before starting the worker. The worker only builds that snapshot.
- Adding a document increments `workspace_version_`, updates the Workspace/UI, and calls `invalidate_corpus()`. Reset clears the Workspace and increments its version. Both invalidate the installed corpus; dependent outputs are cleared and corpus-dependent actions are disabled.
- The completion arrives on the GUI thread. `finish_build()` first rejects a result with an obsolete `build_generation_`; it then compares the captured version with the current `workspace_version_`. On mismatch it discards the result, updates status, and does not install it or mark `corpus_current_` true.
- The user can continue using the interface while the worker runs, but must rebuild against the new Workspace before searching/inspecting/building context. A build for the prior Workspace can finish computationally, yet it is no longer valid.
- Generation and version checks cover different identity changes: generation distinguishes build attempts; workspace version detects content changes since the snapshot.

**A useful concrete sequence:** build captures version 4 and generation 7 → add document increments version to 5 and invalidates the corpus → build callback arrives with (4, 7) → generation may still match, but version 4 != 5 → discard.

## Written question practice 2: CR-1 message path

**Prompt:** Trace a remote add-document request from `aiws_send` to the Workspace. Which thread does each step run on, and what does a successful reply mean?

**Answer outline:**

- The client connects to the Unix-domain socket and sends `AIWS`, a 32-bit network-byte-order path length, then that many UTF-8 path bytes. The server rejects bad magic, incomplete/invalid lengths, NUL-containing paths, or unreadable/non-regular files.
- `IpcServer::run()` does accept/read/validation on its dedicated `std::thread`. It calls the provided handler; `MainWindow::start_ipc_server()`'s handler creates a `QString` and uses `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)`.
- The queued lambda executes on the GUI thread and calls `add_document_file()`. This is the shared local/remote add path: it reads the file, appends a `Document` to `workspace_`, increments `workspace_version_`, updates/selects the widget row, invalidates the corpus, clears stale processing outputs, and refreshes action states.
- The server replies on its own thread using status byte + network-order message length + UTF-8 text. “Accepted” means the valid request was queued for GUI handling, not that the GUI mutation has already completed. This avoids blocking the IPC thread while waiting for the GUI event loop.
- The GUI thread alone mutates widgets and live Workspace. The worker threads do not directly call widget methods. At shutdown, the server is stopped before the build worker is joined and window state is destroyed.

**Design reason:** synchronous/blocking socket work on the GUI thread could stall the event loop. Queuing work to the GUI also preserves Qt's thread-affinity rules and reuses the app's normal document-add/invalidation behavior.

## Code-analysis practice (unrelated example)

```cpp
int total = 0;
void add(int n) {
    for (int i = 0; i < n; ++i) {
        int old = total;
        total = old + 1;
    }
}
// Two threads call add(1000) at the same time.
```

**Find the problem:** `total` is shared mutable state with unsynchronized reads and writes. `total = old + 1` is a read-modify-write sequence, not one indivisible operation. For example, both threads can read 8, both compute 9, and both write 9; one increment is lost. In C++, the conflicting unsynchronized accesses are a data race, so behavior is undefined (not merely guaranteed to be a slightly low count).

**One fix:** protect every access with the same `std::mutex` and `std::lock_guard`, or use `std::atomic<int>::fetch_add(1)` for this simple counter. A fix must cover all readers/writers that share the variable. `volatile` does not provide synchronization. Also ask whether a lock can deadlock (inconsistent lock order / waiting while holding a needed lock) and whether shared objects outlive all threads that may access them (join/stop before destruction).

Try to answer before reading the explanation: What exactly is shared? Which operations conflict? What interleaving breaks the intended result? Is there a C++ data race? What smallest ownership or synchronization change fixes it?

## Thread-safety concepts tied to your project

- **Thread confinement:** widgets and live UI state are GUI-thread-only. Confine access instead of sprinkling locks around Qt objects.
- **Snapshotting:** the worker receives a copied Workspace, making its input stable while the GUI can continue to change the live Workspace.
- **Queued handoff:** `Qt::QueuedConnection` schedules work in the receiver object's thread; the worker returns a result rather than mutating GUI state itself.
- **Version validation:** the snapshot is useful only while its source state is current. The captured version makes that condition explicit.
- **Ownership and lifetime:** the worker owns its temporary core until queued delivery transfers it; the GUI then owns the installed core. Stop/join threads before destroying objects they could use.
- **Atomics are narrow tools:** `workspace_version_` and `build_running_` are atomic fields, but atomics do not make the Workspace, widgets, or a whole multi-step operation thread-safe. In this design, most state is safe because it is confined to one thread or copied.
- **Failure path:** worker exceptions are caught and reported with a queued `build_failed()` callback so the GUI can leave its Building state and report failure.

## Fast self-check questions

1. Why is a copied Workspace safer than letting the worker iterate the live `workspace_` while the GUI can add documents?
2. Why are both a version and generation captured? What does each invalidate?
3. Does the IPC “accepted” response prove that `workspace_.add_document()` already ran? Why not?
4. Name the one method both local and IPC document addition use, and state its important side effects.
5. Why can an old corpus not be used after adding a document even before the replacement build finishes?
6. What does `Qt::QueuedConnection` protect here, and what does it not guarantee (for example, successful file reading in the later callback)?
7. Why is joining the worker during destruction important? Why stop IPC first?
8. In unfamiliar code, distinguish a data race from a logical race/stale result, a deadlock, and a use-after-free.

## Last review checklist

- Say the stale-build timeline without notes, including both captured values and the completion checks.
- Trace a request from socket bytes through IPC thread to GUI-thread mutation; explain the asynchronous acknowledgement.
- Be ready to name which project state is thread-confined, which is copied, and where ownership moves.
- Review your submitted implementation and M4 lectures 15–20; this guide focuses on the project details in `DESIGN.md` and source code, not lecture-specific material absent from the repository.
