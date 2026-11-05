// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! The password dialog sudo opens for Aurora's scripts, behind a small wrapper. sudo runs the dialog with the
//! terminal as its stderr, and a Qt dialog started by sudo prints warnings there ("qt.accessibility.atspi: Error in
//! contacting registry"), right through the install's progress. The wrapper keeps the dialog's stderr out of the
//! terminal; the password still comes back on stdout.

use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};

use anyhow::{Context, Result};

/// The wrapper's text: runs `program` with its arguments and drops what it prints on stderr.
#[must_use]
pub fn script(program: &Path) -> String {
    // Single quotes keep any path literal; a quote inside it closes, escapes and reopens them.
    let quoted = program.to_string_lossy().replace('\'', r"'\''");
    format!(
        "#!/bin/sh\n# Aurora: sudo's password dialog, without its warnings in the terminal (cli/src/askpass.rs).\n\
         exec '{quoted}' \"$@\" 2> /dev/null\n"
    )
}

/// Writes the wrapper of `program` into `folder` and returns its path.
///
/// # Errors
/// When the wrapper cannot be written or made executable.
pub fn write(program: &Path, folder: &Path) -> Result<PathBuf> {
    fs::create_dir_all(folder).with_context(|| format!("creating {}", folder.display()))?;
    let wrapper = folder.join("askpass");
    fs::write(&wrapper, script(program)).with_context(|| format!("writing {}", wrapper.display()))?;
    fs::set_permissions(&wrapper, fs::Permissions::from_mode(0o755))
        .with_context(|| format!("making {} executable", wrapper.display()))?;
    Ok(wrapper)
}

/// The wrapper of `program` in `folder`, or `program` itself when the wrapper cannot be written; `program` as it is
/// when it already is the wrapper.
#[must_use]
pub fn wrapped(program: PathBuf, folder: &Path) -> PathBuf {
    if program == folder.join("askpass") {
        return program;
    }
    write(&program, folder).unwrap_or(program)
}

#[cfg(test)]
mod tests {
    use std::process::Command;

    use super::*;

    fn temp(name: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("aurora-askpass-{name}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).expect("create the folder");
        dir
    }

    /// A dialog that answers like ksshaskpass and warns like a Qt app started by sudo.
    fn dialog(path: &Path) {
        fs::write(
            path,
            "#!/bin/sh\necho \"prompt: $1\" >&2\necho 'qt.accessibility.atspi: Error in contacting registry' >&2\n\
             echo secret\n",
        )
        .expect("write the dialog");
        fs::set_permissions(path, fs::Permissions::from_mode(0o755)).expect("make it executable");
    }

    #[test]
    fn the_password_comes_back_and_the_dialog_s_warnings_stay_out_of_the_terminal() {
        let dir = temp("quiet");
        let program = dir.join("it's a dialog");
        dialog(&program);
        let wrapper = write(&program, &dir.join("state")).expect("write the wrapper");
        let out = Command::new(&wrapper).arg("[sudo] password for someone: ").output().expect("run the wrapper");
        assert!(out.status.success());
        assert_eq!(String::from_utf8_lossy(&out.stdout), "secret\n");
        assert_eq!(String::from_utf8_lossy(&out.stderr), "", "nothing reaches the terminal");
        fs::remove_dir_all(dir).expect("clean up");
    }

    #[test]
    fn the_wrapper_is_never_wrapped_again() {
        let dir = temp("again");
        let program = dir.join("dialog");
        dialog(&program);
        let folder = dir.join("state");
        let first = wrapped(program.clone(), &folder);
        assert_eq!(wrapped(first.clone(), &folder), first);
        assert!(fs::read_to_string(&first).expect("read the wrapper").contains(&*program.to_string_lossy()));
        fs::remove_dir_all(dir).expect("clean up");
    }
}
