// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
//! Aurora's pacman repository on this PC: the `[aurora]` section of pacman.conf and pacman's trust in Aurora's key,
//! which get.sh sets up. `aurora update` checks both first, without root, and puts back what is missing or wrong with
//! one sudo, so a removed section, an older address or a removed key never keeps a new Aurora away. Without the
//! section pacman also counts the package as one from the AUR, which no helper can update.

use std::fs;
use std::path::Path;
use std::process::Command;

use anyhow::{Context, Result, bail};

/// The address of Aurora's repository: the files of the GitHub release tagged `repo` (get.sh, `SERVER`).
pub const SERVER: &str = "https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme/releases/download/repo";
/// The fingerprint of Aurora's signing key (get.sh, `KEY`; tests/contract.rs keeps the three equal).
pub const KEY: &str = "657D8110799A4E65BE4E1AB4F028284112794CC2";
/// The public half of that key, carried inside the program so a repair needs no download.
pub const KEY_FILE: &str = include_str!("../../packaging/repo/aurora.asc");
/// pacman's configuration file, where get.sh writes the section.
pub const CONF: &str = "/etc/pacman.conf";
/// pacman's keyring; its public keys and trust can be read without root.
const KEYRING: &str = "/etc/pacman.d/gnupg";

/// The `[aurora]` section of pacman.conf.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Section {
    /// It points at Aurora's address.
    Ours,
    /// It is there without a `Server` line.
    NoServer,
    /// It points at another address (an older one, a test server).
    Elsewhere(String),
    /// There is no such section.
    Missing,
}

/// Reads the `[aurora]` section of a pacman.conf's text: the `Server` lines between `[aurora]` and the next section.
#[must_use]
pub fn section(conf: &str) -> Section {
    let mut lines = conf.lines().map(str::trim).skip_while(|line| *line != "[aurora]");
    if lines.next().is_none() {
        return Section::Missing;
    }
    let servers: Vec<&str> = lines
        .take_while(|line| !line.starts_with('['))
        .filter_map(|line| {
            let (key, value) = line.split_once('=')?;
            (key.trim() == "Server").then_some(value.trim())
        })
        .collect();
    if servers.contains(&SERVER) {
        Section::Ours
    } else {
        servers.first().map_or(Section::NoServer, |server| Section::Elsewhere((*server).to_string()))
    }
}

/// pacman's trust in Aurora's key.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Trust {
    /// The key is in the keyring and signed by its owner (`pacman-key --lsign-key`).
    Trusted,
    /// The key is there but not signed, so pacman refuses what it signs.
    Untrusted,
    /// The key is not in the keyring.
    Missing,
    /// The keyring cannot be read: nothing is known, so nothing is changed.
    Unknown,
}

/// Reads `gpg --with-colons --list-keys KEY` output: the validity field of the `pub` line is `f` (full) or `u`
/// (ultimate) for a key pacman trusts (gpg's doc/DETAILS, "Field 2 - Validity").
#[must_use]
pub fn trust(colons: &str) -> Trust {
    match colons.lines().find(|line| line.starts_with("pub:")).and_then(|line| line.split(':').nth(1)) {
        Some("f" | "u") => Trust::Trusted,
        Some(_) => Trust::Untrusted,
        None => Trust::Missing,
    }
}

/// The commands that put the section and the key right, each run through sudo; none when both are right already.
/// `key_file` is where [`KEY_FILE`] was written.
#[must_use]
pub fn repairs(section: &Section, trust: Trust, key_file: &Path) -> Vec<Vec<String>> {
    let mut commands: Vec<Vec<String>> = Vec::new();
    let sudo = |words: &[&str]| words.iter().map(|word| (*word).to_string()).collect::<Vec<_>>();
    match section {
        Section::Ours => {}
        Section::Missing => commands.push(sudo(&[
            "sudo",
            "sh",
            "-c",
            &format!("printf '\\n[aurora]\\nServer = %s\\n' '{SERVER}' >> {CONF}"),
        ])),
        Section::NoServer => {
            commands.push(sudo(&["sudo", "sed", "-i", &format!("/^\\[aurora\\]$/a Server = {SERVER}"), CONF]));
        }
        Section::Elsewhere(_) => commands.push(sudo(&[
            "sudo",
            "sed",
            "-i",
            &format!("/^\\[aurora\\]$/,/^\\[/ s#^Server *=.*#Server = {SERVER}#"),
            CONF,
        ])),
    }
    let key_file = key_file.to_string_lossy();
    if trust == Trust::Missing {
        commands.push(sudo(&["sudo", "pacman-key", "--add", &key_file]));
    }
    if matches!(trust, Trust::Missing | Trust::Untrusted) {
        commands.push(sudo(&["sudo", "pacman-key", "--lsign-key", KEY]));
    }
    commands
}

