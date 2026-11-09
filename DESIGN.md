# M4 Decision Record

## 1. Threads and ownership

`MainWindow` and all Qt widgets run on the GUI thread. `MainWindow::build_corpus()` copies the current `Workspace`, captures the current workspace version and build generation, then starts one owned `std::thread`. The worker constructs and rebuilds a temporary `ProcessingCore`; it does not access widgets or the live `Workspace`. On success it posts a queued callback with the completed core. The callback runs on the GUI thread and moves the core into `MainWindow::processing_core_`. `MainWindow::~MainWindow()` joins the owned thread before its members or child widgets are destroyed. The delay is applied in `ProcessingCore::rebuild()` once per document, in the worker.

Constraint 5: cross-thread mutable/shared state is limited to the following. The worker reads its copied `Workspace` snapshot; after the GUI creates the copy, it is not mutated. The worker creates a local `ProcessingCore`, then queued delivery transfers ownership to the GUI thread, which becomes its only accessor. `workspace_version_` is incremented and read by GUI code; the worker receives its value by copy and the GUI compares it on completion. `build_generation_`, `build_running_`, live `Workspace`, installed `ProcessingCore`, and widgets are GUI-thread-only. The callback and its result are transferred by Qt's queued invocation, so GUI widget/state access occurs on the GUI thread.

## 2. When a completed build is valid

`MainWindow::finish_build()` accepts a result only if its captured generation is still current and its captured `workspace_version_` equals the live version. Adding a document and Reset invalidate the corpus and advance the version/generation; the previous corpus is not available to Search, Term Inspector, or Context while the workspace is newer. A valid result is installed and its chunk count displayed. A failed worker build is delivered to `build_failed()`, which updates the status and shows a nonmodal warning so the user can continue working.

## 3. Changes since the checkpoint

The supplied `DESIGN_NOTE.md` is still the original checkpoint placeholder, so there is no detailed checkpoint design to compare against. The implemented design adds an owned worker thread and queued result delivery, workspace version and build generation checks, per-document delay, failure reporting, the required `MainWindow::add_document_file()` and `set_build_delay_ms()` APIs, and `--build-delay-ms` parsing in `main.cpp`. `CMakeLists.txt` now builds `ai_workspace_m4` and registers the `student_tests` CTest target. The M1/M2 `ProcessingCore` API remains unchanged.

## 4. CR-1 impact and wire format

`MainWindow::start_ipc_server()` creates the owned `IpcServer` and supplies a handler that queues `MainWindow::add_document_file()` on the GUI thread. `main()` parses `--ipc-socket PATH`, starts the server before showing the window, and reports startup errors. The existing file-add function remains the single UI update and corpus invalidation path, so local and remote adds have the same R2 behavior. `MainWindow::~MainWindow()` stops the IPC server before joining a build worker. `src/ipc_server.cpp` owns the POSIX accept/read loop; it validates file readability and sends the reply directly on its own thread without waiting for the GUI. The new `tools/aiws_send.cpp` implements the companion client. No change to M1/M2 processing interfaces or search/context functions was needed because IPC adds documents through the existing coordinator API.

The wire request is: four ASCII bytes `AIWS`; a 32-bit unsigned path byte length in network byte order; then exactly that many UTF-8 path bytes (NUL bytes are rejected). The response is: one status byte (`0` accepted, `1` rejected), a 32-bit unsigned UTF-8 message length in network byte order, then that many message bytes. The server caps request paths at 1 MiB; the client caps response text at 64 KiB. The client uses a 2-second connect deadline and a 5-second total send/receive deadline. A request is validated and queued before the server returns success; the GUI callback performs the actual widget/workspace mutation asynchronously.

## 5. Fault switches and tests

`AIWS_FAULT_ACCEPT_STALE` is in `MainWindow::invalidate_corpus()` and `MainWindow::finish_build()`: it suppresses invalidation-generation changes and bypasses the captured workspace-version check, allowing an old build to become current. The delayed add/Reset case in `tests/student_tests.cpp` verifies that the old corpus does not reappear after Reset; it should fail in the stale-acceptance fault build.

`AIWS_FAULT_LOSE_FAILURE` suppresses worker-to-GUI delivery in the catch path of `MainWindow::build_corpus()`; `build_failed()` also returns without changing state when the macro is enabled. The duplicate-document-ID test in `tests/student_tests.cpp` expects the normal build to leave the building state and report failure; the fault build now fails that test because it remains in the building state.

The `student_tests` suite covers normal completion, Reset and file-add behavior during a delayed build, failure delivery, and on POSIX, an incomplete client disconnect followed by a fragmented valid request, a path containing spaces/non-ASCII/newline characters through `aiws_send`, and socket-file cleanup. The normal CTest suite was run successfully before CR-1; the CR-1 additions and macro variants still need to be run on POSIX.

## 6. AI-use disclosure

AI assistance was used to implement and document portions of M4.

– Partner's Name: Akanksha Potluri
– Date: 11/8/2026
Start time: 7PM

Time of each session:
-Akanksha's Session: 7pm - 7:30pm (switched at 7:15pm)
    -(NAVIGATOR, Kendra) I verified both the document count and the sender’s exit code to confirm that only the complete request resulted in a document being added.
    -(DRIVER, Akanksha) Wrote a test (based on E3 by simulating a client that disconnects before completing a request) that checks the server's ability to handle an abandoned client connection and then successfully process a second client later complete request, resulting in exactly one document being added.


-My (Kendra) Session: 7:30pm - 8pm (switched at 7:45pm)
    -(NAVIGATOR, Akanksha) Suggested validating both the sender's exit code and the final document count to confirm that the path was processed correctly. And made sure that it followed E1 and E2 required behavior.
    -(DRIVER, Kendra) Added a test where aiws_send submits a document whose path contains spaces, a UTF‑8 character, and a newline. The test verifies that the request succeeds and that the document is added to the workspace.

-One sentence on what kind of interaction worked best for your pair:
    -Regularly reviewing each other's work while coding helped us find bugs, validate requirements more effectively and find edge cases in testing with the required behavior on cr-1.