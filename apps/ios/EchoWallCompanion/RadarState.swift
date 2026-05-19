//
//  RadarState.swift
//  EchoWallCompanion
//
//  Observable state model. Holds the latest semantic presence data
//  decoded from the EchoWall WebSocket JSON stream.
//  Raw CSI is never present here — only semantic output.
//

import Foundation
import Combine

// MARK: - Wire types

/// Decoded from ws:///ws JSON frames.
struct RadarFrame: Decodable, Equatable {
    let presence: Bool
    let count: Int
    let posture: String
    let confidence: Double
    let breathingRate: Double?
    let heartRate: Double?
    let timestamp: Double?

    enum CodingKeys: String, CodingKey {
        case presence, count, posture, confidence
        case breathingRate  = "breathing_rate"
        case heartRate      = "heart_rate"
        case timestamp
    }
}

// MARK: - Connection state

enum ConnectionState: Equatable {
    case disconnected
    case connecting
    case connected
    case error(String)

    var label: String {
        switch self {
        case .disconnected:   return "Disconnected"
        case .connecting:     return "Connecting…"
        case .connected:      return "Live"
        case .error(let msg): return "Error: \(msg)"
        }
    }

    var color: String {
        switch self {
        case .connected:      return "green"
        case .connecting:     return "yellow"
        case .disconnected:   return "gray"
        case .error:          return "red"
        }
    }
}

// MARK: - RadarState

@MainActor
final class RadarState: ObservableObject {

    // Latest frame values
    @Published var presence:      Bool   = false
    @Published var count:         Int    = 0
    @Published var posture:       String = "unknown"
    @Published var confidence:    Double = 0.0
    @Published var breathingRate: Double? = nil
    @Published var heartRate:     Double? = nil

    // Connection meta
    @Published var connectionState: ConnectionState = .disconnected
    @Published var lastUpdated:     Date?           = nil
    @Published var frameCount:      Int             = 0

    // Settings (persisted in UserDefaults)
    @Published var nodeHost: String {
        didSet { UserDefaults.standard.set(nodeHost, forKey: "ew_node_host") }
    }
    @Published var nodePort: Int {
        didSet { UserDefaults.standard.set(nodePort, forKey: "ew_node_port") }
    }

    init() {
        self.nodeHost = UserDefaults.standard.string(forKey: "ew_node_host") ?? "echowall.local"
        self.nodePort = UserDefaults.standard.integer(forKey: "ew_node_port")
        if self.nodePort == 0 { self.nodePort = 8765 }
    }

    /// Apply a decoded frame to published state.
    func apply(_ frame: RadarFrame) {
        presence      = frame.presence
        count         = frame.count
        posture       = frame.posture
        confidence    = frame.confidence
        breathingRate = frame.breathingRate
        heartRate     = frame.heartRate
        lastUpdated   = Date()
        frameCount   += 1
    }

    var wsURL: URL? {
        URL(string: "ws://\(nodeHost):\(nodePort)/ws")
    }
}
