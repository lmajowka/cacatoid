import Foundation

struct SearchStats {
    var currentKeyHex: String = ""
    var keysPerSec: Int64 = 0
    var totalChecked: Int64 = 0
}

struct SearchFound {
    var puzzle: Int
    var privKeyHex: String
    var wif: String
    var address: String
}

// File-scope C-compatible callbacks — Swift global functions can be passed as
// C function pointers as long as they don't capture any state.
func _statsCallback(
    _ keyHex: UnsafePointer<CChar>?,
    _ kps: Int64,
    _ total: Int64,
    _ ctx: UnsafeMutableRawPointer?
) {
    guard let ctx = ctx, let keyHex = keyHex else { return }
    let vm = Unmanaged<SearchViewModel>.fromOpaque(ctx).takeUnretainedValue()
    let hex = String(cString: keyHex)
    DispatchQueue.main.async {
        vm.stats = SearchStats(currentKeyHex: hex, keysPerSec: kps, totalChecked: total)
    }
}

func _foundCallback(
    _ privHex: UnsafePointer<CChar>?,
    _ wif: UnsafePointer<CChar>?,
    _ addr: UnsafePointer<CChar>?,
    _ puzzle: Int32,
    _ ctx: UnsafeMutableRawPointer?
) {
    guard let ctx = ctx,
          let privHex = privHex,
          let wif = wif,
          let addr = addr else { return }
    let vm = Unmanaged<SearchViewModel>.fromOpaque(ctx).takeUnretainedValue()
    let result = SearchFound(
        puzzle: Int(puzzle),
        privKeyHex: String(cString: privHex),
        wif: String(cString: wif),
        address: String(cString: addr)
    )
    DispatchQueue.main.async {
        vm.found = result
        vm.running = false
        vm.persistFound(result)
        // Release the retain taken in start().
        Unmanaged<SearchViewModel>.fromOpaque(ctx).release()
        vm.contextPtr = nil
    }
}

class SearchViewModel: ObservableObject {
    @Published var running = false
    @Published var stats = SearchStats()
    @Published var found: SearchFound? = nil
    @Published var selectedPuzzle = 71

    let puzzles = [20, 71, 72, 73]

    // Holds the retained reference passed to the C callbacks so we can
    // release it on explicit stop() before a found callback fires.
    var contextPtr: UnsafeMutableRawPointer? = nil

    func start() {
        guard !running else { return }
        running = true
        found = nil
        stats = SearchStats()

        let ctx = Unmanaged.passRetained(self).toOpaque()
        contextPtr = ctx
        searcher_start(Int32(selectedPuzzle), _statsCallback, _foundCallback, ctx)
    }

    func stop() {
        guard running else { return }
        searcher_stop()
        running = false
        if let ctx = contextPtr {
            Unmanaged<SearchViewModel>.fromOpaque(ctx).release()
            contextPtr = nil
        }
    }

    func persistFound(_ result: SearchFound) {
        guard let docs = FileManager.default
            .urls(for: .documentDirectory, in: .userDomainMask).first else { return }
        let file = docs.appendingPathComponent("found_keys.txt")
        let ts = ISO8601DateFormatter().string(from: Date())
        let text = "[\(ts)] puzzle \(result.puzzle)\n" +
                   "  privkey_hex: \(result.privKeyHex)\n" +
                   "  wif:         \(result.wif)\n" +
                   "  address:     \(result.address)\n\n"
        guard let data = text.data(using: .utf8) else { return }
        if FileManager.default.fileExists(atPath: file.path),
           let handle = try? FileHandle(forWritingTo: file) {
            handle.seekToEndOfFile()
            handle.write(data)
            try? handle.close()
        } else {
            try? data.write(to: file)
        }
    }
}
