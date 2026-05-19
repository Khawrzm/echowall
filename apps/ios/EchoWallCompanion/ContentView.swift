//
//  ContentView.swift
//  EchoWallCompanion
//
//  Minimalist dark-themed radar dashboard.
//  Displays: presence ring, occupancy count, posture, confidence,
//  breathing rate, heart rate, connection status, and node settings.
//

import SwiftUI

struct ContentView: View {

    @StateObject private var state  = RadarState()
    @StateObject private var client: LocalRadarClient

    @State private var showSettings = false

    init() {
        let s = RadarState()
        _state  = StateObject(wrappedValue: s)
        _client = StateObject(wrappedValue: LocalRadarClient(state: s))
    }

    var body: some View {
        NavigationStack {
            ZStack {
                Color.black.ignoresSafeArea()

                ScrollView {
                    VStack(spacing: 24) {

                        // ── Connection banner ──────────────────────────────
                        ConnectionBanner(state: state)

                        // ── Presence ring ─────────────────────────────────
                        PresenceRing(state: state)

                        // ── Metrics grid ──────────────────────────────────
                        MetricsGrid(state: state)

                        // ── Vitals row ────────────────────────────────────
                        if state.presence {
                            VitalsRow(state: state)
                        }

                        // ── Last updated ──────────────────────────────────
                        if let updated = state.lastUpdated {
                            Text("Updated \(updated.formatted(.relative(presentation: .named)))")
                                .font(.caption2)
                                .foregroundStyle(.gray)
                        }
                    }
                    .padding()
                }
            }
            .navigationTitle("EchoWall")
            .navigationBarTitleDisplayMode(.large)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button { showSettings = true } label: {
                        Image(systemName: "antenna.radiowaves.left.and.right")
                            .foregroundStyle(.cyan)
                    }
                }
            }
            .sheet(isPresented: $showSettings) {
                SettingsSheet(state: state, client: client)
            }
            .preferredColorScheme(.dark)
        }
        .onAppear  { client.connect() }
        .onDisappear { client.disconnect() }
    }
}

// MARK: - Connection Banner

struct ConnectionBanner: View {
    @ObservedObject var state: RadarState

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(dotColor)
                .frame(width: 8, height: 8)
                .shadow(color: dotColor.opacity(0.8), radius: 4)
            Text(state.connectionState.label)
                .font(.caption)
                .foregroundStyle(.secondary)
            Spacer()
            Text(state.nodeHost)
                .font(.caption)
                .foregroundStyle(.tertiary)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 10))
    }

    private var dotColor: Color {
        switch state.connectionState {
        case .connected:    return .green
        case .connecting:   return .yellow
        case .disconnected: return .gray
        case .error:        return .red
        }
    }
}

// MARK: - Presence Ring

struct PresenceRing: View {
    @ObservedObject var state: RadarState

    private let ringSize: CGFloat = 200

    var body: some View {
        ZStack {
            // Outer glow ring
            Circle()
                .stroke(ringColor.opacity(0.15), lineWidth: 24)
                .frame(width: ringSize, height: ringSize)

            // Confidence arc
            Circle()
                .trim(from: 0, to: state.presence ? state.confidence : 0)
                .stroke(
                    ringColor,
                    style: StrokeStyle(lineWidth: 6, lineCap: .round)
                )
                .frame(width: ringSize, height: ringSize)
                .rotationEffect(.degrees(-90))
                .animation(.easeInOut(duration: 0.6), value: state.confidence)

            // Centre content
            VStack(spacing: 4) {
                Image(systemName: state.presence ? "person.fill" : "person.slash")
                    .font(.system(size: 44))
                    .foregroundStyle(ringColor)
                    .symbolEffect(.bounce, value: state.presence)

                Text(state.presence ? "Present" : "Empty")
                    .font(.title2.bold())
                    .foregroundStyle(.white)

                if state.presence {
                    Text(state.posture.capitalized)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .animation(.easeInOut(duration: 0.4), value: state.presence)
    }

    private var ringColor: Color {
        guard state.presence else { return .gray }
        switch state.posture.lowercased() {
        case "fallen", "fall": return .red
        case "standing":       return .cyan
        case "sitting":        return .blue
        default:               return .green
        }
    }
}

// MARK: - Metrics Grid

struct MetricsGrid: View {
    @ObservedObject var state: RadarState

    var body: some View {
        LazyVGrid(
            columns: [GridItem(.flexible()), GridItem(.flexible())],
            spacing: 16
        ) {
            MetricCard(
                icon:  "person.2.fill",
                label: "Occupancy",
                value: "\(state.count)",
                unit:  "people",
                color: .cyan
            )
            MetricCard(
                icon:  "gauge.medium",
                label: "Confidence",
                value: String(format: "%.0f", state.confidence * 100),
                unit:  "%",
                color: confidenceColor
            )
        }
    }

    private var confidenceColor: Color {
        switch state.confidence {
        case 0.85...: return .green
        case 0.65..<0.85: return .yellow
        default: return .orange
        }
    }
}

struct MetricCard: View {
    let icon:  String
    let label: String
    let value: String
    let unit:  String
    let color: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label(label, systemImage: icon)
                .font(.caption)
                .foregroundStyle(color)

            HStack(alignment: .firstTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: 36, weight: .bold, design: .rounded))
                    .foregroundStyle(.white)
                    .contentTransition(.numericText())
                Text(unit)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14))
    }
}

