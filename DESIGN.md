# M5 Design

1. **M4 fixes:** The supplied M4 implementation passed the included M4 checks; no known M4 functional fix was needed.

2. **Threads and shared data:** Threads are the GUI thread and the single `worker_loop()` thread. The only mutable data both can reach is:

   | Data | Threads and protection | Why safe |
   |---|---|---|
   | `pending_build_`, `worker_stopping_` | GUI and worker; every read/write is under `worker_mutex_`. | The mutex protects request publication/removal and the condition-variable predicate. The worker waits on `worker_condition_` with that mutex, so it cannot miss a request or shutdown notification. |
   | `BuildRequest::cancelled` atomic | GUI and worker; `std::atomic<bool>`. | GUI sets it on Reset, Workspace add, or shutdown; worker checks it between documents. Each request has its own token, so a later build cannot clear an earlier cancellation. |
   | `BuildRequest::workspace`, `version`, `generation`, `delay_ms` | GUI initializes before publishing under `worker_mutex_`; worker reads after taking the request under the same mutex. | These fields are immutable after publication; mutex unlock/lock publishes their initialized values. |
   | Build result captured by queued callback | Worker creates and queues it; GUI callback consumes it. | Qt queued invocation transfers delivery to the GUI thread; the worker makes no further access to the result after posting. |

   `worker_condition_` is a synchronization primitive, not separately mutable application data. `active_cancel_` is GUI-thread-only. Widgets, `Workspace`, `ProcessingCore`, state objects, and lifecycle counters are GUI-thread-only. The worker never sees a state object.

3. **States:**

   | State | Meaning and events |
   |---|---|
   | `ReadyState` | No current corpus and no build; Build enters `BuildingState`; Workspace change or Reset keeps it Ready. |
   | `BuildingState` | A build is active; Build is ignored; add or Reset cancels and enters Ready; current matching success enters `CurrentState`; failure returns to Ready. |
   | `CurrentState` | Corpus matches Workspace; Build enters Building while retaining the current corpus; add or Reset cancels if needed and enters Ready; successful rebuild remains Current. |

4. **Risk sequence:** GUI `request_build()` publishes a snapshot and token, then notifies the worker. Worker `worker_loop()` waits with `worker_condition_`, checks the token between prefix rebuilds, and stops before processing the next document. GUI Reset calls `BuildingState::reset()` → `cancel_worker_build()`, invalidating the generation and token. Any queued completion has the old generation and cannot install a corpus. GUI can request a new build immediately.

5. **Decisions:** Build during a build is disabled and duplicate requests are ignored by `BuildingState::build()`. A local or `aiws_send` add follows `add_document_file()`, invalidates the corpus, and cancels the active request; the UI reports cancellation and hides stale results. Shared data is limited to the condition-variable predicate/request handoff, the immutable request snapshot, and its atomic cancellation token; they are required to notify, transfer work, and stop work safely.

6. **Faults and tests:** `AIWS_FAULT_NO_CANCEL` is in the worker's per-document cancellation check; the Reset-during-build test expects a following build to finish promptly. `AIWS_FAULT_SILENT_SHUTDOWN` is in `stop_worker()`'s idle wake path; the idle-destruction test bounds shutdown.

7. **AI-use disclosure:** AI assistance was used to implement and document M5. I reviewed the code and tests and will explain the design in my own words in the walkthrough.
