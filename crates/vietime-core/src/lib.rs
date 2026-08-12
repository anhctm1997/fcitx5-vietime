#![forbid(unsafe_code)]

mod engine;
mod replace;
mod syllable;
mod telex;
mod tone;
mod unicode;

pub use engine::{Action, Config, Engine, InputMode, KeyInput, Modifiers, SpecialKey};
pub use replace::{diff, Replacement};

#[cfg(test)]
mod tests {
    use super::*;

    fn type_keys(input: &str) -> String {
        let mut engine = Engine::default();
        let mut output = String::new();
        for ch in input.chars() {
            match engine.process(KeyInput::character(ch)) {
                Action::Pass => output.push(ch),
                Action::Consume => {}
                Action::Commit { text, consume } => {
                    output.push_str(&text);
                    if !consume {
                        output.push(ch);
                    }
                }
                Action::Replace {
                    delete_before,
                    text,
                    consume,
                } => {
                    for _ in 0..delete_before {
                        output.pop();
                    }
                    output.push_str(&text);
                    if !consume {
                        output.push(ch);
                    }
                }
                Action::Reset => {}
            }
        }
        output
    }

    #[test]
    fn required_corpus() {
        for (raw, expected) in [
            ("tieesng", "tiếng"),
            ("nguyeenx", "nguyễn"),
            ("dduwowngf", "đường"),
            ("truowngf", "trường"),
            ("Vieetj", "Việt"),
            ("ddawng", "đăng"),
            ("chuyeenr", "chuyển"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn tone_keys_at_end_of_syllable() {
        for (raw, expected) in [
            ("has", "há"),
            ("haf", "hà"),
            ("har", "hả"),
            ("hax", "hã"),
            ("haj", "hạ"),
            ("hoas", "hóa"),
            ("hoaf", "hòa"),
            ("hoar", "hỏa"),
            ("hoax", "hõa"),
            ("hoaj", "họa"),
            ("toans", "toán"),
            ("hoatj", "hoạt"),
            ("tieengs", "tiếng"),
            ("chuyeenj", "chuyện"),
            ("nguyeenx", "nguyễn"),
            ("thuyr", "thủy"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn horn_and_breve_families() {
        for (raw, expected) in [
            ("how", "hơ"),
            ("hows", "hớ"),
            ("howf", "hờ"),
            ("howr", "hở"),
            ("howx", "hỡ"),
            ("howj", "hợ"),
            ("mow", "mơ"),
            ("mowis", "mới"),
            ("dduwowcj", "được"),
            ("tuowngr", "tưởng"),
            ("cuar", "của"),
            ("cuwar", "cửa"),
            ("suawr", "sửa"),
            ("muaw", "mưa"),
            ("quawn", "quăn"),
            ("awns", "ắn"),
            ("awngf", "ằng"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn uo_horns_are_completed_immediately_or_by_a_late_w() {
        for (raw, expected) in [
            ("dd", "đ"),
            ("dduwo", "đươ"),
            ("dduwow", "đươ"),
            ("dduocw", "đươc"),
            ("dduocwj", "được"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn leading_dd_is_committed_without_deleting_surrounding_text() {
        let mut engine = Engine::default();
        assert_eq!(engine.process(KeyInput::character('d')), Action::Consume);
        assert_eq!(
            engine.process(KeyInput::character('d')),
            Action::Commit {
                text: "đ".into(),
                consume: true,
            }
        );

        assert_eq!(type_keys("da"), "da");
        assert_eq!(type_keys("d "), "d ");
    }

    #[test]
    fn qu_and_gi_glides_do_not_receive_tone() {
        for (raw, expected) in [
            ("quas", "quá"),
            ("quaf", "quà"),
            ("quanr", "quản"),
            ("quyeets", "quyết"),
            ("khoer", "khỏe"),
            ("gias", "giá"),
            ("giaf", "già"),
            ("giojng", "giọng"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn tone_can_be_typed_before_the_coda() {
        for (raw, expected) in [
            ("tasn", "tán"),
            ("tafng", "tàng"),
            ("toasnj", "toạn"),
            ("tieesng", "tiếng"),
            ("dduwowjfng", "đường"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn modifiers_can_be_typed_late_in_the_syllable() {
        for (raw, expected) in [
            ("doandj", "đoạn"),
            ("deder", "để"),
            ("loixo", "lỗi"),
            ("dafna", "dần"),
            ("danaf", "dần"),
            ("vanw", "văn"),
            ("cacsh", "cách"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn user_typing_style_regression_corpus() {
        for (raw, expected) in [
            ("tooi", "tôi"),
            ("sex", "sẽ"),
            ("gox", "gõ"),
            ("thuowngf", "thường"),
            ("suwr", "sử"),
            ("dungj", "dụng"),
            ("ddeer", "để"),
            ("tieesng", "tiếng"),
            ("vieejt", "việt"),
            ("haxy", "hãy"),
            ("duwaj", "dựa"),
            ("vaof", "vào"),
            ("nhuwng", "nhưng"),
            // A single leading d is intentionally preserved; "đoạn" needs
            // either ddoanj or the user's late-modifier form doandj.
            ("doanj", "doạn"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn repeated_telex_key_restores_literal_like_unikey() {
        for (raw, expected) in [
            ("ass", "as"),
            ("asf", "à"),
            ("azz", "azz"),
            ("aaa", "aa"),
            ("eee", "ee"),
            ("ooo", "oo"),
            ("ddd", "dd"),
            ("aww", "aw"),
            ("oww", "ow"),
            ("uww", "uw"),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn backspace_replays_raw_keystrokes() {
        let mut engine = Engine::default();
        assert_eq!(
            engine.process(KeyInput::character('a')),
            Action::Commit {
                text: "a".into(),
                consume: true
            }
        );
        assert_eq!(
            engine.process(KeyInput::character('a')),
            Action::Replace {
                delete_before: 1,
                text: "â".into(),
                consume: true
            }
        );
        assert_eq!(
            engine.process(KeyInput {
                ch: None,
                key: SpecialKey::Backspace,
                modifiers: Modifiers::default()
            }),
            Action::Replace {
                delete_before: 1,
                text: "a".into(),
                consume: true
            }
        );
        assert_eq!(engine.rendered(), "a");
    }

    #[test]
    fn actions_commit_directly_and_leave_one_space_to_the_client() {
        let mut engine = Engine::default();
        assert_eq!(engine.process(KeyInput::character('d')), Action::Consume);
        assert_eq!(
            engine.process(KeyInput::character('d')),
            Action::Commit {
                text: "đ".into(),
                consume: true
            }
        );
        assert_eq!(engine.process(KeyInput::character(' ')), Action::Pass);
        assert!(engine.rendered().is_empty());
    }

    #[test]
    fn deferred_terminal_buffers_without_replace_or_flicker() {
        let mut engine = Engine::new(Config {
            deferred_commit: true,
            ..Default::default()
        });
        for ch in "dduwowngf".chars() {
            assert_eq!(engine.process(KeyInput::character(ch)), Action::Consume);
        }
        assert_eq!(engine.rendered(), "đường");
        assert_eq!(
            engine.process(KeyInput::character(' ')),
            Action::Commit {
                text: "đường".into(),
                consume: false
            }
        );
        assert!(engine.rendered().is_empty());
    }

    #[test]
    fn deferred_terminal_backspace_only_changes_internal_buffer() {
        let mut engine = Engine::new(Config {
            deferred_commit: true,
            ..Default::default()
        });
        assert_eq!(engine.process(KeyInput::character('d')), Action::Consume);
        assert_eq!(engine.process(KeyInput::character('d')), Action::Consume);
        assert_eq!(engine.rendered(), "đ");
        assert_eq!(
            engine.process(KeyInput {
                ch: None,
                key: SpecialKey::Backspace,
                modifiers: Modifiers::default()
            }),
            Action::Consume
        );
        assert_eq!(engine.rendered(), "d");
    }

    #[test]
    fn free_typing_restores_non_vietnamese_words_at_boundary() {
        for raw in [
            "docker", "async", "redis", "postgres", "json", "yaml", "struct",
        ] {
            assert_eq!(type_keys(&format!("{raw} ")), format!("{raw} "), "{raw}");
        }
        assert_eq!(type_keys("tieesng "), "tiếng ");
        assert_eq!(type_keys("dduwowngf."), "đường.");
    }

    #[test]
    fn repeated_tone_cancel_does_not_leak_raw_key_at_boundary() {
        for (raw, expected) in [
            ("dockerr ", "docker "),
            ("dockerr. ", "docker. "),
            ("ass ", "as "),
        ] {
            assert_eq!(type_keys(raw), expected, "{raw}");
        }
    }

    #[test]
    fn separators_and_shortcuts_pass_and_reset() {
        let mut e = Engine::default();
        e.process(KeyInput::character('a'));
        assert_eq!(e.process(KeyInput::character(' ')), Action::Pass);
        assert!(e.rendered().is_empty());
        e.process(KeyInput::character('a'));
        assert_eq!(
            e.process(KeyInput {
                ch: Some('s'),
                key: SpecialKey::Character,
                modifiers: Modifiers {
                    ctrl: true,
                    ..Default::default()
                }
            }),
            Action::Pass
        );
        assert!(e.rendered().is_empty());
    }

    #[test]
    fn diff_counts_chars_not_bytes() {
        assert_eq!(
            diff("tiê", "tiế"),
            Replacement {
                delete_before: 1,
                insert: "ế".into()
            }
        );
    }
}
