# External VST3 hosting in the TONE3000 chain

Status: design / implementation plan. This document does not claim that VST3 hosting is implemented yet.

## Goal

Allow a user to place a third-party VST3 effect (for example, an EQ) in the same ordered processing chain as existing NAM and IR blocks:

`Input → NAM → IR → External VST3 EQ → Output`

The feature should work in the Windows VST3 build hosted by REAPER and, where JUCE can discover and load the same installed VST3s, in the Standalone build.

## Findings from the current code

- The project is a JUCE 9.0.3 C++ application/plugin built by CMake.
- The chain model is currently `ChainBlockType { NAM, IR, INSERT }` in `plugin/include/ChainBlock.h`.
- `INSERT` is an empty UI/chain slot, not an instantiated third-party plugin.
- `ChainBlock` owns the current NAM/IR engine and per-block settings. Chain state and history are implemented across `ProcessorChain.cpp`, `ProcessorState.cpp`, `ProcessorHistory.cpp` and preset serialization.
- The project already links JUCE audio processor modules, but the inspected chain code does not yet provide an external VST3 processor instance or plugin-description persistence.

## Proposed implementation stages

1. **Hosting spike:** add a small JUCE-based manager that scans installed VST3 plug-ins, lists descriptions, creates an instance off the audio thread, and reports load/prepare errors clearly. Keep this isolated until it builds.
2. **Runtime block:** add an external-plugin block representation that owns a JUCE `AudioPluginInstance`, prepares it for the chain sample rate and block size, and processes audio without allocations, blocking locks, file I/O, or UI work on the audio callback.
3. **Selection and UI:** allow an empty insert slot to offer “Load VST3…”, show the selected plug-in name, and expose the plug-in's own editor when supported. UI/editor lifecycle must remain on JUCE's message thread.
4. **Persistence and undo:** serialize a stable plug-in identifier/description plus the plug-in's opaque state blob. Restore missing plug-ins as a bypassed/error block rather than breaking the whole session. Integrate add/remove/reorder/duplicate, undo/redo, preset/state save, and both stereo lanes.
5. **Audio correctness:** verify channel layouts, MIDI policy, plug-in bypass/failure behavior, reported latency and tail, sample-rate changes, oversized host blocks, and transitions that must avoid clicks. Update host latency when a loaded plug-in changes it.
6. **Validation:** compile the Windows VST3 and Standalone targets; test a known VST3 EQ in REAPER and Standalone; reopen a saved session with the plug-in installed and with it unavailable; exercise mono/stereo chains and rapid add/remove/reorder.

## Important constraints

- Never instantiate, scan, destroy, or open a plug-in editor from the real-time audio callback.
- Do not assume every VST3 supports stereo, arbitrary bus layouts, MIDI, or a custom editor.
- Plug-in state can be large and must not be copied or serialized from the audio thread.
- A plug-in installed on one machine is not automatically available on another. Persist identity and state, not the binary itself.
- VST3 hosting support and plug-in availability need separate handling: a failed/missing plug-in should not crash the host or invalidate the rest of the chain.
- The Standalone target must be tested separately; success inside REAPER does not prove Standalone discovery or editor behavior.

## First implementation milestone

Deliver the discovery/instantiate/prepare/process/save-state path for one external VST3 effect before adding the complete picker/editor UX. Keep changes on `feature/vst3-hosting` until the Windows build and smoke tests pass.
