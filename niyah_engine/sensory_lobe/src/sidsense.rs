//! # SIDSense — Autonomous TVWS Spectrum Scanner
//!
//! Initializes and drives the TV White Space (TVWS) spectrum scanner
//! for sovereign long-range low-power connectivity and passive sensing.
//!
//! TVWS occupies UHF channels 21–60 (470–790 MHz, region-dependent).
//! SIDSense scans these channels autonomously, identifies vacant slots,
//! and feeds occupancy data into the Sensory fusion pipeline.

use heapless::Vec;

/// Maximum number of TVWS channels tracked simultaneously.
pub const MAX_TVWS_CHANNELS: usize = 40;

/// A single TVWS channel descriptor.
#[derive(Debug, Clone, Copy)]
pub struct TvwsChannel {
    /// Channel number (21–60, ITU Region 1).
    pub channel: u8,
    /// Centre frequency in kHz (e.g., channel 21 = 474_000 kHz).
    pub centre_khz: u32,
    /// Channel bandwidth in kHz (typically 8_000 kHz for DVB-T).
    pub bandwidth_khz: u32,
    /// Last measured received signal strength in dBm × 10 (INT16).
    /// e.g., -70 dBm is stored as -700.
    pub rssi_dbm_x10: i16,
    /// True if this channel is assessed as vacant (no primary user detected).
    pub vacant: bool,
}

impl TvwsChannel {
    /// Construct a channel descriptor with default (unscanned) state.
    pub const fn new(channel: u8, centre_khz: u32) -> Self {
        Self {
            channel,
            centre_khz,
            bandwidth_khz: 8_000,
            rssi_dbm_x10: 0,
            vacant: false,
        }
    }
}

/// SIDSense initialization configuration.
#[derive(Debug, Clone, Copy)]
pub struct SidSenseConfig {
    /// First channel to scan.
    pub channel_start: u8,
    /// Last channel to scan (inclusive).
    pub channel_end: u8,
    /// Dwell time per channel in milliseconds.
    pub dwell_ms: u16,
    /// RSSI vacancy threshold in dBm × 10.
    /// Channels with RSSI below this are marked vacant.
    pub vacancy_threshold_dbm_x10: i16,
    /// Number of scan passes before publishing occupancy map.
    pub scan_passes: u8,
}

impl Default for SidSenseConfig {
    fn default() -> Self {
        Self {
            channel_start: 21,
            channel_end: 60,
            dwell_ms: 50,
            vacancy_threshold_dbm_x10: -900, // -90.0 dBm
            scan_passes: 3,
        }
    }
}

/// Autonomous TVWS spectrum scanner.
///
/// Scans UHF TVWS channels, identifies vacant spectrum,
/// and publishes occupancy maps to the fusion pipeline.
pub struct SidSense {
    config: SidSenseConfig,
    channels: Vec<TvwsChannel, MAX_TVWS_CHANNELS>,
    scan_pass: u8,
    initialized: bool,
}

impl SidSense {
    /// Construct a SIDSense scanner with the given configuration.
    pub fn new(config: SidSenseConfig) -> Self {
        let mut channels = Vec::new();
        // Pre-populate ITU Region 1 UHF channel plan (8 MHz spacing, start 474 MHz).
        let start = config.channel_start.max(21);
        let end = config.channel_end.min(60);
        for ch in start..=end {
            let centre_khz = 474_000u32 + (ch as u32 - 21) * 8_000;
            // Ignore push error — Vec is bounded; channels beyond MAX are skipped.
            let _ = channels.push(TvwsChannel::new(ch, centre_khz));
        }
        Self {
            config,
            channels,
            scan_pass: 0,
            initialized: false,
        }
    }

    /// Initialize the scanner hardware interface.
    ///
    /// Must be called once before `tick()`. Idempotent.
    pub fn init(&mut self) {
        self.initialized = true;
        self.scan_pass = 0;
    }

    /// Process one scan tick: update RSSI for the next channel in sequence.
    ///
    /// In production, `rssi_sample` comes from the SDR via DMA descriptor
    /// output. In simulation/test, inject a value directly.
    ///
    /// Returns `true` when a full scan pass has completed and the occupancy
    /// map is ready for fusion.
    pub fn tick(&mut self, channel_idx: usize, rssi_dbm_x10: i16) -> bool {
        if !self.initialized || channel_idx >= self.channels.len() {
            return false;
        }
        let ch = &mut self.channels[channel_idx];
        ch.rssi_dbm_x10 = rssi_dbm_x10;
        ch.vacant = rssi_dbm_x10 < self.config.vacancy_threshold_dbm_x10;

        if channel_idx == self.channels.len() - 1 {
            self.scan_pass = self.scan_pass.saturating_add(1);
            return self.scan_pass >= self.config.scan_passes;
        }
        false
    }

    /// Return a slice of all tracked channels.
    pub fn channels(&self) -> &[TvwsChannel] {
        &self.channels
    }

    /// Count vacant channels in the current occupancy map.
    pub fn vacant_count(&self) -> usize {
        self.channels.iter().filter(|c| c.vacant).count()
    }
}
