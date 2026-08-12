use std::{
    panic::{catch_unwind, AssertUnwindSafe},
    ptr,
};
use vietime_core::{Action, Engine, KeyInput, Modifiers, SpecialKey};

pub const VIETIME_ACTION_PASS: u32 = 0;
pub const VIETIME_ACTION_CONSUME: u32 = 1;
pub const VIETIME_ACTION_COMMIT: u32 = 2;
pub const VIETIME_ACTION_REPLACE: u32 = 3;
pub const VIETIME_ACTION_REPLACE_AND_PASS: u32 = 4;
pub const VIETIME_ACTION_COMMIT_AND_PASS: u32 = 5;

#[repr(C)]
pub struct VietimeAction {
    pub action_type: u32,
    pub delete_before: u32,
    pub text_ptr: *mut u8,
    pub text_len: usize,
    text_capacity: usize,
}

impl VietimeAction {
    fn empty(kind: u32) -> Self {
        Self {
            action_type: kind,
            delete_before: 0,
            text_ptr: ptr::null_mut(),
            text_len: 0,
            text_capacity: 0,
        }
    }
    fn text(kind: u32, delete_before: usize, text: String) -> Self {
        let mut bytes = text.into_bytes();
        let result = Self {
            action_type: kind,
            delete_before: delete_before.min(u32::MAX as usize) as u32,
            text_ptr: bytes.as_mut_ptr(),
            text_len: bytes.len(),
            text_capacity: bytes.capacity(),
        };
        std::mem::forget(bytes);
        result
    }
}

fn special(key: u32) -> SpecialKey {
    match key {
        1 => SpecialKey::Backspace,
        2 => SpecialKey::Delete,
        3 => SpecialKey::Enter,
        4 => SpecialKey::Tab,
        5 => SpecialKey::Escape,
        6 => SpecialKey::Left,
        7 => SpecialKey::Right,
        8 => SpecialKey::Up,
        9 => SpecialKey::Down,
        10 => SpecialKey::Home,
        11 => SpecialKey::End,
        _ => SpecialKey::Other,
    }
}

#[no_mangle]
pub extern "C" fn vietime_engine_new() -> *mut Engine {
    Box::into_raw(Box::new(Engine::default()))
}

#[no_mangle]
pub extern "C" fn vietime_engine_new_deferred() -> *mut Engine {
    Box::into_raw(Box::new(Engine::new(vietime_core::Config {
        deferred_commit: true,
        ..Default::default()
    })))
}

#[no_mangle]
/// # Safety
/// `engine` must be null or a pointer returned by `vietime_engine_new` that has not been freed.
pub unsafe extern "C" fn vietime_engine_free(engine: *mut Engine) {
    if !engine.is_null() {
        drop(Box::from_raw(engine));
    }
}

#[no_mangle]
/// # Safety
/// `engine` must be null or a live pointer returned by `vietime_engine_new`.
pub unsafe extern "C" fn vietime_engine_reset(engine: *mut Engine) {
    if let Some(engine) = engine.as_mut() {
        engine.reset();
    }
}

/// `unicode` is a Unicode scalar (0 for a special key). Modifiers: Ctrl=1, Alt=2, Super=4, Shift=8.
///
/// # Safety
/// `engine` must be null or a live pointer returned by `vietime_engine_new`. The returned
/// action must be released exactly once with `vietime_action_free`.
#[no_mangle]
pub unsafe extern "C" fn vietime_process_key(
    engine: *mut Engine,
    unicode: u32,
    key: u32,
    modifiers: u32,
) -> VietimeAction {
    let Some(engine) = engine.as_mut() else {
        return VietimeAction::empty(VIETIME_ACTION_PASS);
    };
    let result = catch_unwind(AssertUnwindSafe(|| {
        let ch = char::from_u32(unicode).filter(|_| key == 0);
        engine.process(KeyInput {
            ch,
            key: if key == 0 {
                SpecialKey::Character
            } else {
                special(key)
            },
            modifiers: Modifiers {
                ctrl: modifiers & 1 != 0,
                alt: modifiers & 2 != 0,
                super_key: modifiers & 4 != 0,
                shift: modifiers & 8 != 0,
            },
        })
    }));
    match result {
        Err(_) => {
            engine.reset();
            VietimeAction::empty(VIETIME_ACTION_PASS)
        }
        Ok(Action::Pass | Action::Reset) => VietimeAction::empty(VIETIME_ACTION_PASS),
        Ok(Action::Consume) => {
            VietimeAction::text(VIETIME_ACTION_CONSUME, 0, engine.rendered().to_string())
        }
        Ok(Action::Commit { text, consume }) => VietimeAction::text(
            if consume {
                VIETIME_ACTION_COMMIT
            } else {
                VIETIME_ACTION_COMMIT_AND_PASS
            },
            0,
            text,
        ),
        Ok(Action::Replace {
            delete_before,
            text,
            consume,
        }) => VietimeAction::text(
            if consume {
                VIETIME_ACTION_REPLACE
            } else {
                VIETIME_ACTION_REPLACE_AND_PASS
            },
            delete_before,
            text,
        ),
    }
}

#[no_mangle]
/// # Safety
/// `action` must be an action returned by `vietime_process_key` and must not be freed twice.
pub unsafe extern "C" fn vietime_action_free(action: VietimeAction) {
    if !action.text_ptr.is_null() {
        drop(Vec::from_raw_parts(
            action.text_ptr,
            action.text_len,
            action.text_capacity,
        ));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe fn action_text(action: &VietimeAction) -> String {
        String::from_utf8(std::slice::from_raw_parts(action.text_ptr, action.text_len).to_vec())
            .unwrap()
    }

    #[test]
    fn dd_crosses_the_ffi_as_utf8_d_stroke() {
        unsafe {
            let engine = vietime_engine_new();
            let first = vietime_process_key(engine, 'd' as u32, 0, 0);
            assert_eq!(first.action_type, VIETIME_ACTION_CONSUME);
            assert_eq!(action_text(&first), "d");
            vietime_action_free(first);

            let second = vietime_process_key(engine, 'd' as u32, 0, 0);
            assert_eq!(second.action_type, VIETIME_ACTION_COMMIT);
            assert_eq!(second.delete_before, 0);
            assert_eq!(action_text(&second).as_bytes(), "đ".as_bytes());
            vietime_action_free(second);

            let space = vietime_process_key(engine, ' ' as u32, 0, 0);
            assert_eq!(space.action_type, VIETIME_ACTION_PASS);
            assert_eq!(space.text_len, 0);
            vietime_action_free(space);
            vietime_engine_free(engine);
        }
    }

    #[test]
    fn deferred_engine_emits_only_at_the_word_boundary() {
        unsafe {
            let engine = vietime_engine_new_deferred();
            for (ch, expected) in [('d', "d"), ('d', "đ")] {
                let action = vietime_process_key(engine, ch as u32, 0, 0);
                assert_eq!(action.action_type, VIETIME_ACTION_CONSUME);
                assert_eq!(action_text(&action), expected);
                vietime_action_free(action);
            }
            let space = vietime_process_key(engine, ' ' as u32, 0, 0);
            assert_eq!(space.action_type, VIETIME_ACTION_COMMIT_AND_PASS);
            assert_eq!(action_text(&space), "đ");
            vietime_action_free(space);
            vietime_engine_free(engine);
        }
    }
}
