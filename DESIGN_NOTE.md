# M4 Design Note

Due Wed Oct 28, 11:59 pm. See specification Section 7.
# Design Document

## 1. Components and Thread Assignment

The system consists of four primary components:

| Component | Responsibility | Thread |
|------------|---------------|----------|
| MainWindow / Workspace UI | Displays documents, corpus status, and build status; handles user actions | GUI Thread |
| Corpus Builder | Builds the corpus from the current document set | Worker Thread |
| IPC Server | Listens for remote add-document requests through a Unix-domain socket | IPC Thread |
| aiws_send | External client process that sends document-add requests | Separate Process |

The GUI thread owns all Qt widgets and application state visible to the user. The corpus builder runs on a worker thread so that building does not block the interface. The IPC server runs on a dedicated `std::thread` and communicates accepted requests back to the GUI thread.

---

## 2. Corpus States and State Transitions

The application maintains three corpus states:

### Not Built

The corpus is missing or invalid.

**Entered when:**

- The application starts.
- Reset is selected.
- A document is added after a successful build.

### Building

A corpus build is currently running.

**Entered when:**

- The user selects **Build Corpus**.

**Exited when:**

- The build succeeds.
- The build fails.

### Built

A valid corpus exists and can be searched.

**Entered when:**

- A build completes successfully.

**Exited when:**

- A document is added.
- Reset is selected.

### State Diagram

```text
                Build Corpus
Not Built --------------------------> Building
    ^                                   |
    |                                   |
    | Reset / Add Document              | Success
    |                                   v
    +----------------------------- Built

                Add Document
                      |
                      v
                 Not Built
```

---

## 3. Event Sequences Threatening R1-R4

### Threat 1: Document Added During Build

**Sequence**

1. User starts a build.
2. Builder thread begins creating a corpus.
3. Another document is added.
4. The document set no longer matches the corpus being built.

**Handling**

The corpus is invalidated and the application returns to the **Not Built** state. A new build is required before the corpus can be used.

### Threat 2: IPC Request During Build

**Sequence**

1. The user starts a build.
2. The IPC server receives an add-document request.
3. The request modifies the document collection.

**Handling**

The request is forwarded safely to the GUI thread. The corpus is marked invalid because the document set has changed.

### Threat 3: Client Disconnects Mid-Request

**Sequence**

1. A client connects to the socket.
2. The client sends only part of a request.
3. The client disconnects before completion.

**Handling**

The IPC thread detects that the message is incomplete and discards it without changing application state.

### Threat 4: Application Shutdown During IPC Activity

**Sequence**

1. The user exits the application.
2. The server stops accepting connections.
3. Existing work is allowed to finish.
4. The socket file is removed.
5. The IPC thread terminates.

**Handling**

This prevents requests from accessing objects that are being destroyed.

### Detailed Walkthrough A: Partial Request Recovery

1. Client A connects.
2. Client A sends only part of a request.
3. Client A disconnects.
4. The IPC server detects an incomplete message.
5. The request is discarded.
6. Client B connects.
7. Client B sends a valid request.
8. The server processes the request successfully.
9. Exactly one document is added.

This ensures malformed requests cannot corrupt server state.

### Detailed Walkthrough B: Document Added During Build

1. A build begins using the current document collection.
2. The builder thread starts processing documents.
3. A new document is added.
4. The application marks the corpus as invalid.
5. The build result is no longer considered current.
6. The corpus status returns to **Not Built**.
7. The user must rebuild before searching.

This guarantees that every built corpus corresponds to the document set currently displayed.

---

## 4. Ownership Path of a Built Corpus

The lifetime of a corpus follows this ownership chain:

```text
Corpus Builder Creates Corpus
              |
              v
      std::unique_ptr<Corpus>
              |
              v
  Ownership transferred to GUI
              |
              v
   Stored as current corpus
              |
              v
      Used by application
              |
              v
 Replaced by rebuild or reset
              |
              v
 Automatically destroyed
```

Using `std::unique_ptr` ensures that exactly one component owns the corpus at any time and that memory is released automatically when the corpus is replaced or discarded.

---

## 5. Rejected Alternative

One alternative was to perform IPC operations on the GUI thread using blocking socket calls.

This approach was rejected because calls such as `accept()` and `read()` could block the event loop and freeze the user interface while waiting for client requests. It would also make it more difficult to satisfy responsiveness requirements during builds and shutdown.

The final design instead places all socket operations on a dedicated IPC thread and communicates accepted requests back to the GUI thread, allowing the interface to remain responsive at all times.