/// What is wrong, in words, for the line `aurora update` shows before it repairs; `None` when nothing is.
#[must_use]
pub fn problem(section: &Section, trust: Trust) -> Option<String> {
    let section = match section {
        Section::Ours => None,
        Section::Missing => Some("pacman.conf has no [aurora] section".to_string()),
        Section::NoServer => Some("pacman.conf's [aurora] section has no address".to_string()),
        Section::Elsewhere(server) => Some(format!("pacman.conf's [aurora] section points at {server}")),
    };
    let key = match trust {
        Trust::Trusted | Trust::Unknown => None,
        Trust::Untrusted => Some("pacman does not trust Aurora's signing key".to_string()),
        Trust::Missing => Some("pacman does not have Aurora's signing key".to_string()),
    };
    match (section, key) {
        (None, None) => None,
        (Some(one), None) | (None, Some(one)) => Some(one),
        (Some(section), Some(key)) => Some(format!("{section}, and {}", key.replacen("pacman ", "it ", 1))),
    }
}

/// The section of this PC's pacman.conf; a file that cannot be read counts as one without the section.
fn current_section() -> Section {
    section(&fs::read_to_string(CONF).unwrap_or_default())
}

/// pacman's trust in Aurora's key on this PC, read without root.
fn current_trust() -> Trust {
    if fs::File::open(Path::new(KEYRING).join("pubring.gpg")).is_err()
        && fs::File::open(Path::new(KEYRING).join("pubring.kbx")).is_err()
    {
        return Trust::Unknown;
    }
    let output = Command::new("gpg")
        .args(["--homedir", KEYRING, "--no-auto-check-trustdb", "--batch", "--with-colons", "--list-keys", KEY])
        .output();
    output.map_or(Trust::Unknown, |output| trust(&String::from_utf8_lossy(&output.stdout)))
}

/// This PC's repository in a few words, for `aurora status`: set up, or what is wrong and what fixes it.
#[must_use]
pub fn describe() -> String {
    let trust = current_trust();
    match problem(&current_section(), trust) {
        None if trust == Trust::Unknown => "set up (the key's trust cannot be read)".to_string(),
        None => "set up, Aurora's key trusted".to_string(),
        Some(problem) => format!("{problem}: `aurora update` sets it up"),
    }
}

/// Checks the section and the key and repairs what is wrong, after saying what and asking (unless `yes`). `scratch`
/// is a folder for the key file. Returns whether the repository is set up as Aurora's now.
///
/// # Errors
/// When a repair command fails.
pub fn ensure(scratch: &Path, yes: bool) -> Result<bool> {
    let (section, trust) = (current_section(), current_trust());
    let Some(problem) = problem(&section, trust) else {
        return Ok(true);
    };
    cliclack::log::warning(format!("{problem}, so pacman would not get Aurora's updates"))?;
    if !yes
        && !cliclack::confirm("Set up Aurora's repository the way get.sh does? (asks for your password once)")
            .initial_value(true)
            .interact()?
    {
        return Ok(false);
    }
    fs::create_dir_all(scratch).with_context(|| format!("creating {}", scratch.display()))?;
    let key_file = scratch.join("aurora.asc");
    fs::write(&key_file, KEY_FILE).with_context(|| format!("writing {}", key_file.display()))?;
    for command in repairs(&section, trust, &key_file) {
        let shown = command.join(" ");
        let status =
            Command::new(&command[0]).args(&command[1..]).status().with_context(|| format!("starting {shown}"))?;
        if !status.success() {
            bail!("`{shown}` failed, so Aurora's repository is not set up");
        }
    }
    cliclack::log::success("Aurora's repository is set up")?;
    Ok(true)
}

#[cfg(test)]
mod tests {
    use super::*;

    const ARCH: &str = "[options]\nArchitecture = auto\n\n[core]\nInclude = /etc/pacman.d/mirrorlist\n";

    #[test]
    fn a_section_with_aurora_s_address_is_ours() {
        let conf = format!("{ARCH}\n[aurora]\nServer = {SERVER}\n");
        assert_eq!(section(&conf), Section::Ours);
        // Another server first, a comment and spaces around `=` do not hide it.
        let conf = format!("{ARCH}\n[aurora]\n# mirror\nServer=https://example.org/old\nServer   =   {SERVER}\n");
        assert_eq!(section(&conf), Section::Ours);
    }

