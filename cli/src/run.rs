// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Runs one of the theme's scripts and shows its progress: one spinner per step the script announces, the latest
//! line it printed beside it, and the whole output in a log file.

use std::fs::{self, File, OpenOptions};
use std::io::{BufRead, BufReader, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result, bail};
use console::Style;

use crate::steps::{self, Step};

/// How many of the last lines a failure shows.
const TAIL: usize = 14;

/// What one script run did.
#[derive(Debug)]
pub struct Outcome {
    pub log: PathBuf,
    pub steps: Vec<(Step, std::time::Duration)>,
}

/// A step's label: "3/6 Building the parts for Plasma 6.8.2".
fn label(step: &Step) -> String {
    match step.position {
        Some((number, total)) => format!("{number}/{total}  {}", step.title),
        None => step.title.clone(),
    }
}

fn seconds(duration: std::time::Duration) -> String {
    let total = duration.as_secs();
    if total >= 60 { format!("{}m {:02}s", total / 60, total % 60) } else { format!("{total}s") }
}

/// A new log file in `~/.local/state/aurora/logs`.
///
/// # Errors
/// When the folder cannot be made or the file opened.
pub fn log_file(state_home: &Path, what: &str) -> Result<(PathBuf, File)> {
    let folder = state_home.join("aurora/logs");
    fs::create_dir_all(&folder).with_context(|| format!("creating {}", folder.display()))?;
    let stamp = SystemTime::now().duration_since(UNIX_EPOCH).map(|time| time.as_secs()).unwrap_or_default();
    let path = folder.join(format!("{what}-{stamp}.log"));
    // Only the owner can read the log: it lists paths and package names of this account.
    let file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600)
        .open(&path)
        .with_context(|| format!("creating {}", path.display()))?;
    Ok((path, file))
}

