// rust_code/src/probability.rs

/// Fixed-point scaling factor.
/// Must be consistent across all modules.
pub const FIXED_SCALE: u32 = 1 << 16;

/// Structure representing the context content.
#[derive(Debug, Clone, Copy)]
pub struct ContextContent {
    pub context_length: usize,
    pub count_ones: u32,
}

/// Type alias for probability functions.
pub type ProbabilityFunction = fn(&ContextContent) -> u32;

/// Optimized function to obtain the fixed-point probability of bit '1'.
///
/// This function replaces floating-point arithmetic with integer operations
/// to enhance performance while maintaining behavior.
///
/// # Arguments
///
/// * `context_content` - Reference to the ContextContent structure containing
///                        context length and count of '1's.
///
/// # Returns
///
/// * `u32` - Fixed-point probability of bit '1', scaled by FIXED_SCALE.
pub fn example_get_probability_fixed(context_content: &ContextContent) -> u32 {
    let context_length = context_content.context_length;
    let count_ones = context_content.count_ones;

    // Calculate numerator and denominator
    let numerator = count_ones + 1;
    let denominator = (context_length + 2) as u32;

    // Compute probability with rounding
    let prob_fixed =
        ((numerator as u64 * FIXED_SCALE as u64) + ((denominator as u64) / 2)) / denominator as u64;

    // Branchless clamping to ensure 1 <= prob_fixed <= FIXED_SCALE - 1
    if prob_fixed < 1 {
        1
    } else if prob_fixed >= FIXED_SCALE as u64 {
        FIXED_SCALE - 1
    } else {
        prob_fixed as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_probability_clamping_low() {
        let context = ContextContent {
            context_length: 4,
            count_ones: 0,
        };
        let prob = example_get_probability_fixed(&context);
        // Expected: ((0 + 1)*65536 + (6 /2))/6 = (65536 +3)/6 = 65539 /6 ≈10923
        assert_eq!(prob, 10923);
    }

    #[test]
    fn test_probability_clamping_high() {
        let context = ContextContent {
            context_length: 1,
            count_ones: 10,
        };
        let prob = example_get_probability_fixed(&context);
        assert_eq!(prob, FIXED_SCALE - 1); // 65535
    }

    #[test]
    fn test_probability_normal() {
        let context = ContextContent {
            context_length: 4,
            count_ones: 2,
        };
        let prob = example_get_probability_fixed(&context);
        // Expected: ((2 + 1)*65536)/6 = 196608 /6 = 32768
        let expected = 32768;
        assert_eq!(prob, expected);
    }
}
