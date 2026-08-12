#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tone {
    None,
    Acute,
    Grave,
    Hook,
    Tilde,
    Dot,
}

pub fn from_telex(ch: char) -> Option<Tone> {
    match ch.to_ascii_lowercase() {
        's' => Some(Tone::Acute),
        'f' => Some(Tone::Grave),
        'r' => Some(Tone::Hook),
        'x' => Some(Tone::Tilde),
        'j' => Some(Tone::Dot),
        'z' => Some(Tone::None),
        _ => None,
    }
}

fn vowel(c: char) -> bool {
    "aăâeêioôơuưyAĂÂEÊIOÔƠUƯY".contains(c)
}

fn target(chars: &[char], modern: bool) -> Option<usize> {
    let mut vowels: Vec<usize> = chars
        .iter()
        .enumerate()
        .filter_map(|(i, &c)| vowel(c).then_some(i))
        .collect();
    if vowels.is_empty() {
        return None;
    }
    // In "qu" and "gi", u/i is a glide when another vowel follows. It must
    // never receive the tone (quá, khỏe, giá, giọng).
    if vowels.len() > 1 && vowels[0] == 1 {
        let first = chars[0].to_ascii_lowercase();
        let glide = chars[1].to_ascii_lowercase();
        if (first == 'q' && glide == 'u') || (first == 'g' && glide == 'i') {
            vowels.remove(0);
        }
    }
    // In the Vietnamese nucleus "ươ", the tone belongs to ơ (đường, tưởng),
    // not to the first modified vowel ư.
    for pair in vowels.windows(2) {
        if matches!(chars[pair[0]], 'ư' | 'Ư') && matches!(chars[pair[1]], 'ơ' | 'Ơ') {
            return Some(pair[1]);
        }
    }
    for &i in &vowels {
        if "ăâêôơĂÂÊÔƠ".contains(chars[i]) {
            return Some(i);
        }
    }
    if vowels.len() == 1 {
        return Some(vowels[0]);
    }
    let last_vowel = *vowels.last().unwrap();
    let open = last_vowel + 1 == chars.len();
    if !modern && open {
        let pair: String = vowels
            .iter()
            .rev()
            .take(2)
            .rev()
            .map(|&i| chars[i].to_ascii_lowercase())
            .collect();
        if pair == "oa" || pair == "oe" || pair == "uy" {
            return vowels.last().copied();
        }
    }
    if vowels.len() >= 3 {
        vowels.get(vowels.len() - 2).copied()
    } else if open {
        vowels.first().copied()
    } else {
        vowels.last().copied()
    }
}

fn strip(c: char) -> char {
    const GROUPS: [&str; 12] = [
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
    ];
    for g in GROUPS {
        if g.contains(c.to_lowercase().next().unwrap_or(c)) {
            let base = g.chars().next().unwrap();
            return if c.is_uppercase() {
                base.to_uppercase().next().unwrap()
            } else {
                base
            };
        }
    }
    c
}

fn mark(c: char, tone: Tone) -> char {
    const ROWS: [(&str, &str); 12] = [
        ("aáàảãạ", "a"),
        ("ăắằẳẵặ", "ă"),
        ("âấầẩẫậ", "â"),
        ("eéèẻẽẹ", "e"),
        ("êếềểễệ", "ê"),
        ("iíìỉĩị", "i"),
        ("oóòỏõọ", "o"),
        ("ôốồổỗộ", "ô"),
        ("ơớờởỡợ", "ơ"),
        ("uúùủũụ", "u"),
        ("ưứừửữự", "ư"),
        ("yýỳỷỹỵ", "y"),
    ];
    let base = strip(c).to_lowercase().next().unwrap_or(c);
    let idx = match tone {
        Tone::None => 0,
        Tone::Acute => 1,
        Tone::Grave => 2,
        Tone::Hook => 3,
        Tone::Tilde => 4,
        Tone::Dot => 5,
    };
    for (row, b) in ROWS {
        if b.starts_with(base) {
            let out = row.chars().nth(idx).unwrap();
            return if c.is_uppercase() {
                out.to_uppercase().next().unwrap()
            } else {
                out
            };
        }
    }
    c
}

pub fn apply(word: &str, tone: Tone, modern: bool) -> String {
    let mut chars: Vec<char> = word.chars().map(strip).collect();
    if tone != Tone::None {
        if let Some(i) = target(&chars, modern) {
            chars[i] = mark(chars[i], tone);
        }
    }
    chars.into_iter().collect()
}