    #[test]
    fn a_missing_section_an_empty_one_and_another_address_are_told_apart() {
        assert_eq!(section(ARCH), Section::Missing);
        assert_eq!(section(&format!("{ARCH}\n[aurora]\n\n[extra]\nServer = {SERVER}\n")), Section::NoServer);
        assert_eq!(
            section(&format!("{ARCH}\n[aurora]\nServer = https://example.org/old\n")),
            Section::Elsewhere("https://example.org/old".to_string())
        );
    }

    #[test]
    fn the_validity_field_says_whether_pacman_trusts_the_key() {
        // Recorded from this PC's keyring (gpg 2.4, 2026-11-08), trimmed to the pub line.
        let trusted =
            "tru::1:1794114780:0:3:1:5\npub:f:255:22:F028284112794CC2:1793854158:::-:::scSC:::::ed25519:::0:\n";
        assert_eq!(trust(trusted), Trust::Trusted);
        assert_eq!(trust(&trusted.replace("pub:f:", "pub:-:")), Trust::Untrusted);
        assert_eq!(trust(&trusted.replace("pub:f:", "pub:u:")), Trust::Trusted);
        assert_eq!(trust(""), Trust::Missing);
    }

    #[test]
    fn nothing_is_repaired_when_everything_is_right() {
        assert_eq!(repairs(&Section::Ours, Trust::Trusted, Path::new("/k")), Vec::<Vec<String>>::new());
        assert_eq!(repairs(&Section::Ours, Trust::Unknown, Path::new("/k")), Vec::<Vec<String>>::new());
        assert_eq!(problem(&Section::Ours, Trust::Trusted), None);
    }

    #[test]
    fn each_problem_gets_its_own_repair_through_sudo() {
        let all = repairs(&Section::Missing, Trust::Missing, Path::new("/tmp/aurora.asc"));
        assert_eq!(all.len(), 3);
        assert!(all.iter().all(|command| command[0] == "sudo"));
        assert!(all[0].join(" ").contains(&format!("[aurora]\\nServer = %s\\n' '{SERVER}' >> /etc/pacman.conf")));
        assert_eq!(all[1], ["sudo", "pacman-key", "--add", "/tmp/aurora.asc"]);
        assert_eq!(all[2], ["sudo", "pacman-key", "--lsign-key", KEY]);
        let untrusted = repairs(&Section::Ours, Trust::Untrusted, Path::new("/k"));
        assert_eq!(untrusted, [["sudo", "pacman-key", "--lsign-key", KEY]]);
        let moved = repairs(&Section::Elsewhere("https://example.org/old".into()), Trust::Trusted, Path::new("/k"));
        assert_eq!(moved.len(), 1);
        assert!(moved[0].join(" ").contains("s#^Server *=.*#Server = https://github.com/"));
    }

    #[test]
    fn the_repairs_leave_a_section_this_module_reads_as_ours() {
        // The sed and printf commands run on a copy of a pacman.conf, each starting from its own broken section.
        let folder = std::env::temp_dir().join(format!("aurora-repo-test-{}", std::process::id()));
        fs::create_dir_all(&folder).expect("a scratch folder");
        let cases = [
            (Section::Missing, ARCH.to_string()),
            (Section::NoServer, format!("{ARCH}\n[aurora]\n\n[extra]\nInclude = /etc/pacman.d/mirrorlist\n")),
            (
                Section::Elsewhere("https://example.org/old".into()),
                format!("{ARCH}\n[aurora]\nServer = https://example.org/old\n\n[extra]\nServer = https://x.org\n"),
            ),
        ];
        for (index, (state, text)) in cases.into_iter().enumerate() {
            let conf = folder.join(format!("pacman-{index}.conf"));
            fs::write(&conf, &text).expect("a pacman.conf");
            for command in repairs(&state, Trust::Trusted, Path::new("/k")) {
                let words: Vec<String> =
                    command[1..].iter().map(|word| word.replace(CONF, &conf.to_string_lossy())).collect();
                let status = Command::new(&words[0]).args(&words[1..]).status().expect("the repair runs");
                assert!(status.success());
            }
            let repaired = fs::read_to_string(&conf).expect("the repaired file");
            assert_eq!(section(&repaired), Section::Ours, "{state:?} gave:\n{repaired}");
            // The section after it keeps its own server.
            if text.contains("https://x.org") {
                assert!(repaired.contains("[extra]\nServer = https://x.org"));
            }
        }
        let _ = fs::remove_dir_all(&folder);
    }

    #[test]
    fn the_problem_reads_as_one_sentence() {
        assert_eq!(
            problem(&Section::Missing, Trust::Missing).as_deref(),
            Some("pacman.conf has no [aurora] section, and it does not have Aurora's signing key")
        );
        assert_eq!(
            problem(&Section::Ours, Trust::Untrusted).as_deref(),
            Some("pacman does not trust Aurora's signing key")
        );
    }
}
