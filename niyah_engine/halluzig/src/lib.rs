//! # HalluZig — Topological Data Analysis for LLM Hallucination Detection
//!
//! This module implements zigzag persistence analysis on attention matrix evolution
//! to detect hallucinations BEFORE the first token is generated.
//!
//! ## Core Concept
//!
//! Traditional hallucination detection operates on generated text (reactive).
//! HalluZig operates on the internal attention graph structure (proactive).
//!
//! ## Mathematical Foundation
//!
//! 1. Model attention matrices as a time-varying graph filtration
//! 2. Apply zigzag persistence to extract topological signatures
//! 3. Compare signatures against "factual reasoning" benchmarks
//! 4. Detect fragmented geometric properties indicating hallucination
//!
//! ## Architecture
//!
//! ```text
//! Layer 1 Attention → Graph₁
//! Layer 2 Attention → Graph₂
//!       ⋮                ⋮
//! Layer N Attention → Graphₙ
//!        ↓
//!   Zigzag Filtration
//!        ↓
//!   Persistence Diagram
//!        ↓
//!   Topological Signature
//!        ↓
//!   Hallucination Score
//! ```

#![no_std]
#![deny(unsafe_code)]

#[cfg(feature = "gpu")]
pub mod gpu_accel;
#![deny(missing_docs)]
#![warn(clippy::all)]

extern crate alloc;
use alloc::vec::Vec;
use core::cmp::Ordering;

/// Maximum number of layers we track in the attention graph filtration
pub const MAX_LAYERS: usize = 32;

/// Maximum number of attention heads per layer
pub const MAX_HEADS: usize = 16;

/// Topological signature extracted from zigzag persistence
#[derive(Debug, Clone)]
pub struct TopologicalSignature {
    /// Birth-death pairs from persistence diagram
    pub persistence_pairs: Vec<(f32, f32)>,
    /// Betti numbers at each filtration step
    pub betti_numbers: Vec<usize>,
    /// Fragmentation score (0.0 = coherent, 1.0 = highly fragmented)
    pub fragmentation_score: f32,
}

/// Attention matrix for a single layer
#[derive(Debug, Clone)]
pub struct AttentionMatrix {
    /// Layer index
    pub layer: usize,
    /// Number of attention heads
    pub num_heads: usize,
    /// Attention weights (flattened: [heads × seq_len × seq_len])
    pub weights: Vec<f32>,
    /// Sequence length
    pub seq_len: usize,
}

/// Zigzag persistence analyzer
pub struct ZigzagAnalyzer {
    /// Stored attention matrices from multiple layers
    attention_stack: Vec<AttentionMatrix>,
    /// Factual reasoning baseline signature (loaded from benchmarks)
    baseline_signature: Option<TopologicalSignature>,
}

impl ZigzagAnalyzer {
    /// Create a new zigzag analyzer
    pub const fn new() -> Self {
        Self {
            attention_stack: Vec::new(),
            baseline_signature: None,
        }
    }

    /// Add an attention matrix from a layer
    pub fn push_attention(&mut self, attention: AttentionMatrix) {
        if self.attention_stack.len() < MAX_LAYERS {
            self.attention_stack.push(attention);
        }
    }

    /// Compute zigzag persistence and extract topological signature
    pub fn compute_signature(&self) -> TopologicalSignature {
        // Stage 1: Build zigzag graph filtration
        let filtration = self.build_filtration();

        // Stage 2: Compute persistence diagram
        let persistence_pairs = self.compute_persistence(&filtration);

        // Stage 3: Extract Betti numbers
        let betti_numbers = self.compute_betti_numbers(&filtration);

        // Stage 4: Calculate fragmentation score
        let fragmentation_score = self.compute_fragmentation(&persistence_pairs);

        TopologicalSignature {
            persistence_pairs,
            betti_numbers,
            fragmentation_score,
        }
    }

    /// Detect if current signature indicates hallucination
    pub fn is_hallucinating(&self, threshold: f32) -> bool {
        let signature = self.compute_signature();
        
        // Hallucinated outputs show fragmented geometric properties
        // in the attention graph (higher fragmentation score)
        signature.fragmentation_score > threshold
    }

    /// Compare against baseline factual reasoning signature
    pub fn deviation_from_baseline(&self) -> f32 {
        let current = self.compute_signature();
        
        if let Some(baseline) = &self.baseline_signature {
            self.compute_signature_distance(&current, baseline)
        } else {
            // No baseline loaded, use fragmentation score directly
            current.fragmentation_score
        }
    }