// MARK: - Vitals Row

struct VitalsRow: View {
    @ObservedObject var state: RadarState

    var body: some View {
        HStack(spacing: 16) {
            if let bpm = state.breathingRate {
                VitalPill(
                    icon:  "lungs.fill",
                    label: "Breathing",
                    value: String(format: "%.1f", bpm),
                    unit:  "bpm",
                    color: .teal,
                    disclaimer: true
                )
            }
            if let hr = state.heartRate {
                VitalPill(
                    icon:  "heart.fill",
                    label: "Heart Rate",
                    value: String(format: "%.0f", hr),
                    unit:  "bpm",
                    color: .pink,
                    disclaimer: true
                )
            }
        }
    }
}

struct VitalPill: View {
    let icon:       String
    let label:      String
    let value:      String
    let unit:       String
    let color:      Color
    let disclaimer: Bool

    @State private var showDisclaimer = false

    var body: some View {
        Button { if disclaimer { showDisclaimer = true } } label: {
            HStack(spacing: 8) {
                Image(systemName: icon)
                    .foregroundStyle(color)
                VStack(alignment: .leading, spacing: 2) {
                    Text(label)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                    HStack(alignment: .firstTextBaseline, spacing: 2) {
                        Text(value)
                            .font(.title3.bold())
                            .foregroundStyle(.white)
                            .contentTransition(.numericText())
                        Text(unit)
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                    }
                }
                if disclaimer {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .font(.caption2)
                        .foregroundStyle(.yellow.opacity(0.7))
                }
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
        }
        .buttonStyle(.plain)
        .alert("Research-Grade Estimate", isPresented: $showDisclaimer) {
            Button("Understood", role: .cancel) { }
        } message: {
            Text(
                "This reading is a research-grade estimate from passive Wi-Fi sensing. " +
                "Accuracy is ±2–8 bpm. It is NOT a medical measurement. " +
                "Do not use for clinical or emergency decisions."
            )
        }
    }
}

// MARK: - Settings Sheet

struct SettingsSheet: View {
    @ObservedObject var state:  RadarState
    @ObservedObject var client: LocalRadarClient
    @Environment(\.dismiss) private var dismiss

    @State private var draftHost: String = ""
    @State private var draftPort: String = ""

    var body: some View {
        NavigationStack {
            Form {
                Section("EchoWall Node") {
                    LabeledContent("Host / mDNS") {
                        TextField("echowall.local", text: $draftHost)
                            .multilineTextAlignment(.trailing)
                            .keyboardType(.URL)
                            .autocorrectionDisabled()
                            .textInputAutocapitalization(.never)
                    }
                    LabeledContent("Port") {
                        TextField("8765", text: $draftPort)
                            .multilineTextAlignment(.trailing)
                            .keyboardType(.numberPad)
                    }
                }

                Section("Connection") {
                    LabeledContent("Status",  value: state.connectionState.label)
                    LabeledContent("Frames",  value: "\(state.frameCount)")
                    if let updated = state.lastUpdated {
                        LabeledContent("Last frame",
                                       value: updated.formatted(.dateTime.hour().minute().second()))
                    }
                    Button("Reconnect") {
                        client.disconnect()
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                            client.connect()
                        }
                    }
                    .foregroundStyle(.cyan)
                }

                Section {
                    Text(
                        "EchoWall Companion connects only to your local EchoWall node. " +
                        "No data is sent to any external server. " +
                        "Only semantic presence data is received — no raw CSI."
                    )
                    .font(.caption)
                    .foregroundStyle(.secondary)
                } header: {
                    Text("Privacy")
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        if !draftHost.isEmpty {
                            state.nodeHost = draftHost
                        }
                        if let p = Int(draftPort), p > 0, p < 65536 {
                            state.nodePort = p
                        }
                        client.disconnect()
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
                            client.connect()
                        }
                        dismiss()
                    }
                }
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
            }
            .onAppear {
                draftHost = state.nodeHost
                draftPort = String(state.nodePort)
            }
        }
        .presentationDetents([.medium])
    }
}

// MARK: - Preview

#Preview {
    ContentView()
}
