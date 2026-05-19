//
//  LocalRadarClient.swift
//  EchoWallCompanion
//
//  LAN-only WebSocket client using URLSessionWebSocketTask.
//  Connects to ws://echowall.local:8765/ws (or user-configured host).
//
//  Privacy contract:
//    - Only outbound destination: LAN IP / echowall.local.
//    - Only data received: semantic JSON (presence, posture, vitals).
//    - No raw CSI, no audio, no waveforms.
//    - No external SDK. No analytics. No crash reporting.
//

import Foundation

@MainActor
final class LocalRadarClient: NSObject, ObservableObject {

    private weak var state: RadarState?
    private var webSocketTask: URLSessionWebSocketTask?
    private var session: URLSession?
    private var reconnectTask: Task<Void, Never>?

    /// Reconnect back-off: 1s, 2s, 4s, 8s, capped at 16s.
    private var backoffSeconds: UInt64 = 1
    private static let maxBackoff: UInt64 = 16

    init(state: RadarState) {
        self.state = state
        super.init()
        self.session = URLSession(
            configuration: .default,
            delegate: self,
            delegateQueue: nil
        )
    }

    // MARK: - Public API

    func connect() {
        guard let state, let url = state.wsURL else { return }
        disconnect()
        state.connectionState = .connecting
        let task = session!.webSocketTask(with: url)
        webSocketTask = task
        task.resume()
        scheduleReceive()
    }

    func disconnect() {
        reconnectTask?.cancel()
        reconnectTask = nil
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil
        Task { @MainActor in
            state?.connectionState = .disconnected
        }
    }

    // MARK: - Receive loop

    private func scheduleReceive() {
        webSocketTask?.receive { [weak self] result in
            guard let self else { return }
            Task { @MainActor in
                switch result {
                case .success(let message):
                    self.backoffSeconds = 1   // reset back-off on successful receive
                    self.state?.connectionState = .connected
                    self.handleMessage(message)
                    self.scheduleReceive()    // re-arm for next frame

                case .failure(let error):
                    let msg = error.localizedDescription
                    self.state?.connectionState = .error(msg)
                    self.scheduleReconnect()
                }
            }
        }
    }

    private func handleMessage(_ message: URLSessionWebSocketTask.Message) {
        let data: Data
        switch message {
        case .data(let d):   data = d
        case .string(let s): data = Data(s.utf8)
        @unknown default:    return
        }

        guard let frame = try? JSONDecoder().decode(RadarFrame.self, from: data) else {
            // Silently drop malformed frames — never crash on bad input.
            return
        }
        state?.apply(frame)
    }

    // MARK: - Reconnect with exponential back-off

    private func scheduleReconnect() {
        let delay = backoffSeconds
        backoffSeconds = min(backoffSeconds * 2, Self.maxBackoff)

        reconnectTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: delay * 1_000_000_000)
            guard !Task.isCancelled, let self else { return }
            await MainActor.run { self.connect() }
        }
    }
}

// MARK: - URLSessionWebSocketDelegate

extension LocalRadarClient: URLSessionWebSocketDelegate {

    nonisolated func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didOpenWithProtocol protocol: String?
    ) {
        Task { @MainActor in
            self.state?.connectionState = .connected
            self.backoffSeconds = 1
        }
    }

    nonisolated func urlSession(
        _ session: URLSession,
        webSocketTask: URLSessionWebSocketTask,
        didCloseWith closeCode: URLSessionWebSocketTask.CloseCode,
        reason: Data?
    ) {
        Task { @MainActor in
            self.state?.connectionState = .disconnected
            // Attempt reconnect unless we closed intentionally (.goingAway)
            if closeCode != .goingAway {
                self.scheduleReconnect()
            }
        }
    }
}