/// Runs `program` with `args` (stderr joined to stdout) and draws its steps; `title` labels the spinner until the
/// script announces its first step (a script without steps shows only that one). When it fails, shows its last
/// lines and the log, and returns an error.
///
/// # Errors
/// When the program cannot start or exits with an error.
pub fn script(program: &Path, args: &[&str], env: &[(&str, &Path)], state_home: &Path, title: &str) -> Result<Outcome> {
    let what = title.split_whitespace().next().unwrap_or("run").to_lowercase();
    let (log, mut file) = log_file(state_home, &what)?;
    // `exec "$0" "$@" 2>&1` joins the two streams in the order the script writes them.
    let mut child = Command::new("bash")
        .arg("-c")
        .arg(r#"exec "$0" "$@" 2>&1"#)
        .arg(program)
        .args(args)
        .envs(env.iter().map(|(key, value)| (*key, value.as_os_str())))
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .spawn()
        .with_context(|| format!("starting {}", program.display()))?;
    let output = child.stdout.take().context("reading the script's output")?;

    let dim = Style::new().dim();
    let mut done: Vec<(Step, std::time::Duration)> = Vec::new();
    // The title's spinner stands in until the first step; it is cleared, not listed, when one comes.
    let opening = cliclack::spinner();
    opening.start(title);
    let mut placeholder = Some(opening);
    let mut current: Option<(Step, Instant, cliclack::ProgressBar)> = None;
    let mut tail: Vec<String> = Vec::new();
    for line in BufReader::new(output).lines() {
        let line = line.unwrap_or_default();
        writeln!(file, "{line}").ok();
        if let Some(step) = steps::parse(&line) {
            if let Some(opening) = placeholder.take() {
                opening.clear();
            }
            if let Some((previous, started, spinner)) = current.take() {
                spinner.stop(format!("{}  {}", label(&previous), dim.apply_to(seconds(started.elapsed()))));
                done.push((previous, started.elapsed()));
            }
            if step.position.is_none() {
                continue;
            }
            let spinner = cliclack::spinner();
            spinner.start(label(&step));
            current = Some((step, Instant::now(), spinner));
            continue;
        }
        let plain = steps::strip_ansi(&line);
        if plain.trim().is_empty() {
            continue;
        }
        let shown: String = plain.trim().trim_start_matches("+ ").chars().take(60).collect();
        if let Some((step, _, spinner)) = &current {
            spinner.set_message(format!("{}  {}", label(step), dim.apply_to(shown)));
        } else if let Some(opening) = &placeholder {
            opening.set_message(format!("{title}  {}", dim.apply_to(shown)));
        }
        tail.push(plain);
        if tail.len() > TAIL {
            tail.remove(0);
        }
    }
    let status = child.wait().context("waiting for the script")?;
    if !status.success() {
        if let Some((step, _, spinner)) = current.take() {
            spinner.error(format!("{} failed", label(&step)));
        } else if let Some(opening) = placeholder.take() {
            opening.error(format!("{title} failed"));
        }
        cliclack::note("Last lines", tail.join("\n"))?;
        bail!("{} stopped with {status}; the whole output is in {}", program.display(), log.display());
    }
    if let Some((step, started, spinner)) = current.take() {
        spinner.stop(format!("{}  {}", label(&step), dim.apply_to(seconds(started.elapsed()))));
        done.push((step, started.elapsed()));
    }
    if let Some(opening) = placeholder.take() {
        opening.stop(title);
    }
    Ok(Outcome { log, steps: done })
}

/// Whether `program` is an executable file in one of `PATH`'s folders.
#[must_use]
pub fn on_path(program: &str) -> bool {
    use std::os::unix::fs::PermissionsExt;
    std::env::var_os("PATH").is_some_and(|path| {
        std::env::split_paths(&path).any(|folder| {
            fs::metadata(folder.join(program))
                .is_ok_and(|meta| meta.is_file() && meta.permissions().mode() & 0o111 != 0)
        })
    })
}

/// Runs a short command and returns its standard output, or the error it printed.
///
/// # Errors
/// When the command cannot start or fails.
pub fn capture(program: &str, args: &[&str]) -> Result<String> {
    let output = Command::new(program).args(args).output().with_context(|| format!("starting {program}"))?;
    if !output.status.success() {
        bail!("{program} {}: {}", args.join(" "), String::from_utf8_lossy(&output.stderr).trim());
    }
    Ok(String::from_utf8_lossy(&output.stdout).into_owned())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn executable(path: &Path, text: &str) {
        use std::os::unix::fs::PermissionsExt;
        fs::write(path, text).expect("write the script");
        fs::set_permissions(path, fs::Permissions::from_mode(0o755)).expect("make it executable");
    }

    #[test]
    fn only_its_owner_can_read_a_log() {
        use std::os::unix::fs::PermissionsExt;
        let dir = std::env::temp_dir().join(format!("aurora-cli-log-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        let (path, _file) = log_file(&dir, "test").expect("a log");
        assert_eq!(fs::metadata(&path).expect("the log").permissions().mode() & 0o777, 0o600);
        fs::remove_dir_all(dir).expect("clean up");
    }

    #[test]
    fn a_program_is_found_on_the_path_only_when_it_is_there() {
        assert!(on_path("sh"));
        assert!(!on_path("aurora-no-such-program"));
    }

    #[test]
    fn a_numbered_step_is_labelled_with_its_place() {
        let step = Step { position: Some((3, 6)), title: "Building".into() };
        assert_eq!(label(&step), "3/6  Building");
    }

    #[test]
    fn durations_read_in_seconds_then_minutes() {
        assert_eq!(seconds(std::time::Duration::from_secs(42)), "42s");
        assert_eq!(seconds(std::time::Duration::from_secs(161)), "2m 41s");
    }

    #[test]
    fn a_script_run_reports_its_steps_and_keeps_its_output() {
        let dir = std::env::temp_dir().join(format!("aurora-cli-run-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).expect("create the folder");
        let script = dir.join("steps.sh");
        executable(&script, "#!/bin/bash\necho '==> 1/2 One'\necho working\necho '==> 2/2 Two' >&2\necho '==> Done'\n");
        let outcome = super::script(&script, &[], &[], &dir, "Test run").expect("the script runs");
        let titles: Vec<&str> = outcome.steps.iter().map(|(step, _)| step.title.as_str()).collect();
        assert_eq!(titles, ["One", "Two"]);
        assert!(fs::read_to_string(&outcome.log).expect("the log").contains("working"));
        fs::remove_dir_all(dir).expect("clean up");
    }

    #[test]
    fn a_failing_script_is_an_error_that_names_its_log() {
        let dir = std::env::temp_dir().join(format!("aurora-cli-fail-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(&dir).expect("create the folder");
        let script = dir.join("fails.sh");
        executable(&script, "#!/bin/bash\necho '==> 1/1 Only'\necho 'no space left' >&2\nexit 3\n");
        let error = super::script(&script, &[], &[], &dir, "Test run").expect_err("it fails");
        assert!(error.to_string().contains("aurora/logs/test-"), "{error}");
        fs::remove_dir_all(dir).expect("clean up");
    }
}
