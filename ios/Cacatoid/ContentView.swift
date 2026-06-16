import SwiftUI

struct ContentView: View {
    @EnvironmentObject var vm: SearchViewModel

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 20) {
                    puzzleSection
                    controlSection
                    statusSection
                    statsSection
                    if vm.found != nil {
                        resultSection
                    }
                }
                .padding()
            }
            .navigationTitle("Cacatoid")
            .navigationBarTitleDisplayMode(.large)
        }
        .navigationViewStyle(.stack)
    }

    // MARK: - Puzzle picker

    private var puzzleSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Puzzle")
                .font(.headline)
            Picker("Puzzle", selection: $vm.selectedPuzzle) {
                ForEach(vm.puzzles, id: \.self) { p in
                    Text("Puzzle \(p)").tag(p)
                }
            }
            .pickerStyle(.segmented)
            .disabled(vm.running)
        }
    }

    // MARK: - Start / Stop

    private var controlSection: some View {
        HStack(spacing: 16) {
            Button("Start") { vm.start() }
                .buttonStyle(.borderedProminent)
                .disabled(vm.running)

            Button("Stop") { vm.stop() }
                .buttonStyle(.bordered)
                .tint(.red)
                .disabled(!vm.running)
        }
    }

    // MARK: - Status row

    private var statusSection: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(statusColor)
                .frame(width: 12, height: 12)
            Text(statusText)
                .font(.subheadline)
            if vm.running {
                ProgressView().scaleEffect(0.8)
            }
            Spacer()
        }
    }

    private var statusColor: Color {
        if vm.found != nil { return .orange }
        return vm.running ? .green : .gray
    }

    private var statusText: String {
        if vm.found != nil { return "Match found!" }
        return vm.running ? "Running…" : "Stopped"
    }

    // MARK: - Stats card

    private var statsSection: some View {
        VStack(spacing: 10) {
            statRow("Current key", value: truncatedKey)
            statRow("Speed", value: "\(vm.stats.keysPerSec.formatted()) keys/s")
            statRow("Total checked", value: vm.stats.totalChecked.formatted())
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
    }

    private var truncatedKey: String {
        let h = vm.stats.currentKeyHex
        guard !h.isEmpty else { return "—" }
        return "…" + String(h.suffix(20))
    }

    private func statRow(_ label: String, value: String) -> some View {
        HStack {
            Text(label).foregroundColor(.secondary)
            Spacer()
            Text(value).font(.system(.body, design: .monospaced))
        }
    }

    // MARK: - Result card

    private var resultSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("Match Found!", systemImage: "checkmark.seal.fill")
                .font(.headline)
                .foregroundColor(.green)

            if let found = vm.found {
                resultField("Puzzle",      value: "\(found.puzzle)")
                resultField("Address",     value: found.address)
                resultField("Private Key", value: found.privKeyHex)
                resultField("WIF",         value: found.wif)

                Button {
                    UIPasteboard.general.string = found.privKeyHex
                } label: {
                    Label("Copy Private Key", systemImage: "doc.on.doc")
                }
                .buttonStyle(.bordered)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .cornerRadius(12)
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.green, lineWidth: 1))
    }

    private func resultField(_ label: String, value: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label).font(.caption).foregroundColor(.secondary)
            Text(value)
                .font(.system(.caption, design: .monospaced))
                .textSelection(.enabled)
                .lineLimit(2)
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(SearchViewModel())
}
