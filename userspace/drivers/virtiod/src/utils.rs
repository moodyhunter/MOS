// SPDX-License-Identifier: GPL-3.0-or-later

use std::num::ParseIntError;

pub(crate) fn parse_hex64(input: &str) -> Result<u64, ParseIntError> {
    if (input.starts_with("0x") || input.starts_with("0X")) && input.len() > 2 {
        return u64::from_str_radix(&input[2..], 16);
    }
    u64::from_str_radix(input, 16)
}
