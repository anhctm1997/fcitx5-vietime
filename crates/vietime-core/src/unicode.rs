use unicode_normalization::UnicodeNormalization;

pub fn nfc(text: &str) -> String {
    text.nfc().collect()
}
