// Loads umbra_sim.wasm in a browser or in Node. The module only needs six WASI functions,
// all of them stubs here: it has no files and prints nothing (see README.md).
export async function loadSimulation(wasmBytes) {
    let memory;
    const wasi = {
        fd_write: (fd, iovs, count, written) => { new DataView(memory.buffer).setUint32(written, 0, true); return 0; },
        fd_close: () => 0,
        fd_seek: () => 70,             // ESPIPE: no seekable files
        fd_prestat_get: () => 8,       // EBADF: no preopened folders
        fd_prestat_dir_name: () => 8,
        proc_exit: (code) => { throw new Error(`simulation exited with code ${code}`); },
    };
    const { instance } = await WebAssembly.instantiate(wasmBytes, { wasi_snapshot_preview1: wasi });
    memory = instance.exports.memory;
    instance.exports._initialize(); // run C++ static constructors (reactor module)
    const e = instance.exports;

    const readString = (pointer) => {
        const bytes = new Uint8Array(memory.buffer, pointer);
        return new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)));
    };
    return {
        // 1. Copy the content text into wasm memory and parse it there.
        load(contentText) {
            const bytes = new TextEncoder().encode(contentText);
            const pointer = e.umbra_alloc(bytes.length);
            new Uint8Array(memory.buffer, pointer, bytes.length).set(bytes);
            const result = e.umbra_load(pointer, bytes.length);
            e.umbra_free(pointer);
            if (result < 0) throw new Error('content rejected by the rules');
            return result; // number of warnings
        },
        reset: () => e.umbra_reset() === 1,
        stepAi: () => e.umbra_step_ai() === 1,
        state: () => JSON.parse(readString(e.umbra_state())),
        memoryBytes: () => memory.buffer.byteLength,
    };
}
