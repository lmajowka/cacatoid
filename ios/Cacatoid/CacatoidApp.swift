import SwiftUI

@main
struct CacatoidApp: App {
    @StateObject private var viewModel = SearchViewModel()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(viewModel)
        }
    }
}