    /// Build zigzag graph filtration from attention matrices
    fn build_filtration(&self) -> Vec<Graph> {
        let mut graphs = Vec::new();

        for attention in &self.attention_stack {
            // Convert attention matrix to graph representation
            let graph = self.attention_to_graph(attention);
            graphs.push(graph);
        }

        graphs
    }

    /// Convert attention matrix to graph
    fn attention_to_graph(&self, attention: &AttentionMatrix) -> Graph {
        let mut edges = Vec::new();

        // Average across all attention heads
        for i in 0..attention.seq_len {
            for j in 0..attention.seq_len {
                let mut weight_sum = 0.0;
                
                for head in 0..attention.num_heads {
                    let idx = head * attention.seq_len * attention.seq_len 
                            + i * attention.seq_len 
                            + j;
                    
                    if idx < attention.weights.len() {
                        weight_sum += attention.weights[idx];
                    }
                }

                let avg_weight = weight_sum / (attention.num_heads as f32);
                
                // Only include edges above threshold
                if avg_weight > 0.01 {
                    edges.push(Edge {
                        source: i,
                        target: j,
                        weight: avg_weight,
                    });
                }
            }
        }

        Graph {
            num_nodes: attention.seq_len,
            edges,
        }
    }

    /// Compute persistence diagram via zigzag homology
    fn compute_persistence(&self, filtration: &[Graph]) -> Vec<(f32, f32)> {
        let mut pairs = Vec::new();

        // Simplified zigzag persistence computation
        // In production, use proper TDA library (e.g., Ripser, GUDHI)
        for (i, graph) in filtration.iter().enumerate() {
            let birth_time = i as f32;
            
            // Detect cycles/holes in the graph
            let cycles = self.detect_cycles(graph);
            
            for cycle_size in cycles {
                // Birth: when cycle appears
                // Death: when cycle is filled (approximated)
                let death_time = birth_time + (cycle_size as f32) * 0.1;
                pairs.push((birth_time, death_time));
            }
        }

        pairs
    }

    /// Detect cycles in graph (simplified)
    fn detect_cycles(&self, graph: &Graph) -> Vec<usize> {
        let mut cycles = Vec::new();
        
        // Simple cycle detection via DFS
        // Count connected components and estimate cycle sizes
        let mut visited = vec![false; graph.num_nodes];
        
        for start in 0..graph.num_nodes {
            if !visited[start] {
                let component_size = self.dfs_count(graph, start, &mut visited);
                if component_size > 2 {
                    cycles.push(component_size);
                }
            }
        }

        cycles
    }

    /// DFS to count component size
    fn dfs_count(&self, graph: &Graph, node: usize, visited: &mut [bool]) -> usize {
        if visited[node] {
            return 0;
        }

        visited[node] = true;
        let mut count = 1;

        for edge in &graph.edges {
            if edge.source == node && !visited[edge.target] {
                count += self.dfs_count(graph, edge.target, visited);
            }
        }

        count
    }

    /// Compute Betti numbers (connectivity features)
    fn compute_betti_numbers(&self, filtration: &[Graph]) -> Vec<usize> {
        let mut betti = Vec::new();

        for graph in filtration {
            // β₀ = number of connected components
            let beta_0 = self.count_components(graph);
            betti.push(beta_0);
        }

        betti
    }

    /// Count connected components
    fn count_components(&self, graph: &Graph) -> usize {
        let mut visited = vec![false; graph.num_nodes];
        let mut components = 0;

        for node in 0..graph.num_nodes {
            if !visited[node] {
                self.dfs_mark(graph, node, &mut visited);
                components += 1;
            }
        }

        components
    }

    /// DFS to mark component
    fn dfs_mark(&self, graph: &Graph, node: usize, visited: &mut [bool]) {
        if visited[node] {
            return;
        }

        visited[node] = true;

        for edge in &graph.edges {
            if edge.source == node {
                self.dfs_mark(graph, edge.target, visited);
            }
        }
    }

