#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Replacement {
    pub delete_before: usize,
    pub insert: String,
}

pub fn diff(old: &str, new: &str) -> Replacement {
    let common = old
        .chars()
        .zip(new.chars())
        .take_while(|(a, b)| a == b)
        .count();
    Replacement {
        delete_before: old.chars().count() - common,
        insert: new.chars().skip(common).collect(),
    }
}
