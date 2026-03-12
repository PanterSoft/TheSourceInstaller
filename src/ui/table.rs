use std::fmt::Display;

pub fn pad_right(s: &str, width: usize) -> String {
    let len = s.chars().count();
    if len >= width {
        s.to_string()
    } else {
        format!("{}{}", s, " ".repeat(width - len))
    }
}

pub fn format_list_table<T: Display, U: Display>(rows: &[(T, U)], name_width: usize) -> String {
    let mut out = String::new();
    for (name, version) in rows {
        out.push_str(&pad_right(&name.to_string(), name_width));
        out.push(' ');
        out.push_str(&version.to_string());
        out.push('\n');
    }
    out
}