    /// Compute fragmentation score from persistence pairs
    fn compute_fragmentation(&self, pairs: &[(f32, f32)]) -> f32 {
        if pairs.is_empty() {
            return 0.0;
        }

        // Fragmentation = variance in persistence lifetimes
        let lifetimes: Vec<f32> = pairs
            .iter()
            .map(|(birth, death)| death - birth)
            .collect();

        let mean = lifetimes.iter().sum::<f32>() / (lifetimes.len() as f32);
        
        let variance = lifetimes
            .iter()
            .map(|&x| (x - mean).powi(2))
            .sum::<f32>() / (lifetimes.len() as f32);

        // Normalize to [0, 1]
        (variance.sqrt() / (mean + 1e-6)).min(1.0)
    }

    /// Compute distance between two signatures
    fn compute_signature_distance(&self, sig1: &TopologicalSignature, sig2: &TopologicalSignature) -> f32 {
        // Bottleneck distance approximation
        let frag_diff = (sig1.fragmentation_score - sig2.fragmentation_score).abs();
        
        // Betti number difference
        let betti_diff = self.betti_distance(&sig1.betti_numbers, &sig2.betti_numbers);

        // Weighted combination
        0.6 * frag_diff + 0.4 * betti_diff
    }

    /// Compute distance between Betti number sequences
    fn betti_distance(&self, b1: &[usize], b2: &[usize]) -> f32 {
        let min_len = b1.len().min(b2.len());
        let mut diff = 0.0;

        for i in 0..min_len {
            diff += ((b1[i] as f32) - (b2[i] as f32)).abs();
        }

        // Normalize by length
        diff / (min_len as f32 + 1e-6)
    }

    /// Load baseline signature from factual reasoning benchmarks
    pub fn load_baseline(&mut self, signature: TopologicalSignature) {
        self.baseline_signature = Some(signature);
    }

    /// Clear attention stack (reset for new generation)
    pub fn clear(&mut self) {
        self.attention_stack.clear();
    }
}

impl Default for ZigzagAnalyzer {
    fn default() -> Self {
        Self::new()
    }
}

/// Graph representation
#[derive(Debug, Clone)]
struct Graph {
    num_nodes: usize,
    edges: Vec<Edge>,
}

/// Edge in attention graph
#[derive(Debug, Clone)]
struct Edge {
    source: usize,
    target: usize,
    weight: f32,
}

/// Decision from hallucination detector
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HallucinationDecision {
    /// Output is grounded and coherent
    Coherent,
    /// Output shows signs of hallucination
    Hallucinating,
    /// Unable to determine (insufficient data)
    Uncertain,
}

/// High-level hallucination detector
pub struct HallucinationDetector {
    analyzer: ZigzagAnalyzer,
    threshold: f32,
}

impl HallucinationDetector {
    /// Create new detector with threshold
    pub fn new(threshold: f32) -> Self {
        Self {
            analyzer: ZigzagAnalyzer::new(),
            threshold,
        }
    }

    /// Process attention matrix from a layer
    pub fn process_layer(&mut self, attention: AttentionMatrix) {
        self.analyzer.push_attention(attention);
    }

    /// Make decision on current generation
    pub fn decide(&self) -> HallucinationDecision {
        if self.analyzer.attention_stack.is_empty() {
            return HallucinationDecision::Uncertain;
        }

        if self.analyzer.is_hallucinating(self.threshold) {
            HallucinationDecision::Hallucinating
        } else {
            HallucinationDecision::Coherent
        }
    }

    /// Get detailed signature
    pub fn get_signature(&self) -> TopologicalSignature {
        self.analyzer.compute_signature()
    }

    /// Reset for new generation
    pub fn reset(&mut self) {
        self.analyzer.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_signature_computation() {
        let mut analyzer = ZigzagAnalyzer::new();
        
        // Create dummy attention matrix
        let attention = AttentionMatrix {
            layer: 0,
            num_heads: 4,
            seq_len: 8,
            weights: vec![0.1; 4 * 8 * 8],
        };

        analyzer.push_attention(attention);
        
        let signature = analyzer.compute_signature();
        
        assert!(signature.fragmentation_score >= 0.0);
        assert!(signature.fragmentation_score <= 1.0);
    }

    #[test]
    fn test_hallucination_detection() {
        let mut detector = HallucinationDetector::new(0.5);
        
        let attention = AttentionMatrix {
            layer: 0,
            num_heads: 4,
            seq_len: 8,
            weights: vec![0.1; 4 * 8 * 8],
        };

        detector.process_layer(attention);
        
        let decision = detector.decide();
        assert_ne!(decision, HallucinationDecision::Uncertain);
    }
}
