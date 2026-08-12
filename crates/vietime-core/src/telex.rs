use crate::tone::{self, Tone};

fn replace_last(s: &mut String, candidates: &str, replacement: char) -> bool {
    let mut chars: Vec<char> = s.chars().collect();
    if let Some(i) = chars.iter().rposition(|c| candidates.contains(*c)) {
        chars[i] = if chars[i].is_uppercase() {
            replacement.to_uppercase().next().unwrap()
        } else {
            replacement
        };
        *s = chars.into_iter().collect();
        true
    } else {
        false
    }
}

fn replace_first(s: &mut String, candidates: &str, replacement: char) -> bool {
    let mut chars: Vec<char> = s.chars().collect();
    if let Some(i) = chars.iter().position(|c| candidates.contains(*c)) {
        chars[i] = if chars[i].is_uppercase() {
            replacement.to_uppercase().next().unwrap()
        } else {
            replacement
        };
        *s = chars.into_iter().collect();
        true
    } else {
        false
    }
}

fn undo_first(s: &mut String, candidates: &str, replacement: char, literal: char) -> bool {
    if replace_first(s, candidates, replacement) {
        s.push(literal);
        true
    } else {
        false
    }
}

fn add_horns_to_uo(s: &mut String) -> bool {
    let mut chars: Vec<char> = s.chars().collect();
    let Some(i) = chars
        .windows(2)
        .rposition(|pair| matches!(pair[0], 'u' | 'U' | 'ư' | 'Ư') && matches!(pair[1], 'o' | 'O'))
    else {
        return false;
    };
    chars[i] = match chars[i] {
        'U' | 'Ư' => 'Ư',
        _ => 'ư',
    };
    chars[i + 1] = if chars[i + 1].is_uppercase() {
        'Ơ'
    } else {
        'ơ'
    };
    *s = chars.into_iter().collect();
    true
}

fn has_uo_with_horns(s: &str) -> bool {
    let chars: Vec<char> = s.chars().collect();
    chars
        .windows(2)
        .any(|pair| matches!(pair[0], 'ư' | 'Ư') && matches!(pair[1], 'ơ' | 'Ơ'))
}

fn add_horn_to_u_before_a(s: &mut String) -> bool {
    let mut chars: Vec<char> = s.chars().collect();
    let Some(i) = chars
        .windows(2)
        .rposition(|pair| matches!(pair[0], 'u' | 'U') && matches!(pair[1], 'a' | 'A'))
    else {
        return false;
    };
    // In "qu", u is a glide and w belongs to a (quawn -> quăn). For other
    // ua nuclei, a late w modifies u (suawr -> sửa, muaw -> mưa).
    if i > 0 && matches!(chars[i - 1], 'q' | 'Q') {
        return false;
    }
    chars[i] = if chars[i].is_uppercase() { 'Ư' } else { 'ư' };
    *s = chars.into_iter().collect();
    true
}

pub fn transform(raw: &str, modern: bool) -> String {
    let mut out = String::new();
    let mut active_tone = Tone::None;
    for ch in raw.chars() {
        let lower = ch.to_ascii_lowercase();
        let changed = match lower {
            // Telex users often type the second modifier after a tone or coda
            // (doandj, deder, loixo). Apply it to the matching base character
            // anywhere in the current syllable rather than requiring adjacency.
            'd' if undo_first(&mut out, "đĐ", 'd', ch) => true,
            'a' if undo_first(&mut out, "âÂ", 'a', ch) => true,
            'e' if undo_first(&mut out, "êÊ", 'e', ch) => true,
            'o' if out.ends_with('ư') || out.ends_with('Ư') => {
                out.push(if ch.is_uppercase() { 'Ơ' } else { 'ơ' });
                true
            }
            'o' if undo_first(&mut out, "ôÔ", 'o', ch) => true,
            'd' => replace_first(&mut out, "dD", 'đ'),
            'a' => replace_first(&mut out, "aA", 'â'),
            'e' => replace_first(&mut out, "eE", 'ê'),
            'o' => replace_first(&mut out, "oO", 'ô'),
            // Accept w after the coda too (duocw -> đươc). The already
            // converted form also consumes the conventional second w in uow.
            'w' if add_horns_to_uo(&mut out)
                || has_uo_with_horns(&out)
                || add_horn_to_u_before_a(&mut out) =>
            {
                true
            }
            'w' if undo_first(&mut out, "ăĂ", 'a', ch)
                || undo_first(&mut out, "ơƠ", 'o', ch)
                || undo_first(&mut out, "ưƯ", 'u', ch) =>
            {
                true
            }
            'w' => {
                replace_last(&mut out, "aA", 'ă')
                    || replace_last(&mut out, "oO", 'ơ')
                    || replace_last(&mut out, "uU", 'ư')
            }
            _ => false,
        };
        if changed {
            continue;
        }
        if let Some(t) = tone::from_telex(lower) {
            if out.chars().any(|c| "aăâeêioôơuưyAĂÂEÊIOÔƠUƯYáàảãạắằẳẵặấầẩẫậéèẻẽẹếềểễệíìỉĩịóòỏõọốồổỗộớờởỡợúùủũụứừửữựýỳỷỹỵ".contains(c)) {
                if active_tone == t {
                    active_tone = Tone::None;
                    out.push(ch);
                } else if t == Tone::None && active_tone == Tone::None {
                    out.push(ch);
                } else {
                    active_tone = t;
                }
                continue;
            }
        }
        out.push(ch);
    }
    tone::apply(&out, active_tone, modern)
}
