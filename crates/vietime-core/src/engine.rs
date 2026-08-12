use crate::{replace::diff, syllable, telex, unicode};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputMode {
    Vietnamese,
    Raw,
    Disabled,
}
#[derive(Debug, Clone)]
pub struct Config {
    pub modern_tone: bool,
    pub free_typing: bool,
    pub deferred_commit: bool,
    pub mode: InputMode,
    pub max_raw_chars: usize,
}
impl Default for Config {
    fn default() -> Self {
        Self {
            modern_tone: true,
            free_typing: true,
            deferred_commit: false,
            mode: InputMode::Vietnamese,
            max_raw_chars: 64,
        }
    }
}
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Modifiers {
    pub ctrl: bool,
    pub alt: bool,
    pub super_key: bool,
    pub shift: bool,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpecialKey {
    Character,
    Backspace,
    Delete,
    Enter,
    Tab,
    Escape,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Other,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KeyInput {
    pub ch: Option<char>,
    pub key: SpecialKey,
    pub modifiers: Modifiers,
}
impl KeyInput {
    pub fn character(ch: char) -> Self {
        Self {
            ch: Some(ch),
            key: SpecialKey::Character,
            modifiers: Modifiers::default(),
        }
    }
}
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    Pass,
    Consume,
    Commit {
        text: String,
        consume: bool,
    },
    Replace {
        delete_before: usize,
        text: String,
        consume: bool,
    },
    Reset,
}

pub struct Engine {
    raw: String,
    rendered: String,
    deferred_leading_d: bool,
    config: Config,
}
impl Default for Engine {
    fn default() -> Self {
        Self::new(Config::default())
    }
}
impl Engine {
    pub fn new(config: Config) -> Self {
        Self {
            raw: String::new(),
            rendered: String::new(),
            deferred_leading_d: false,
            config,
        }
    }
    pub fn reset(&mut self) {
        self.raw.clear();
        self.rendered.clear();
        self.deferred_leading_d = false;
    }
    pub fn rendered(&self) -> &str {
        &self.rendered
    }
    pub fn process(&mut self, input: KeyInput) -> Action {
        if self.config.deferred_commit {
            return self.process_deferred(input);
        }
        if input.modifiers.ctrl || input.modifiers.alt || input.modifiers.super_key {
            if self.deferred_leading_d {
                let text = self.rendered.clone();
                self.reset();
                return Action::Commit {
                    text,
                    consume: false,
                };
            }
            self.reset();
            return Action::Pass;
        }
        if input.key == SpecialKey::Backspace && !self.raw.is_empty() {
            if self.deferred_leading_d {
                self.reset();
                return Action::Consume;
            }
            let old = self.rendered.clone();
            self.raw.pop();
            self.rendered = unicode::nfc(&telex::transform(&self.raw, self.config.modern_tone));
            let replacement = diff(&old, &self.rendered);
            return Action::Replace {
                delete_before: replacement.delete_before,
                text: replacement.insert,
                consume: true,
            };
        }
        if input.key != SpecialKey::Character {
            if self.deferred_leading_d {
                let text = self.rendered.clone();
                self.reset();
                return Action::Commit {
                    text,
                    consume: false,
                };
            }
            self.reset();
            return Action::Pass;
        }
        let Some(ch) = input.ch else {
            return Action::Pass;
        };
        if !ch.is_ascii_alphabetic() || self.config.mode != InputMode::Vietnamese {
            if self.deferred_leading_d {
                let text = self.rendered.clone();
                self.reset();
                return Action::Commit {
                    text,
                    consume: false,
                };
            }
            if self.config.mode == InputMode::Vietnamese
                && self.config.free_typing
                && !self.raw.is_empty()
                && self.raw != self.rendered
                && syllable::has_vietnamese_mark(&self.rendered)
                && !syllable::is_valid(&self.rendered)
            {
                let delete_before = self.rendered.chars().count();
                let text = std::mem::take(&mut self.raw);
                self.rendered.clear();
                return Action::Replace {
                    delete_before,
                    text,
                    consume: false,
                };
            }
            self.reset();
            return Action::Pass;
        }
        if self.raw.chars().count() >= self.config.max_raw_chars {
            self.reset();
            return Action::Pass;
        }
        if self.raw.is_empty() && ch.eq_ignore_ascii_case(&'d') {
            self.raw.push(ch);
            self.rendered.push(ch);
            self.deferred_leading_d = true;
            return Action::Consume;
        }
        let had_deferred_leading_d = self.deferred_leading_d;
        self.deferred_leading_d = false;
        let old = self.rendered.clone();
        self.raw.push(ch);
        self.rendered = unicode::nfc(&telex::transform(&self.raw, self.config.modern_tone));
        if had_deferred_leading_d {
            return Action::Commit {
                text: self.rendered.clone(),
                consume: true,
            };
        }
        let replacement = diff(&old, &self.rendered);
        if replacement.delete_before == 0 && replacement.insert == ch.to_string() {
            Action::Commit {
                text: replacement.insert,
                consume: true,
            }
        } else {
            Action::Replace {
                delete_before: replacement.delete_before,
                text: replacement.insert,
                consume: true,
            }
        }
    }

    fn process_deferred(&mut self, input: KeyInput) -> Action {
        if input.modifiers.ctrl || input.modifiers.alt || input.modifiers.super_key {
            return self.flush_deferred(false);
        }
        if input.key == SpecialKey::Backspace {
            if self.raw.is_empty() {
                return Action::Pass;
            }
            self.raw.pop();
            self.rendered = unicode::nfc(&telex::transform(&self.raw, self.config.modern_tone));
            return Action::Consume;
        }
        if input.key != SpecialKey::Character {
            return self.flush_deferred(false);
        }
        let Some(ch) = input.ch else {
            return Action::Pass;
        };
        if !ch.is_ascii_alphabetic() || self.config.mode != InputMode::Vietnamese {
            return self.flush_deferred(false);
        }
        if self.raw.chars().count() >= self.config.max_raw_chars {
            return self.flush_deferred(false);
        }
        self.raw.push(ch);
        self.rendered = unicode::nfc(&telex::transform(&self.raw, self.config.modern_tone));
        Action::Consume
    }

    fn flush_deferred(&mut self, consume: bool) -> Action {
        if self.raw.is_empty() {
            self.reset();
            return Action::Pass;
        }
        let invalid = self.config.free_typing
            && self.raw != self.rendered
            && syllable::has_vietnamese_mark(&self.rendered)
            && !syllable::is_valid(&self.rendered);
        let text = if invalid {
            std::mem::take(&mut self.raw)
        } else {
            self.raw.clear();
            std::mem::take(&mut self.rendered)
        };
        self.rendered.clear();
        Action::Commit { text, consume }
    }
}
