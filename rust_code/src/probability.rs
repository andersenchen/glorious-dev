// rust_code/src/probability.rs
use contracts::*;

/// Fixed-point scaling factor.
pub const FIXED_SCALE: u32 = 1 << 16;

/// Structure representing the context content.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ContextContent {
    pub context_length: usize,
    pub count_ones: u32,
}

/// Type alias for probability functions.
pub type ProbabilityFunction = fn(&ContextContent) -> u32;

/// Optimized function to obtain the fixed-point probability of bit '1'.
///
/// # Arguments
///
/// * `context_content` - Reference to the `ContextContent` structure containing
///                        context length and count of '1's.
///
/// # Returns
///
/// * `u32` - Fixed-point probability of bit '1', scaled by `FIXED_SCALE`.
#[requires(context_content.count_ones <= context_content.context_length as u32)]
#[ensures((1..FIXED_SCALE).contains(&ret))]
pub fn example_get_probability_fixed(context_content: &ContextContent) -> u32 {
    let context_length = context_content.context_length;
    let count_ones = context_content.count_ones;

    // If all bits are '1', set probability to FIXED_SCALE - 1
    if count_ones == context_length as u32 {
        FIXED_SCALE - 1
    } else {
        // Calculate numerator and denominator with Laplace smoothing
        let numerator = count_ones + 1;
        let denominator = (context_length + 2) as u32;

        // Compute probability with rounding
        let prob_fixed = ((numerator as u64 * FIXED_SCALE as u64) + ((denominator as u64) / 2))
            / denominator as u64;

        // Ensure 1 <= prob_fixed <= FIXED_SCALE - 1
        prob_fixed.clamp(1, FIXED_SCALE as u64 - 1) as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use proptest::prelude::*;

    #[test]
    fn test_probability_clamping_low() {
        let context = ContextContent {
            context_length: 4,
            count_ones: 0,
        };
        let prob = example_get_probability_fixed(&context);
        assert_eq!(prob, 10923);
    }

    #[test]
    fn test_probability_clamping_high() {
        let context = ContextContent {
            context_length: 1,
            count_ones: 1,
        };
        let prob = example_get_probability_fixed(&context);
        assert_eq!(prob, FIXED_SCALE - 1); // Expects 65535
    }

    #[test]
    fn test_probability_normal() {
        let context = ContextContent {
            context_length: 4,
            count_ones: 2,
        };
        let prob = example_get_probability_fixed(&context);
        assert_eq!(prob, 32768);
    }

    #[test]
    fn test_probability_bounds() {
        for context_length in 1..=100 {
            for count_ones in 0..=context_length {
                let context = ContextContent {
                    context_length,
                    count_ones: count_ones as u32,
                };
                let prob = example_get_probability_fixed(&context);
                assert!(
                    (1..FIXED_SCALE).contains(&prob),
                    "Probability out of bounds for context: {:?}",
                    context
                );
            }
        }
    }

    #[test]
    fn test_probability_monotonicity() {
        for context_length in 1..=10 {
            let mut prev_prob = 0;
            for count_ones in 0..=context_length {
                let context = ContextContent {
                    context_length,
                    count_ones: count_ones as u32,
                };
                let prob = example_get_probability_fixed(&context);
                assert!(
                    prob > prev_prob,
                    "Probability not monotonically increasing for context: {:?}",
                    context
                );
                prev_prob = prob;
            }
        }
    }

    proptest! {
        #[test]
        fn test_probability_properties(context_length in 1usize..1000, count_ones in 0u32..1000u32) {
            let context = ContextContent {
                context_length,
                count_ones: count_ones.min(context_length as u32),
            };
            let prob = example_get_probability_fixed(&context);
            prop_assert!((1..FIXED_SCALE).contains(&prob));

            if count_ones < context_length as u32 {
                let next_context = ContextContent {
                    context_length,
                    count_ones: count_ones + 1,
                };
                let next_prob = example_get_probability_fixed(&next_context);
                prop_assert!(next_prob > prob, "Probability should increase with count_ones");
            }
        }
    }
}

// Additional module for formal verification
pub mod verification {
    use super::*;
    #[requires(context.count_ones <= context.context_length as u32)]
    #[ensures((1..FIXED_SCALE).contains(&ret))]
    pub fn verified_get_probability_fixed(context: &ContextContent) -> u32 {
        example_get_probability_fixed(context)
    }

    #[requires(context1.context_length == context2.context_length)]
    #[requires(context1.count_ones < context2.count_ones)]
    #[ensures(
        verified_get_probability_fixed(context1) <
        verified_get_probability_fixed(context2)
    )]
    pub fn prove_monotonicity(context1: &ContextContent, context2: &ContextContent) -> bool {
        // Runtime check for monotonicity
        let prob1 = verified_get_probability_fixed(context1);
        let prob2 = verified_get_probability_fixed(context2);

        prob1 < prob2
    }

    #[cfg(test)]
    mod tests {
        use super::*;

        #[test]
        fn test_monotonicity() {
            for context_length in 1..=10 {
                for count_ones in 0..context_length {
                    let context1 = ContextContent {
                        context_length,
                        count_ones: count_ones as u32,
                    };
                    let context2 = ContextContent {
                        context_length,
                        count_ones: (count_ones + 1) as u32,
                    };
                    assert!(
                        prove_monotonicity(&context1, &context2),
                        "Monotonicity violated for {:?} and {:?}",
                        context1,
                        context2
                    );
                }
            }
        }
    }
}
