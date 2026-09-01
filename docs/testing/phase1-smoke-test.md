# Phase 1 interactive smoke test

Run this checklist after the automated MSVC, clang-cl, and AddressSanitizer tests pass.

## Test record

- Date: 2026-09-01
- Commit: Working tree after Phase 1 automation changes
- Tester: Codex-assisted launch; visual checks require user confirmation
- Compiler/configuration: MSVC x64 Debug
- Windows version: Local development machine
- Result: Partial — application launched and exited normally with code 0; detailed checklist pending sign-off

## Preparation

From an x64 Visual Studio Developer PowerShell:

```powershell
cmake --fresh --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
& ".\out\build\windows-msvc\apps\umbra\Debug\Umbra.exe"
```

## Checklist

- [ ] The application opens a 1280×720 window without an error message.
- [ ] Dragging the window edges updates its cached size without resize oscillation or repeated resize events.
- [ ] Pressing and releasing mapped keys does not produce invalid-key diagnostics.
- [ ] Losing focus clears held input; returning focus does not restore stale held keys.
- [ ] Minimizing or backgrounding the application suspends simulation work.
- [ ] Restoring the application resumes it without a large frame-time jump.
- [ ] Escape closes the application normally.
- [ ] The native window close button closes the application normally.
- [ ] Shutdown logs appear in this order: window destroyed, platform shut down, diagnostics shutting down.
- [ ] No duplicate shutdown, assertion, fatal-error, or crash-handler messages appear during a normal run.

Run the application again as needed to exercise both Escape and the native close button. Mark the result `Passed` only after every item succeeds; otherwise record the failed item and observed log text.
