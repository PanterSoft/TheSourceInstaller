use console::{style, Style};
use std::io::{self, Write};

const ARROW: &str = "==>";
const INDENT: &str = "   ";
const OK_MARKER: &str = "[ok]";
const WARN_MARKER: &str = "[!!]";
const ERROR_MARKER: &str = "[!!]";

fn is_tty() -> bool {
    console::Term::stderr().features().is_attended()
}

pub fn section(msg: impl std::fmt::Display) {
    if is_tty() {
        let _ = writeln!(io::stderr(), "{} {}", style(ARROW).bold().blue(), msg);
    } else {
        let _ = writeln!(io::stderr(), "{} {}", ARROW, msg);
    }
}

pub fn detail(msg: impl std::fmt::Display) {
    let _ = writeln!(io::stderr(), "{}{}", INDENT, msg);
}

pub fn success(msg: impl std::fmt::Display) {
    if is_tty() {
        let _ = writeln!(io::stderr(), "{} {}", style(OK_MARKER).green(), msg);
    } else {
        let _ = writeln!(io::stderr(), "{} {}", OK_MARKER, msg);
    }
}

pub fn warning(msg: impl std::fmt::Display) {
    if is_tty() {
        let _ = writeln!(io::stderr(), "{} {}", style(WARN_MARKER).yellow(), msg);
    } else {
        let _ = writeln!(io::stderr(), "{} {}", WARN_MARKER, msg);
    }
}

pub fn error(msg: impl std::fmt::Display) {
    if is_tty() {
        let _ = writeln!(io::stderr(), "{} {}", style(ERROR_MARKER).red(), msg);
    } else {
        let _ = writeln!(io::stderr(), "{} {}", ERROR_MARKER, msg);
    }
}

pub fn info(msg: impl std::fmt::Display) {
    let _ = writeln!(io::stderr(), "{}", msg);
}

pub fn step(msg: impl std::fmt::Display) {
    if is_tty() {
        let _ = writeln!(io::stderr(), "{} {}", style("->").cyan(), msg);
    } else {
        let _ = writeln!(io::stderr(), "-> {}", msg);
    }
}
