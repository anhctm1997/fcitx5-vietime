//! Lightweight Vietnamese syllable validation used by Free Typing.
//!
//! Like UniKey's `lastWordIsNonVn`, this deliberately runs at a word boundary.
//! It is not a dictionary: it validates onset + contiguous nucleus + coda and
//! lets the engine restore raw keystrokes only when the shape is impossible.

const ONSETS: &[&str] = &[
    "", "b", "c", "ch", "d", "đ", "g", "gh", "gi", "h", "k", "kh", "l", "m", "n", "ng", "ngh",
    "nh", "p", "ph", "q", "qu", "r", "s", "t", "th", "tr", "v", "x",
];
const CODAS: &[&str] = &["", "c", "ch", "m", "n", "ng", "nh", "p", "t"];

fn base(c: char) -> Option<char> {
    for group in [
        "aáàảãạ",
        "ăắằẳẵặ",
        "âấầẩẫậ",
        "eéèẻẽẹ",
        "êếềểễệ",
        "iíìỉĩị",
        "oóòỏõọ",
        "ôốồổỗộ",
        "ơớờởỡợ",
        "uúùủũụ",
        "ưứừửữự",
        "yýỳỷỹỵ",
    ] {
        if group.contains(c) {
            return group.chars().next();
        }
    }
    None
}

/// Whether the rendered word still contains a Vietnamese transformation.
///
/// Repeating a Telex key can cancel a transformation (for example the second
/// `r` in `dockerr` turns the temporary `dỏcker` back into ASCII `docker`). In
/// that state Free Typing must keep the rendered word instead of restoring the
/// complete raw key sequence and leaking the cancelling key.
pub fn has_vietnamese_mark(word: &str) -> bool {
    word.chars()
        .any(|c| !c.is_ascii() && (base(c).is_some() || matches!(c, 'đ' | 'Đ')))
}

pub fn is_valid(word: &str) -> bool {
    let lower = word.to_lowercase();
    let chars: Vec<char> = lower.chars().collect();
    let Some(first_vowel) = chars.iter().position(|&c| base(c).is_some()) else {
        return true;
    };
    let onset: String = chars[..first_vowel].iter().collect();
    if !ONSETS.contains(&onset.as_str()) {
        return false;
    }
    let vowel_end = chars[first_vowel..]
        .iter()
        .position(|&c| base(c).is_none())
        .map_or(chars.len(), |offset| first_vowel + offset);
    let nucleus_len = vowel_end - first_vowel;
    if nucleus_len == 0 || nucleus_len > 3 {
        return false;
    }
    let coda: String = chars[vowel_end..].iter().collect();
    if !CODAS.contains(&coda.as_str()) {
        return false;
    }

    // Vietnamese checked syllables only allow sắc or nặng. Reject the other
    // tones so raw code-like input is restored at the boundary.
    if matches!(coda.as_str(), "c" | "ch" | "p" | "t") {
        let nucleus: String = chars[first_vowel..vowel_end].iter().collect();
        if nucleus
            .chars()
            .any(|c| "àảãằẳẵầẩẫèẻẽềểễìỉĩòỏõồổỗờởỡùủũừửữỳỷỹ".contains(c))
        {
            return false;
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use super::{has_vietnamese_mark, is_valid};

    #[test]
    fn classifies_shapes_at_word_boundary() {
        for word in ["tiếng", "đường", "hóa", "khỏe", "nguyễn", "dụng"] {
            assert!(is_valid(word), "{word}");
        }
        for word in ["dỏcker", "ásync", "rẻdis", "póstgres", "strúct"] {
            assert!(!is_valid(word), "{word}");
        }
    }

    #[test]
    fn detects_remaining_vietnamese_transformation() {
        assert!(has_vietnamese_mark("dỏcker"));
        assert!(has_vietnamese_mark("đường"));
        assert!(!has_vietnamese_mark("docker"));
    }
}
