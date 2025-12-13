//
//  ContentView.swift
//  RespiroSync Demo App
//
//  Drop this into a new SwiftUI iOS project and you're ready to go!
//

import SwiftUI

struct ContentView: View {
    @StateObject private var respiroManager = RespiroManager()
    
    var body: some View {
        ZStack {
            // Gradient background
            LinearGradient(
                colors: [Color(hex: "1a1a2e"), Color(hex: "16213e")],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()
            
            VStack(spacing: 30) {
                // Header
                VStack(spacing: 8) {
                    Text("RespiroSync")
                        .font(.system(size: 36, weight: .bold))
                        .foregroundColor(.white)
                    
                    Text("Chest-Mounted Sleep Tracker")
                        .font(.subheadline)
                        .foregroundColor(.white.opacity(0.7))
                }
                .padding(.top, 50)
                
                // Status Indicator
                HStack {
                    Circle()
                        .fill(respiroManager.isRunning ? Color.green : Color.gray)
                        .frame(width: 12, height: 12)
                    
                    Text(respiroManager.isRunning ? "Session Active" : "Not Running")
                        .font(.subheadline)
                        .foregroundColor(.white.opacity(0.8))
                }
                
                Spacer()
                
                // Main Metrics
                if respiroManager.isRunning {
                    VStack(spacing: 25) {
                        // Breathing Rate
                        MetricCard(
                            icon: "wind",
                            title: "Breathing Rate",
                            value: String(format: "%.1f", respiroManager.metrics?.breathingRateBPM ?? 0.0),
                            unit: "BPM",
                            color: Color(hex: "4cc9f0")
                        )
                        
                        // Sleep Stage
                        MetricCard(
                            icon: "moon.zzz.fill",
                            title: "Sleep Stage",
                            value: respiroManager.sleepStageText,
                            unit: "",
                            color: Color(hex: "7209b7")
                        )
                        
                        // Confidence
                        HStack(spacing: 15) {
                            MetricCard(
                                icon: "chart.bar.fill",
                                title: "Confidence",
                                value: String(format: "%.0f%%", (respiroManager.metrics?.confidence ?? 0) * 100),
                                unit: "",
                                color: Color(hex: "f72585"),
                                compact: true
                            )
                            
                            MetricCard(
                                icon: "heart.fill",
                                title: "Breaths",
                                value: "\(respiroManager.metrics?.breathCyclesDetected ?? 0)",
                                unit: "",
                                color: Color(hex: "3a0ca3"),
                                compact: true
                            )
                        }
                        
                        // Apnea Warning
                        if respiroManager.metrics?.possibleApnea == true {
                            HStack {
                                Image(systemName: "exclamationmark.triangle.fill")
                                    .foregroundColor(.yellow)
                                Text("Possible Apnea Detected")
                                    .font(.subheadline)
                                    .foregroundColor(.yellow)
                            }
                            .padding()
                            .background(Color.yellow.opacity(0.2))
                            .cornerRadius(12)
                        }
                    }
                } else {
                    VStack(spacing: 20) {
                        Image(systemName: "apps.iphone")
                            .font(.system(size: 80))
                            .foregroundColor(.white.opacity(0.3))
                        
                        Text("Position phone on chest")
                            .font(.title3)
                            .foregroundColor(.white.opacity(0.8))
                        
                        Text("Vertical orientation, screen facing out")
                            .font(.caption)
                            .foregroundColor(.white.opacity(0.5))
                    }
                }
                
                Spacer()
                
                // Control Button
                Button(action: {
                    if respiroManager.isRunning {
                        respiroManager.stop()
                    } else {
                        respiroManager.start()
                    }
                }) {
                    HStack {
                        Image(systemName: respiroManager.isRunning ? "stop.fill" : "play.fill")
                        Text(respiroManager.isRunning ? "Stop Session" : "Start Session")
                            .fontWeight(.semibold)
                    }
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(respiroManager.isRunning ? Color.red : Color(hex: "4cc9f0"))
                    .foregroundColor(.white)
                    .cornerRadius(16)
                }
                .padding(.horizontal, 30)
                .padding(.bottom, 40)
            }
        }
    }
}

// MARK: - Metric Card Component

struct MetricCard: View {
    let icon: String
    let title: String
    let value: String
    let unit: String
    let color: Color
    var compact: Bool = false
    
    var body: some View {
        VStack(spacing: compact ? 8 : 12) {
            Image(systemName: icon)
                .font(.system(size: compact ? 24 : 32))
                .foregroundColor(color)
            
            Text(title)
                .font(compact ? .caption : .subheadline)
                .foregroundColor(.white.opacity(0.7))
            
            HStack(alignment: .firstTextBaseline, spacing: 4) {
                Text(value)
                    .font(.system(size: compact ? 24 : 48, weight: .bold))
                    .foregroundColor(.white)
                
                if !unit.isEmpty {
                    Text(unit)
                        .font(compact ? .caption : .title3)
                        .foregroundColor(.white.opacity(0.6))
                }
            }
        }
        .frame(maxWidth: compact ? nil : .infinity)
        .padding(compact ? 15 : 20)
        .background(Color.white.opacity(0.1))
        .cornerRadius(20)
        .overlay(
            RoundedRectangle(cornerRadius: 20)
                .stroke(color.opacity(0.3), lineWidth: 1)
        )
    }
}

// MARK: - RespiroSync Manager

class RespiroManager: ObservableObject {
    @Published var isRunning = false
    @Published var metrics: RespiroSyncMetrics?
    
    private var respiro: RespiroSync?
    private var timer: Timer?
    
    var sleepStageText: String {
        guard let stage = metrics?.sleepStage else { return "UNKNOWN" }
        switch stage {
        case 0: return "AWAKE"
        case 1: return "LIGHT"
        case 2: return "DEEP"
        case 3: return "REM"
        default: return "UNKNOWN"
        }
    }
    
    func start() {
        respiro = RespiroSync()
        respiro?.startSession()
        isRunning = true
        
        // Update metrics every second
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.updateMetrics()
        }
    }
    
    func stop() {
        timer?.invalidate()
        timer = nil
        respiro?.stopSession()
        respiro = nil
        isRunning = false
        metrics = nil
    }
    
    private func updateMetrics() {
        metrics = respiro?.getCurrentMetrics()
    }
}

// MARK: - Color Extension

extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3: // RGB (12-bit)
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6: // RGB (24-bit)
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8: // ARGB (32-bit)
            (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (255, 0, 0, 0)
        }
        self.init(
            .sRGB,
            red: Double(r) / 255,
            green: Double(g) / 255,
            blue: Double(b) / 255,
            opacity: Double(a) / 255
        )
    }
}

// MARK: - Preview

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}

/*
 * INSTRUCTIONS TO USE:
 * 
 * 1. Create new iOS App project in Xcode (SwiftUI)
 * 2. Replace ContentView.swift with this file
 * 3. Add the RespiroSync library to your project
 * 4. Run on your iPhone
 * 5. Put phone in chest vest and tap "Start Session"
 * 
 * That's it! You're tracking sleep.
 */
