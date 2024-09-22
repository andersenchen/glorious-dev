// rust_code/src/lib.rs

//! Arithmetic Coding Library
//!
//! This library provides functionality for arithmetic encoding and decoding.

pub mod arithmetic_coding;
pub mod probability;

pub use arithmetic_coding::{arithmetic_decode, arithmetic_encode};
pub use probability::{example_get_probability_fixed, ContextContent, ProbabilityFunction};
